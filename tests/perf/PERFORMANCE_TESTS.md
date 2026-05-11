# Six Sines DSP Performance Test Plan

## Context

Before executing PERFORMANCE.md's optimizations, build a repeatable
throughput benchmark so each change can be measured before/after. The
existing test target `six-sines-test` (Catch2, `tests/`) is unit-style; it
has no timing infrastructure. We want to add it without entangling the
regression tests, and without forcing a heavy framework dependency —
Catch2 v2 already supports `BENCHMARK` blocks under
`CATCH_CONFIG_ENABLE_BENCHMARKING`, which is the lowest-friction option.

Goals:

- One number per scenario, stable enough to detect ~5% changes.
- Multiple scenarios that isolate the moving parts (poly, matrix density,
  feedback, modulation, extended mode).
- Output that diffs cleanly between commits.
- No impact on the existing unit-test target — perf is opt-in.

---

## Three levels of granularity

Pick the smallest that answers the question; have all three available.

1. **Whole-plugin** — drive `Synth::process` (or `processInternal<false>`)
   end-to-end including end-of-chain (saturator, LP/HP, decimator, SRC).
   This is "what the user feels." Use it for sanity, not for attribution.
2. **Voice-level** — call `Voice::renderBlock` directly in a tight loop on
   a pre-attacked voice. Isolates the operator + matrix + mixer + output
   work without the SRC + filter tail. This is the right level for
   measuring PERFORMANCE.md items 1–11.
3. **Operator/inner-loop** — call `OpSource::renderBlock` (or, for #4
   specifically, `OpSource::innerLoopImpl<...>`) directly. Useful for
   tuning the inner loop alone — particularly for #2, #3, #4.

All three live in one target, gated by Catch2 tags
(`[bench][plugin]`, `[bench][voice]`, `[bench][inner]`) so they can be
run selectively.

---

## Scenarios

Ten scenarios, picked to vary one thing at a time, plus a worst-case.
Each is a tag on a Catch2 `BENCHMARK` so they can be filtered.

| Tag | Voices | Active ops | Matrix | Self-FB | Mod | Extended | Purpose |
|-----|--------|-----------|--------|---------|-----|----------|---------|
| `[scn:minimal]` | 1 | 1 | none | none | none | NONE | Floor cost of the engine |
| `[scn:1v_dense]` | 1 | 6 | all 15 | all 6 | full | NONE | Single-voice ceiling, EM::NONE |
| `[scn:8v_dense]` | 8 | 6 | all 15 | all 6 | full | NONE | Typical poly load |
| `[scn:32v_dense]` | 32 | 6 | all 15 | all 6 | full | NONE | Heavy poly |
| `[scn:64v_dense]` | 64 | 6 | all 15 | all 6 | full | NONE | Max poly |
| `[scn:em_phaseremap]` | 16 | 6 | all 15 | none | full | PHASE_REMAP | Extended mode cost |
| `[scn:em_resonant]` | 16 | 6 | all 15 | none | full | RESONANT_SWEEP | Extended mode cost |
| `[scn:em_noise]` | 16 | 6 | all 15 | none | full | NOISE | Extended mode cost |
| `[scn:no_fb_simd]` | 16 | 6 | all 15 | **none** | full | NONE | Baseline for #4 (SIMD no-FB) |
| `[scn:worst]` | 64 | 6 | all 15 | all 6 | full | NOISE | Worst-case ceiling |

Workload knobs (varied between scenarios but constant within one):

- **Active ops**: gated by `Patch::SourceNode::active`. "1 active" = op 0
  only; "6 active" = all ops.
- **Matrix density**: 0 or 15 of the unique src→tgt pairs active with
  `level > 0` and `modulationMode == LINEAR_FM` (the default cost).
- **Self-FB**: `Patch::SelfNode::active` + non-zero `fbLevel`.
- **Mod**: every node's 3 mod slots populated with a non-trivial source +
  depth. Sources: a CC, an internal LFO, a macro — mix to exercise
  different `rebindPointer` paths.
