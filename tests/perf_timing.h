/*
 * Shared timing + result-printing helpers for the perf harness.
 *
 * We don't use Catch2's BENCHMARK macro because we want full control over
 * the printed digest format (so `tests/perf/diff.sh` can grep it). Catch2
 * still provides TEST_CASE for filtering and tagging.
 */

#ifndef BACONPAUL_SIX_SINES_TESTS_PERF_TIMING_H
#define BACONPAUL_SIX_SINES_TESTS_PERF_TIMING_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "configuration.h"

namespace baconpaul::six_sines::perf
{

struct BenchResult
{
    double median_ns_per_iter{0};
    double min_ns_per_iter{0};
    double stddev_pct{0};
    int iters_per_sample{0}; // chosen by calibration; printed for transparency
};

// Auto-calibrating timer. We avoid a hardcoded `itersPerSample` (it starves
// fast scenarios and balloons slow ones) by first probing how long one call
// takes, then picking the iteration count needed for each timed sample to
// reach `target_sample_ms` wall-clock. That gives stable medians across
// 100ns-per-call inner-loop benches and 100µs-per-call 64-voice benches alike.
//
// PERF_SAMPLE_MS env var overrides target_sample_ms (handy for quick smoke
// runs vs long stable runs without recompiling).
template <typename Work>
inline BenchResult timeIt(int samples, int warmup, double target_sample_ms, Work &&work)
{
    using clock = std::chrono::steady_clock;

    if (auto *envOverride = std::getenv("PERF_SAMPLE_MS"))
    {
        double v = std::atof(envOverride);
        if (v > 0)
            target_sample_ms = v;
    }
    const double target_ns = target_sample_ms * 1e6;

    // 1. Warmup — let caches, branch predictor, and the OS settle.
    for (int i = 0; i < warmup; ++i)
        work();

    // 2. Calibrate: geometric search for an iteration count that fills the
    //    target window. Capped at 100M iters as a safety against pathological
    //    sub-nanosecond work() (it doesn't exist here, but be defensive).
    int iters = 1;
    const int max_iters = 100'000'000;
    while (iters < max_iters)
    {
        auto t0 = clock::now();
        for (int k = 0; k < iters; ++k)
            work();
        auto dt_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - t0).count();
        if (dt_ns >= target_ns * 0.5)
        {
            // Scale up to hit target_ns precisely.
            double scale = target_ns / std::max<double>(1.0, dt_ns);
            iters = std::max(1, (int)(iters * scale));
            break;
        }
        if (dt_ns == 0)
            iters *= 16; // way too fast; jump harder
        else
            iters *= 4;
    }
    iters = std::min(iters, max_iters);

    // 3. Time `samples` batches.
    std::vector<double> times;
    times.reserve(samples);
    for (int i = 0; i < samples; ++i)
    {
        auto t0 = clock::now();
        for (int k = 0; k < iters; ++k)
            work();
        auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - t0).count();
        times.push_back(static_cast<double>(dt) / iters);
    }
    std::sort(times.begin(), times.end());
    double median = times[times.size() / 2];
    double min = times.front();
    double mean = 0;
    for (auto t : times)
        mean += t;
    mean /= times.size();
    double var = 0;
    for (auto t : times)
        var += (t - mean) * (t - mean);
    var /= times.size();
    double stddev = std::sqrt(var);
    double stddev_pct = (mean > 0) ? (100.0 * stddev / mean) : 0;
    return {median, min, stddev_pct, iters};
}

// FNV-1a over the float bytes of a buffer. Lets each scenario print a
// signature alongside its timing so we can detect if a "no-op refactor"
// silently changed numeric output.
inline uint64_t hashFloats(const float *data, size_t n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    auto *bytes = reinterpret_cast<const uint8_t *>(data);
    for (size_t i = 0; i < n * sizeof(float); ++i)
    {
        h ^= bytes[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

// One-line printable digest, prefixed with the scenario tag so
// `grep '^\[scn:' six-sines-perf-output.txt` is the CSV.
//
//   [scn:8v_dense] level=plugin block_ns=12480 sample_ns=1560 cpu_pct_48k=7.5 vops_per_s=3.07e8
//   stddev_pct=2.1 hash=0x...
//
// `level` is one of: plugin, voice, inner.
// `block_ns` is wall-clock per host (or engine) block, depending on level.
// `sample_ns` is per audio sample at the host rate.
// `cpu_pct_48k` is what fraction of a 48 kHz host realtime budget this
//   workload consumed (samples_in_test * (1/48000) / total_wallclock_time
//   ... but we compute it from block_ns to avoid the explicit total).
// `vops_per_s` is voices * active-ops * samples-per-second sustained.
struct DigestParams
{
    const char *tag{"?"};
    const char *level{"?"};
    int voices{0};
    int activeOps{0};
    double block_ns{0};             // wall-clock per processed block at this level
    int samplesPerBlock{blockSize}; // host-rate samples this block produces (plugin) or
                                    // engine-rate samples for voice/inner
    double stddev_pct{0};
    int iters_per_sample{0}; // how many work() calls each timed sample contained
    uint64_t hash{0};
    const char *notes{""}; // optional free-text suffix
};

inline void printDigest(const DigestParams &d)
{
    double sample_ns = d.block_ns / d.samplesPerBlock;
    // Workload generated samples / wall-clock time.
    // For plugin-level, samplesPerBlock = host blockSize at 48k → 1/48000 s of audio.
    // For voice-level we keep the same arithmetic: the scenario advertises whatever
    // "samples produced" makes sense at its level. (Voice-level produces engine-rate
    // samples, so cpu_pct_48k is informational only.)
    double cpu_pct_48k = (sample_ns / 1e9) * 48000.0 * 100.0;
    double vops_per_s =
        (sample_ns > 0) ? (double)d.voices * (double)d.activeOps * 1e9 / sample_ns : 0.0;
    std::printf("[%s] level=%s voices=%d ops=%d block_ns=%.1f sample_ns=%.2f "
                "cpu_pct_48k=%.2f vops_per_s=%.3e stddev_pct=%.2f iters=%d hash=0x%016llx%s%s\n",
                d.tag, d.level, d.voices, d.activeOps, d.block_ns, sample_ns, cpu_pct_48k,
                vops_per_s, d.stddev_pct, d.iters_per_sample,
                static_cast<unsigned long long>(d.hash), (d.notes && d.notes[0]) ? " " : "",
                d.notes ? d.notes : "");
    std::fflush(stdout);
}

} // namespace baconpaul::six_sines::perf

#endif
