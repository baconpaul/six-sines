/*
 * Performance benchmark scenarios for the Six Sines DSP/voice path.
 *
 * Each TEST_CASE configures a Synth + Patch directly in code (no JSON), fires
 * the noteOns it needs, and times one of three drivers:
 *
 *   runPluginScenario  — synth.process(nullptr): full pipeline including SRC
 *                        and end-of-chain. "What the user feels."
 *   runVoiceScenario   — direct cvoice->renderBlock loop. Skips SRC / filter
 *                        tail. Engine-rate blocks.
 *   runInnerLoopScenario — one OpSource's renderBlock in isolation, useful
 *                        for tuning the inner loop alone (#2/#3/#4).
 *
 * Per-scenario one-line digest is printed via perf_timing.h:printDigest.
 * `tests/perf/diff.sh` greps those lines and joins on the [scn:...] tag.
 *
 * Programmatic patch construction here is deliberately minimal: we set only
 * the params required to make the desired DSP path active. Everything else
 * uses the Patch ctor defaults.
 */

#include "catch2/catch2.hpp"
#include "perf_timing.h"

#include "configuration.h"
#include "synth/patch.h"
#include "synth/synth.h"
#include "synth/voice.h"
#include "synth/mod_matrix.h"
#include "dsp/op_source.h"
#include "dsp/sintable.h"
#include "dsp/matrix_node.h"

#include <memory>
#include <string>
#include <vector>

using namespace baconpaul::six_sines;
using namespace baconpaul::six_sines::perf;