- **Extended**: `Patch::SourceNode::extendedModeMode` per scenario; sub-
  enums (window, depth, noise mode/type/lfsr) set to a deterministic
  default per mode.

Programmatically constructed in a helper (see Harness, below). No factory
patches involved — we want determinism and easy variant generation.

---

## Harness

### Build target

New executable `six-sines-perf` in `tests/CMakeLists.txt`, separate from
`six-sines-test`:

```cmake
add_executable(six-sines-perf
    perf_main.cpp
    perf_scenarios.cpp
)
target_link_libraries(six-sines-perf six-sines-impl catch2 six-sines-patches fmt)
target_compile_definitions(six-sines-perf PRIVATE CATCH_CONFIG_ENABLE_BENCHMARKING)
```

Built only when explicitly requested (`cmake --build … --target six-sines-perf`).
Not in `six-sines_standalone` deps, not in the default `all` target unless
we tag it. Keeps CI / day-to-day builds untouched.

### Patch construction helper

A free function in `tests/perf_scenarios.cpp`:

```cpp
struct ScenarioSpec {
    int activeOps;
    bool fullMatrix;
    bool allSelfFB;
    bool fullMod;
    Patch::SourceNode::ExtendedMode em;
};
std::unique_ptr<Patch> makeScenarioPatch(const ScenarioSpec &);
```

Builds a `Patch` directly via the param struct accessors (same pattern
`patch.h` ctors use). No JSON. Deterministic and trivial to mutate.

### Driving the engine

Two driver functions:

```cpp
// Whole-plugin: brings up makePlugin(), feeds CLAP events, runs process().
double runPluginScenario(const ScenarioSpec &, int numVoices, int blocks);
// Voice-level: constructs Synth + Patch directly, attacks N voices on the
// VoiceManager, then loops Voice::renderBlock blocks times.
double runVoiceScenario(const ScenarioSpec &, int numVoices, int blocks);
```

Both return wall-clock seconds for the `blocks` iterations. Catch2's
`BENCHMARK` calls them inside its iteration/sampling logic.

For #4 specifically, an additional `runInnerLoopScenario(EM, blocks)` that
hits `OpSource::renderBlock` directly on a single op. Lower level; only
needed for micro tuning.

### Catch2 benchmark form

```cpp
TEST_CASE("dense poly throughput", "[bench][voice][scn:8v_dense]") {
    auto spec = ScenarioSpec{ /* 6 ops, full matrix, full FB, full mod, NONE */ };
    BENCHMARK("voice render — 8v dense") {
        return runVoiceScenario(spec, 8, /*blocks=*/512);
    };
}
```

Catch2 handles warmup, sample counts, mean/stddev. Default output is a
table per run — readable, and there is also `--reporter xml` /
`--reporter sonarqube` / `--reporter json` (v3 has more; v2 single-header
has basic). Worst case, capture stdout and parse.

### Numbers we want to print

For each scenario, in addition to Catch2's wall-clock:

- **ns / block** (raw)
- **ns / sample** at the host rate (block ÷ blockSize)
- **CPU%** at 48 kHz host (samples/sec produced ÷ samples/sec needed × 100)
- **Voices × ops / second** throughput

A simple post-print in each scenario writes those four numbers to stdout
in a single line, prefixed with the tag, so `grep [scn:` collects a CSV-
ish digest:

```
[scn:8v_dense] block_ns=12480 sample_ns=1560 cpu_pct_48k=7.5 vops_per_s=3.07e8
```

That digest is what gets diffed across commits.

---

## Stability

Bench numbers are useless if they jitter 30%. Standard hygiene, in order
of impact:

1. **Release build**, `-O2` minimum, NDEBUG defined. (Verify in
   `CMakeLists.txt`; the `cmake-build-debug` dir is wrong for this.)
   Suggest `cmake -B cmake-build-perf -DCMAKE_BUILD_TYPE=Release -GNinja`.
2. **Warmup**: Catch2 BENCHMARK already runs samples to estimate clock
   resolution + does multiple samples per benchmark. That's enough for
   our purposes; we don't need to roll our own warmup.
