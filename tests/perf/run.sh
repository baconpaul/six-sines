#!/usr/bin/env bash
# Run six-sines-perf, capture the [scn:...] digest lines, save as CSV.
#
# Usage:
#   tests/perf/run.sh                    # writes to tests/perf/results/<branch>-<ts>.csv
#   tests/perf/run.sh <label>            # writes to tests/perf/results/<label>.csv
#   tests/perf/run.sh <label> "[bench][plugin]"   # run a Catch2 tag filter
#
# Expects the perf binary at $PERF_BIN or one of these defaults:
#   ./cmake-build-perf/tests/six-sines-perf
#   ./cmake-build-release/tests/six-sines-perf
#
# Repeats the full run REPEATS times (default 3) and emits the per-scenario
# median across the repeats — the same shape diff.sh consumes.
#
# Env vars:
#   REPEATS=N           how many times to run the full suite (default 3)
#   PERF_SAMPLE_MS=N    wall-clock target per timed sample, passed to the
#                       binary (default 30 in code). Use 5 for a fast smoke
#                       check, 100+ for a long stable run.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
RESULTS_DIR="${REPO_ROOT}/tests/perf/results"
mkdir -p "${RESULTS_DIR}"

PERF_BIN="${PERF_BIN:-}"
if [[ -z "${PERF_BIN}" ]]; then
    for candidate in \
        "${REPO_ROOT}/cmake-build-perf/tests/six-sines-perf" \
        "${REPO_ROOT}/cmake-build-release/tests/six-sines-perf"; do
        if [[ -x "${candidate}" ]]; then
            PERF_BIN="${candidate}"
            break
        fi
    done
fi

if [[ -z "${PERF_BIN}" || ! -x "${PERF_BIN}" ]]; then
    echo "error: six-sines-perf binary not found." >&2
    echo "build with:" >&2
    echo "  cmake -B cmake-build-perf -DCMAKE_BUILD_TYPE=Release -GNinja" >&2
    echo "  cmake --build cmake-build-perf --target six-sines-perf" >&2
    exit 1
fi

LABEL="${1:-}"
FILTER="${2:-}"
REPEATS="${REPEATS:-3}"

if [[ -z "${LABEL}" ]]; then
    BRANCH="$(git -C "${REPO_ROOT}" rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
    TS="$(date +%Y%m%d-%H%M%S)"
    LABEL="${BRANCH}-${TS}"
fi

OUT="${RESULTS_DIR}/${LABEL}.csv"
TMP="$(mktemp -t sixsines-perf.XXXXXX)"
trap 'rm -f "${TMP}".* "${TMP}"' EXIT

echo "perf binary : ${PERF_BIN}"
echo "label       : ${LABEL}"
echo "repeats     : ${REPEATS}"
echo "filter      : ${FILTER:-<none>}"
echo

for ((i = 1; i <= REPEATS; ++i)); do
    echo "--- run ${i}/${REPEATS} ---"
    if [[ -n "${FILTER}" ]]; then
        "${PERF_BIN}" "${FILTER}" | tee "${TMP}.${i}"
    else
        "${PERF_BIN}" | tee "${TMP}.${i}"
    fi
done

# Pull just the digest lines from each repeat, then compute the median of
# block_ns per (tag, level) tuple.
python3 - "${OUT}" "${TMP}" "${REPEATS}" <<'PY'
import sys, re, statistics
out_path, tmp_prefix, repeats = sys.argv[1], sys.argv[2], int(sys.argv[3])
kv_re = re.compile(r'(\w+)=([^\s]+)')
rows_by_key = {}
for i in range(1, repeats + 1):
    try:
        with open(f"{tmp_prefix}.{i}") as f:
            for line in f:
                if not line.startswith('[scn:'):
                    continue
                tag = line.split(']', 1)[0].lstrip('[')
                kv = dict(kv_re.findall(line))
                key = (tag, kv.get('level', '?'))
                rows_by_key.setdefault(key, []).append(kv)
    except FileNotFoundError:
        pass

cols = ['tag', 'level', 'voices', 'ops', 'block_ns_median',
        'sample_ns_median', 'cpu_pct_48k_median', 'vops_per_s_median',
        'stddev_pct_max', 'iters_first', 'hash_first']
with open(out_path, 'w') as o:
    o.write(','.join(cols) + '\n')
    for (tag, level), runs in sorted(rows_by_key.items()):
        def med(field):
            vals = [float(r[field]) for r in runs if field in r]
            return statistics.median(vals) if vals else 0.0
        def mx(field):
            vals = [float(r[field]) for r in runs if field in r]
            return max(vals) if vals else 0.0
        first = runs[0]
        o.write(','.join([
            tag,
            level,
            first.get('voices', '?'),
            first.get('ops', '?'),
            f"{med('block_ns'):.2f}",
            f"{med('sample_ns'):.4f}",
            f"{med('cpu_pct_48k'):.3f}",
            f"{med('vops_per_s'):.4g}",
            f"{mx('stddev_pct'):.2f}",
            first.get('iters', '?'),
            first.get('hash', '?'),
        ]) + '\n')
print(f"\nwrote {out_path}")
PY

echo "saved: ${OUT}"