namespace
{

// ---------------------------------------------------------------------------
// ScenarioSpec — the workload knobs the plan calls out. One spec per row in
// the PERFORMANCE_TESTS.md scenarios table.
// ---------------------------------------------------------------------------
struct ScenarioSpec
{
    int activeOps{1};       // ops 0..activeOps-1 will have active=1
    bool fullMatrix{false}; // all 15 matrix nodes active
    bool allSelfFB{false};  // all 6 self-feedback nodes active
    bool fullMod{false};    // 1 mod slot populated on every node
    Patch::SourceNode::ExtendedMode em{Patch::SourceNode::ExtendedMode::NONE};
};

// ---------------------------------------------------------------------------
// Helpers that walk the Patch sub-structures and set fields. Kept terse —
// we read .value directly, which is what Synth itself does at runtime.
// ---------------------------------------------------------------------------

// Default-good envelope so notes don't decay during a benchmark run:
// instant attack, full sustain. envIsMult=1 means env multiplies the level.
void setFastSustainedEnv(Patch::DAHDSRMixin &e)
{
    e.delay.value = 0.f;
    e.attack.value = 0.01f;
    e.hold.value = 0.f;
    e.decay.value = 0.f;
    e.sustain.value = 1.f;
    e.release.value = 0.5f;
    e.envPower.value = 1.f;
    e.envIsMultiplcative.value = 1.f;
    e.envIsOneShot.value = 0.f;
    e.envTriggersFromZero.value = 0.f;
    // triggerMode default (Key Press = 3) is fine.
}

// Active sine LFO at a moderate rate. Keeps INTERNAL_LFO source pointers
// pointing at something that actually moves.
void setActiveLFO(Patch::LFOMixin &l)
{
    l.lfoActive.value = 1.f;
    l.lfoRate.value = 0.3f; // moderate
    l.lfoShape.value = 0.f; // Sine — no smoothing branch
    l.lfoDeform.value = 0.f;
    l.tempoSync.value = 0.f;
    l.lfoBipolar.value = 1.f;
    l.lfoIsEnveloped.value = 0.f;
    l.lfoStartPhase.value = 0.f;
}

// Populate slot 0 of a node's modulation triple. We use DIRECT (target id 10
// on every node type) so the same write applies uniformly. MACRO_0 is a
// cheap, always-valid source.
//
// `targetDirect` is the node-specific TargetID::DIRECT (always 10 in this
// codebase, but we pass it explicitly so accidental drift is caught).
template <typename Node> void wireOneMod(Node &n, int32_t source, int32_t targetDirect, float depth)
{
    n.modsource[0].value = (float)source;
    n.moddepth[0].value = depth;
    n.modtarget[0].value = (float)targetDirect;
}

// Set ratio to a non-trivial value so the inner loop walks the wavetable
// rather than sitting at phase=0 for every op identically.
void setOpDefaults(Patch::SourceNode &s, int idx)
{
    s.ratio.value = 0.0f + 0.13f * idx; // 1.0, ~1.094, ~1.198, ... in 2^x
    s.waveForm.value = (float)SinTable::SIN;
    s.envToRatio.value = 0.f;
    s.envToRatioFine.value = 0.f;
    s.lfoToRatio.value = 0.f;
    s.lfoToRatioFine.value = 0.f;
    s.startingPhase.value = 0.f;
    s.octTranspose.value = 0.f;
    s.keyTrack.value = 1.f;
    s.unisonParticipation.value = 3.f; // pan+tune
    s.unisonToMain.value = 0.f;
    s.unisonToOpOut.value = 0.f;
    setFastSustainedEnv(s);
    setActiveLFO(s);
}

void configureScenarioPatch(Patch &patch, const ScenarioSpec &spec)
{
    // ---- Output ----
    patch.output.level.value = 0.5f;
    patch.output.velSensitivity.value = 0.f;
    patch.output.playMode.value = 0.f; // poly
    patch.output.polyLimit.value = (float)maxVoices;
    patch.output.unisonCount.value = 1.f;
    patch.output.pianoModeActive.value = 0.f;
    patch.output.octTranspose.value = 0.f;
    patch.output.fineTune.value = 0.f;
    patch.output.pan.value = 0.f;
    patch.output.lfoDepth.value = 0.f;
    setFastSustainedEnv(patch.output);
    setActiveLFO(patch.output);

    // Run all nodes with active=1 by default — designModeRunAll is false in
    // these benchmarks; we toggle .active per node explicitly below.

    // ---- Source nodes (operators) ----
    for (int i = 0; i < (int)numOps; ++i)
    {
        auto &s = patch.sourceNodes[i];
        setOpDefaults(s, i);
        s.active.value = (i < spec.activeOps) ? 1.f : 0.f;

        // Extended mode (per-op): only configure when needed; otherwise leave NONE.
        s.extendedModeMode.value = (float)spec.em;
        if (spec.em == Patch::SourceNode::ExtendedMode::PHASE_REMAP)
        {
            s.phaseMapModeShape.value = (float)Patch::SourceNode::PhaseMapShape::SAW;
            s.extendedModeM.value = 0.5f;
        }
        else if (spec.em == Patch::SourceNode::ExtendedMode::RESONANT_SWEEP)
        {
            s.resonantSweepWindowShape.value = (float)Patch::SourceNode::ResonantSweepWindow::HANN;
            s.resonantSweepFrequencyDepth.value =
                (float)Patch::SourceNode::ResonantSweepFrequencyDepth::FOUR;
            s.extendedModeM.value = 0.5f;
        }
        else if (spec.em == Patch::SourceNode::ExtendedMode::NOISE)
        {
            s.noiseMode.value = (float)Patch::SourceNode::NoiseMode::ADD_TO_SIGNAL;
            s.noiseType.value = (float)Patch::SourceNode::NoiseType::PINK;
            s.lfsrMode.value = (float)Patch::SourceNode::LFSRMode::LONG_KEYTRACK;
            s.extendedModeM.value = 0.3f;
            s.extendedModeN.value = 0.5f;
        }

        if (spec.fullMod)
        {
            wireOneMod(s, ModMatrixConfig::Source::MACRO_0, Patch::SourceNode::TargetID::DIRECT,
                       0.1f);
        }
    }

    // ---- Mixer nodes (must be active to produce output) ----
    for (int i = 0; i < (int)numOps; ++i)
    {
        auto &m = patch.mixerNodes[i];
        m.active.value = (i < spec.activeOps) ? 1.f : 0.f;
        m.level.value = 0.5f;
        m.pan.value = 0.f;
        m.lfoToLevel.value = 0.f;
        m.lfoToPan.value = 0.f;
        m.envToLevel.value = 0.f;
        m.solo.value = 0.f;
        setFastSustainedEnv(m);
        setActiveLFO(m);
        if (spec.fullMod)
        {
            wireOneMod(m, ModMatrixConfig::Source::MACRO_0, Patch::MixerNode::TargetID::DIRECT,
                       0.1f);
        }
    }

    // ---- Self-feedback nodes ----
    for (int i = 0; i < (int)numOps; ++i)
    {
        auto &sn = patch.selfNodes[i];
        sn.active.value = (spec.allSelfFB && i < spec.activeOps) ? 1.f : 0.f;
        sn.fbLevel.value = 0.25f;
        sn.lfoToFB.value = 0.f;
        sn.envToFB.value = 0.f;
        sn.overdrive.value = 0.f;
        setFastSustainedEnv(sn);
        setActiveLFO(sn);
        if (spec.fullMod)
        {
            wireOneMod(sn, ModMatrixConfig::Source::MACRO_0, Patch::SelfNode::TargetID::DIRECT,
                       0.1f);
        }
    }

    // ---- Matrix nodes (15 src→tgt with src<tgt) ----
    for (int i = 0; i < (int)matrixSize; ++i)
    {
        auto &mx = patch.matrixNodes[i];
        // Activate only matrix nodes whose source AND target are within
        // activeOps. Otherwise we waste cycles routing from a silent op.
        int srcOp = MatrixIndex::sourceIndexAt(i);
        int tgtOp = MatrixIndex::targetIndexAt(i);
        bool inRange = (srcOp < spec.activeOps) && (tgtOp < spec.activeOps);
        mx.active.value = (spec.fullMatrix && inRange) ? 1.f : 0.f;
        mx.level.value = 0.2f;
        mx.modulationMode.value = 2.f; // Linear FM (the typical hot path)
        mx.modulationScale.value = 0.f;
        mx.lfoToDepth.value = 0.f;
        mx.envToLevel.value = 0.f;
        mx.overdrive.value = 0.f;
        setFastSustainedEnv(mx);
        setActiveLFO(mx);
        if (spec.fullMod)
        {
            wireOneMod(mx, ModMatrixConfig::Source::MACRO_0, Patch::MatrixNode::TargetID::DIRECT,
                       0.1f);
        }
    }

    // ---- Macro nodes (always present; we want at least MACRO_0 sane) ----
    for (int i = 0; i < (int)numMacros; ++i)
    {
        auto &mn = patch.macroNodes[i];
        mn.level.value = 0.5f;
        mn.macroPower.value = 0.f; // off — voiceValues.macroOut falls back to mono macroPtr
    }

    // ---- Output mod nodes (panMod, fineTuneMod) — leave default-off ----
    // Patch ctor already constructs fineTuneMod and mainPanMod with sane defaults.
}

// ---------------------------------------------------------------------------
// Synth setup helpers — bring up a Synth, configure the patch, trigger notes.
// ---------------------------------------------------------------------------

// Constructing a Synth lazily ensures SinTable statics get initialized via
// OpSource ctor before any other DSP code runs.
std::unique_ptr<Synth> bringUpSynth(const ScenarioSpec &spec, int numVoices,
                                    double hostSampleRate = 48000.0)
{
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(hostSampleRate);
    configureScenarioPatch(s->patch, spec);
    // reapplyControlSettings is public and re-reads playMode/polyLimit/MPE etc
    // from the patch we just configured.
    s->reapplyControlSettings();
    // Trigger notes — one per voice, spread across keys so the engine isn't
    // accidentally rendering identical phase trajectories per voice.
    int baseKey = 36;
    for (int v = 0; v < numVoices; ++v)
    {
        s->voiceManager->processNoteOnEvent(0, 0, baseKey + (v % 60), -1, 0.8f, 0.f);
    }
    // Let envelopes step out of the attack stage so timings reflect steady state.
    // Three host blocks ≈ 0.5 ms at 48 kHz; far less than 10 ms minAttack but our
    // attack is set near zero, so this is sufficient.
    for (int i = 0; i < 8; ++i)
        s->process(nullptr);
    return s;
}

// ---------------------------------------------------------------------------
// Drivers — return a callable suitable for timeIt().
// ---------------------------------------------------------------------------

// Plugin-level: full Synth::process() pipeline. samplesPerBlock = blockSize
// at the host rate.
auto makePluginDriver(Synth &s)
{
    return [&s]() { s.process(nullptr); };
}

// Voice-level: skip SRC and filter tail. Replicates only the pre-voice setup
// `processInternal` does so renderBlock sees the same monoValues state.
// samplesPerBlock = blockSize at the *engine* rate.
auto makeVoiceDriver(Synth &s)
{
    return [&s]()
    {
        // Pre-voice setup that voice render relies on.
        s.monoValues.attackFloorOnRetrig = s.patch.output.attackFloorOnRetrig > 0.5;
        s.monoValues.unisonSpreadFactorMinus1 =
            s.monoValues.twoToTheX.twoToThe(s.patch.output.unisonSpread.value) - 1.f;
        s.monoValues.unisonPanScalar = s.patch.output.unisonPan.value;

        auto *cv = s.head;
        while (cv)
        {
            cv->renderBlock();
            cv = cv->next;
        }
    };
}

// Inner-loop level: one OpSource on a held voice, no matrix/mixer/output.
// samplesPerBlock = blockSize at the engine rate.
auto makeInnerDriver(Synth &s)
{
    auto *cv = s.head;
    REQUIRE(cv != nullptr);
    auto *op = &cv->src[0];
    return [op, cv]()
    {
        // zeroInputs / clearOutputs each block so we don't accumulate drift.
        op->zeroInputs();
        op->renderBlock();
        // Touch the voice so the compiler doesn't optimize the op away.
        (void)cv;
    };
}

// ---------------------------------------------------------------------------
// Output buffer hash — runs one block via process() and hashes the stereo
// output. Used as a sanity signature alongside the timing line.
// ---------------------------------------------------------------------------
uint64_t hashOneOutputBlock(Synth &s)
{
    s.process(nullptr);
    // Synth::output is [2*(1+numOps)][blockSize] — main pair is rows 0/1.
    uint64_t h = hashFloats(s.output[0], blockSize);
    h ^= hashFloats(s.output[1], blockSize) * 0x9E3779B97F4A7C15ull;
    return h;
}

// ---------------------------------------------------------------------------
// Per-scenario shared driver: build synth, time, print digest.
// ---------------------------------------------------------------------------

enum class Level
{
    Plugin,
    Voice,
    Inner
};
const char *levelName(Level l)
{
    switch (l)
    {
    case Level::Plugin:
        return "plugin";
    case Level::Voice:
        return "voice";
    case Level::Inner:
        return "inner";
    }
    return "?";
}

// Auto-calibrated timing: each scenario fills `target_sample_ms` of
// wall-clock per timed sample (default 30 ms → 17 scenarios × 15 samples ×
// ~30 ms ≈ 8 s total). Overridable per-scenario, and globally via
// PERF_SAMPLE_MS env var.
struct RunOptions
{
    int samples{15};
    int warmup{3};
    double target_sample_ms{100.0};
};

void runScenario(const char *tag, Level level, const ScenarioSpec &spec, int numVoices,
                 RunOptions opts = {})
{
    auto synth = bringUpSynth(spec, numVoices);

    uint64_t hash = hashOneOutputBlock(*synth);

    BenchResult r;
    switch (level)
    {
    case Level::Plugin:
        r = timeIt(opts.samples, opts.warmup, opts.target_sample_ms, makePluginDriver(*synth));
        break;
    case Level::Voice:
        r = timeIt(opts.samples, opts.warmup, opts.target_sample_ms, makeVoiceDriver(*synth));
        break;
    case Level::Inner:
        r = timeIt(opts.samples, opts.warmup, opts.target_sample_ms, makeInnerDriver(*synth));
        break;
    }

    DigestParams d{};
    d.tag = tag;
    d.level = levelName(level);
    d.voices = numVoices;
    d.activeOps = spec.activeOps;
    d.block_ns = r.median_ns_per_iter;
    d.samplesPerBlock = blockSize;
    d.stddev_pct = r.stddev_pct;
    d.iters_per_sample = r.iters_per_sample;
    d.hash = hash;
    printDigest(d);

    // Catch2 sanity: at least confirm we got non-trivial timing and a real hash.
    REQUIRE(r.median_ns_per_iter > 0);
    REQUIRE(hash != 0);
}

} // namespace