3. **High iteration count**: tune `blocks` per scenario so each run is
   ≥ ~10 ms. For a heavy scenario at < 0.1 ms/block, 256–1024 blocks is
   fine. Set per-scenario, not global.
4. **Disable background load**: documentation note — close DAW, browsers,
   etc. Not enforced.
5. **CPU pinning** (optional): on macOS we can't pin easily; on Linux,
   `taskset -c 0`. Document but don't require.
6. **Avoid frequency scaling**: `sudo cpupower frequency-set -g performance`
   on Linux. On macOS document only. Not enforced.
7. **Repeat 3 runs, take median**: simple shell wrapper. The diff target
   is "did this commit move the median by ≥ 5%?", not "what is the exact
   number?".

Expected noise floor with the above: 2–4% run-to-run on a quiet laptop.

---

## Comparison workflow

Two scripts (shell, in `tests/perf/`):

- `tests/perf/run.sh` — runs `six-sines-perf`, greps the `[scn:…]` digest
  lines, writes to `tests/perf/results/<branchname>-<timestamp>.csv`.
- `tests/perf/diff.sh <before.csv> <after.csv>` — joins on scenario tag,
  prints per-scenario `% delta`, highlights anything outside ±5%.

That's it. No DB, no CI integration on first pass. If we later want
historical tracking, the CSVs are already in a diffable format.

---

## Sanity checks (build into the harness)

- Each scenario hashes its rendered audio buffer (just a `std::accumulate`
  on the floats × prime, or a real `xxhash` if convenient) and prints
  it alongside the timing. If the hash changes when no DSP code changed,
  something nondeterministic crept in (RNG seeding, uninitialized state).
- A `--check-only` flag runs each scenario once and asserts the buffer
  is non-zero and non-NaN. Cheap pre-flight before the timing run.

---

## What we explicitly skip (for now)

- **Cycle counters / `rdtsc`** — wall-clock is fine for relative deltas.
  If a specific item needs cycle attribution, use Instruments / `perf`
  separately on the standalone, not in the bench.
- **Per-function attribution** — same reason. The bench tells us *what*
  moved; the profiler tells us *why*.
- **Cross-platform CI runs** — make the bench runnable on macOS first
  (the dev env); Linux/Windows can follow if useful.
- **Memory / allocation tracking** — separate concern. The hot path
  shouldn't allocate; verify with a sanitizer build if curious.

---

## Critical files to add

- `tests/CMakeLists.txt` — append `six-sines-perf` target.
- `tests/perf_main.cpp` — Catch2 main with benchmarking macro defined.
- `tests/perf_scenarios.cpp` — `ScenarioSpec`, `makeScenarioPatch`, the
  three runner functions, and one `TEST_CASE` per scenario.
- `tests/perf/run.sh`, `tests/perf/diff.sh` — wrappers.
- (optional) `tests/perf/results/.gitkeep` — keep the results dir tracked
  but ignore its CSV contents via `.gitignore`.

---

## Verification (of the harness itself, before using it for real)

1. Build `six-sines-perf` in Release. Run `[scn:minimal]`. Confirm:
   audio hash is non-zero, timing is plausible (sub-microsecond per
   sample on a modern Mac).
2. Run `[scn:64v_dense]`. Confirm: CPU% printout is in a sensible range
   (5–40% at 48 kHz on dev hardware, depending on machine).
3. Run the full suite three times back-to-back. Confirm: median CSV
   deltas across the three runs are < 5%. If they aren't, raise `blocks`
   in the noisy scenario and re-check.
4. Make a deliberately bad change (e.g. add a `std::sin(rand())` in the
   inner loop) and confirm the bench notices, by a wide margin, in the
   right scenario.

---

## Sequencing into the perf work

1. Land the harness on `main` first, with all benchmarks passing (i.e.
   producing a baseline CSV).
2. Save that CSV to `tests/perf/results/baseline-<commit>.csv` and
   commit it as the reference.
3. Then start the PERFORMANCE.md chunks. Each chunk's PR includes its
   before/after CSV diff in the description.

This is exactly what makes the PERFORMANCE.md items defensible — every
item gets a number attached to it.
