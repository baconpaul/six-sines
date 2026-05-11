/*
 * Perf harness entry point. Same Catch2 wiring as `six-sines-test` (Catch2
 * gives us TEST_CASE filtering and tag-based selection); the actual timing
 * comes from a custom helper (perf_timing.h) so we control the digest
 * format that `tests/perf/diff.sh` greps.
 *
 * Filter scenarios with tags, e.g.:
 *   ./six-sines-perf "[scn:8v_dense]"      run one
 *   ./six-sines-perf "[bench][plugin]"     all plugin-level
 *   ./six-sines-perf                       everything
 */

#define CATCH_CONFIG_RUNNER
#include "catch2/catch2.hpp"

int main(int argc, char *argv[]) { return Catch::Session().run(argc, argv); }