// ---------------------------------------------------------------------------
// Test cases. One per scenario row in PERFORMANCE_TESTS.md.
// Run subsets with tag filters:
//   ./six-sines-perf "[bench][plugin]"   all plugin-level
//   ./six-sines-perf "[scn:8v_dense]"    just one
// ---------------------------------------------------------------------------

TEST_CASE("minimal", "[bench][plugin][scn:minimal]")
{
    ScenarioSpec spec{};
    spec.activeOps = 1;
    runScenario("scn:minimal", Level::Plugin, spec, 1);
}

TEST_CASE("1 voice, dense", "[bench][plugin][scn:1v_dense]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = true;
    spec.fullMod = true;
    runScenario("scn:1v_dense", Level::Plugin, spec, 1);
}

TEST_CASE("8 voice, dense", "[bench][plugin][scn:8v_dense]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = true;
    spec.fullMod = true;
    runScenario("scn:8v_dense", Level::Plugin, spec, 8);
}

TEST_CASE("32 voice, dense", "[bench][plugin][scn:32v_dense]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = true;
    spec.fullMod = true;
    runScenario("scn:32v_dense", Level::Plugin, spec, 32);
}

TEST_CASE("64 voice, dense", "[bench][plugin][scn:64v_dense]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = true;
    spec.fullMod = true;
    runScenario("scn:64v_dense", Level::Plugin, spec, 64);
}

