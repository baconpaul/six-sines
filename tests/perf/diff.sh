#!/usr/bin/env bash
# Compare two CSVs produced by tests/perf/run.sh, print per-scenario
# percent deltas, flag anything outside the threshold.
#
# Usage:
#   tests/perf/diff.sh <before.csv> <after.csv> [threshold_pct]
#
# Default threshold = 5%. Improvements (lower block_ns_median) are negative
# percentages; regressions are positive. The exit code is 0 on success
# regardless of deltas — this is a reporting tool, not a gate.

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "usage: $0 <before.csv> <after.csv> [threshold_pct]" >&2
    exit 2
fi

BEFORE="$1"
AFTER="$2"
THRESH="${3:-5}"

python3 - "${BEFORE}" "${AFTER}" "${THRESH}" <<'PY'
import sys, csv
before_path, after_path, thresh = sys.argv[1], sys.argv[2], float(sys.argv[3])

def load(p):
    rows = {}
    with open(p) as f:
        for r in csv.DictReader(f):
            rows[(r['tag'], r['level'])] = r
    return rows

b = load(before_path)
a = load(after_path)

keys = sorted(set(b.keys()) | set(a.keys()))

w = lambda s, n: s.ljust(n)
print(f"{w('tag', 28)} {w('lvl', 7)} {w('before_ns', 12)} {w('after_ns', 12)} {w('delta_%', 10)} flag")
print("-" * 78)
for k in keys:
    tag, lvl = k
    br = b.get(k)
    ar = a.get(k)
    if not br:
        print(f"{w(tag, 28)} {w(lvl, 7)} {w('-', 12)} {w(ar['block_ns_median'], 12)} {w('NEW', 10)}")
        continue
    if not ar:
        print(f"{w(tag, 28)} {w(lvl, 7)} {w(br['block_ns_median'], 12)} {w('-', 12)} {w('GONE', 10)}")
        continue
    bv = float(br['block_ns_median'])
    av = float(ar['block_ns_median'])
    delta_pct = (av - bv) / bv * 100.0 if bv > 0 else 0.0
    flag = ''
    if abs(delta_pct) >= thresh:
        flag = '⚠ regression' if delta_pct > 0 else '✓ improvement'
    print(f"{w(tag, 28)} {w(lvl, 7)} {w(f'{bv:.1f}', 12)} {w(f'{av:.1f}', 12)} "
          f"{w(f'{delta_pct:+.2f}%', 10)} {flag}")
PY