TEST_CASE("16 voice, PHASE_REMAP", "[bench][plugin][scn:em_phaseremap]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = false;
    spec.fullMod = true;
    spec.em = Patch::SourceNode::ExtendedMode::PHASE_REMAP;
    runScenario("scn:em_phaseremap", Level::Plugin, spec, 16);
}

TEST_CASE("16 voice, RESONANT_SWEEP", "[bench][plugin][scn:em_resonant]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = false;
    spec.fullMod = true;
    spec.em = Patch::SourceNode::ExtendedMode::RESONANT_SWEEP;
    runScenario("scn:em_resonant", Level::Plugin, spec, 16);
}

TEST_CASE("16 voice, NOISE", "[bench][plugin][scn:em_noise]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = false;
    spec.fullMod = true;
    spec.em = Patch::SourceNode::ExtendedMode::NOISE;
    runScenario("scn:em_noise", Level::Plugin, spec, 16);
}

TEST_CASE("16 voice, dense, no self-FB (SIMD baseline)", "[bench][plugin][scn:no_fb_simd]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = false;
    spec.fullMod = true;
    runScenario("scn:no_fb_simd", Level::Plugin, spec, 16);
}

TEST_CASE("worst case: 64v + NOISE + everything", "[bench][plugin][scn:worst]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = true;
    spec.fullMod = true;
    spec.em = Patch::SourceNode::ExtendedMode::NOISE;
    runScenario("scn:worst", Level::Plugin, spec, 64);
}

// ---------------------------------------------------------------------------
// Voice-level mirrors of a couple key scenarios — same patches, no SRC tail.
// Useful when PERFORMANCE.md changes are voice-internal and we want to see
// the change without SRC overhead masking it.
// ---------------------------------------------------------------------------

TEST_CASE("voice: 8v dense", "[bench][voice][scn:voice_8v_dense]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = true;
    spec.fullMod = true;
    runScenario("scn:voice_8v_dense", Level::Voice, spec, 8);
}

TEST_CASE("voice: 32v dense", "[bench][voice][scn:voice_32v_dense]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = true;
    spec.fullMod = true;
    runScenario("scn:voice_32v_dense", Level::Voice, spec, 32);
}

TEST_CASE("voice: 16v RESONANT_SWEEP", "[bench][voice][scn:voice_em_resonant]")
{
    ScenarioSpec spec{};
    spec.activeOps = 6;
    spec.fullMatrix = true;
    spec.allSelfFB = false;
    spec.fullMod = true;
    spec.em = Patch::SourceNode::ExtendedMode::RESONANT_SWEEP;
    runScenario("scn:voice_em_resonant", Level::Voice, spec, 16);
}

// ---------------------------------------------------------------------------
// Inner-loop microbenchmarks — single OpSource, no matrix/mixer/output.
// Useful for tuning PERFORMANCE.md items #2/#3/#4. Auto-calibration picks
// a large iters_per_sample here automatically (each call is fast).
// ---------------------------------------------------------------------------

TEST_CASE("inner: EM::NONE", "[bench][inner][scn:inner_none]")
{
    ScenarioSpec spec{};
    spec.activeOps = 1;
    runScenario("scn:inner_none", Level::Inner, spec, 1);
}

TEST_CASE("inner: EM::PHASE_REMAP", "[bench][inner][scn:inner_phaseremap]")
{
    ScenarioSpec spec{};
    spec.activeOps = 1;
    spec.em = Patch::SourceNode::ExtendedMode::PHASE_REMAP;
    runScenario("scn:inner_phaseremap", Level::Inner, spec, 1);
}

TEST_CASE("inner: EM::RESONANT_SWEEP", "[bench][inner][scn:inner_resonant]")
{
    ScenarioSpec spec{};
    spec.activeOps = 1;
    spec.em = Patch::SourceNode::ExtendedMode::RESONANT_SWEEP;
    runScenario("scn:inner_resonant", Level::Inner, spec, 1);
}

TEST_CASE("inner: EM::NOISE", "[bench][inner][scn:inner_noise]")
{
    ScenarioSpec spec{};
    spec.activeOps = 1;
    spec.em = Patch::SourceNode::ExtendedMode::NOISE;
    runScenario("scn:inner_noise", Level::Inner, spec, 1);
}
