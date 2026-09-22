/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024-2025, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work will be
 * released under GPL3.
 *
 * The source code and license are at https://github.com/baconpaul/six-sines
 */

/*
 * Spectral purity of one plain sine operator, measured at the plugin output at 48kHz - so
 * through the oversampled engine and back down through the resampler, not off the raw
 * oscillator.
 *
 * Reports, per octave from C1 to C8:
 *   - measured fundamental against the note's nominal frequency, in cents
 *   - the worst single spurious component, in dB below the fundamental
 *   - everything that is not the fundamental, summed, in dB below it
 *
 * Run it on its own to see the table:
 *   ./six-sines-test "[spectrum]" -s 2>&1 | grep SPECTRUM
 */

#include "catch2/catch2.hpp"

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "configuration.h"
#include "dsp/sintable.h"
#include "synth/matrix_index.h"
#include "synth/patch.h"
#include "synth/synth.h"

#include "pffft.h"

using namespace baconpaul::six_sines;

namespace
{
static constexpr int fftOrder{16};
static constexpr int fftSize{1 << fftOrder}; // 65536 at 48k is 1.37s
static constexpr double hostRate{48000.0};

// Blackman-Harris. Its -92dB sidelobes sit under the pre-fix spurs at -87dB but not under
// the post-fix ones at -99dB, which is why the fundamental's skirt is excluded by bin
// distance below rather than trusted to fall away on its own.
double window(int i, int n)
{
    static constexpr double a0{0.35875}, a1{0.48829}, a2{0.14128}, a3{0.01168};
    auto t = 2 * M_PI * i / (n - 1);
    return a0 - a1 * std::cos(t) + a2 * std::cos(2 * t) - a3 * std::cos(3 * t);
}

// One steady sine operator, everything else out of the way.
std::unique_ptr<Synth> makeSineSynth(int key)
{
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(hostRate);

    auto &p = s->patch;
    p.output.playMode.value = 0.f;
    p.output.polyLimit.value = 1.f;
    p.output.unisonCount.value = 1.f;

    auto flatEnv = [](auto &n)
    {
        n.delay.value = 0.f;
        n.attack.value = 0.f;
        n.hold.value = 0.f;
        n.decay.value = 0.f;
        n.sustain.value = 1.f;
        n.release.value = 0.f;
    };

    auto &sn = p.sourceNodes[0];
    sn.active.value = 1.f;
    sn.waveForm.value = (float)SinTable::SIN;
    sn.ratio.value = 0.f;
    sn.startingPhase.value = 0.f;
    sn.keyTrack.value = 1.f;
    flatEnv(sn);

    flatEnv(p.mixerNodes[0]);
    p.mixerNodes[0].level.value = 1.f;
    p.mixerNodes[0].pan.value = 0.f;

    flatEnv(p.output);
    p.output.level.value = 1.f;
    p.output.velSensitivity.value = 0.f;

    s->reapplyControlSettings();
    s->voiceManager->processNoteOnEvent(0, 0, key, -1, 1.0f, 0.f);

    // let the envelopes and the resampler settle before anything is captured
    for (int i = 0; i < 4096 / blockSize; ++i)
        s->process(nullptr);
    return s;
}

struct Measurement
{
    double f0Hz{0};
    double centsError{0};
    double worstSpurDb{0}; // dB relative to the fundamental, negative
    double totalNonFundDb{0};
    bool valid{false};
};

Measurement measureKey(int key)
{
    Measurement m;
    auto expected = 440.0 * std::pow(2.0, (key - 69) / 12.0);

    auto s = makeSineSynth(key);

    std::vector<float> buf;
    buf.reserve(fftSize);
    while ((int)buf.size() < fftSize)
    {
        s->process(nullptr);
        for (int i = 0; i < blockSize && (int)buf.size() < fftSize; ++i)
            buf.push_back(s->output[0][i]);
    }

    double peak{0};
    for (auto v : buf)
        peak = std::max(peak, (double)std::abs(v));
    if (peak < 1e-4)
        return m; // nothing came out; caller reports rather than dividing by zero

    auto *in = (float *)pffft::pffft_aligned_malloc(fftSize * sizeof(float));
    auto *out = (float *)pffft::pffft_aligned_malloc(fftSize * sizeof(float));
    auto *work = (float *)pffft::pffft_aligned_malloc(fftSize * sizeof(float));
    auto *setup = pffft::pffft_new_setup(fftSize, pffft::PFFFT_REAL);

    double wsum{0};
    for (int i = 0; i < fftSize; ++i)
    {
        auto w = window(i, fftSize);
        in[i] = (float)(buf[i] * w);
        wsum += w;
    }
    pffft::pffft_transform_ordered(setup, in, out, work, pffft::PFFFT_FORWARD);

    // magnitudes, normalised so a full scale sine reads 1.0
    std::vector<double> mag(fftSize / 2, 0.0);
    for (int k = 0; k < fftSize / 2; ++k)
    {
        auto re = out[2 * k], im = out[2 * k + 1];
        mag[k] = 2.0 * std::sqrt((double)re * re + (double)im * im) / wsum;
    }

    pffft::pffft_destroy_setup(setup);
    pffft::pffft_aligned_free(in);
    pffft::pffft_aligned_free(out);
    pffft::pffft_aligned_free(work);

    auto binHz = hostRate / fftSize;

    // locate the fundamental near where it should be
    auto centre = (int)std::round(expected / binHz);
    auto lo = std::max(1, centre - 40), hi = std::min((int)mag.size() - 2, centre + 40);
    int kPeak{lo};
    for (int k = lo; k <= hi; ++k)
        if (mag[k] > mag[kPeak])
            kPeak = k;

    // quadratic interpolation on the log magnitudes gives sub-bin frequency
    auto l = std::log(std::max(mag[kPeak - 1], 1e-30));
    auto c = std::log(std::max(mag[kPeak], 1e-30));
    auto r = std::log(std::max(mag[kPeak + 1], 1e-30));
    auto delta = 0.5 * (l - r) / (l - 2 * c + r);
    m.f0Hz = (kPeak + delta) * binHz;
    m.centsError = 1200.0 * std::log2(m.f0Hz / expected);

    // Blackman-Harris spreads a tone over a few bins; exclude its skirt from "spurious"
    static constexpr int skirt{8};
    double fundEnergy{0};
    for (int k = std::max(0, kPeak - skirt); k <= std::min((int)mag.size() - 1, kPeak + skirt); ++k)
        fundEnergy += mag[k] * mag[k];
    auto fundAmp = std::sqrt(fundEnergy);

    double worst{0}, rest{0};
    for (int k = 2; k < (int)mag.size(); ++k)
    {
        if (std::abs(k - kPeak) <= skirt)
            continue;
        worst = std::max(worst, mag[k]);
        rest += mag[k] * mag[k];
    }
    m.worstSpurDb = 20 * std::log10(std::max(worst, 1e-30) / fundAmp);
    m.totalNonFundDb = 20 * std::log10(std::max(std::sqrt(rest), 1e-30) / fundAmp);
    m.valid = true;
    return m;
}
} // namespace

TEST_CASE("A plain sine operator is spectrally clean across the keyboard", "[spectrum]")
{
    MatrixIndex::initialize();

    std::printf("SPECTRUM  note   nominal Hz   measured Hz    cents    worst spur   "
                "non-fundamental\n");

    // C1..C8 are MIDI 24, 36, ... 108
    for (int key = 24; key <= 108; key += 12)
    {
        auto m = measureKey(key);
        auto expected = 440.0 * std::pow(2.0, (key - 69) / 12.0);
        INFO("key " << key);
        REQUIRE(m.valid);

        std::printf("SPECTRUM  C%-4d  %10.3f   %10.3f   %+7.3f   %8.1f dB   %8.1f dB\n",
                    (key - 24) / 12 + 1, expected, m.f0Hz, m.centsError, m.worstSpurDb,
                    m.totalNonFundDb);

        /*
         * Measured bounds, not golden values. The pre-fix quadrant grid sat at -86 to -89dB
         * worst spur and -79 to -82dB total, so these thresholds do catch it; after the fix
         * the numbers are -98 to -104dB and -92 to -97dB, which is the resampler and float32
         * floor rather than the table.
         *
         * The cents bound is generous because it is measuring the estimator, not the engine:
         * the residual reads identically before and after the grid fix, so none of it comes
         * from the table. At C1 a 1.37s window has 0.73Hz bins, and 0.2 cents is 0.004Hz.
         */
        REQUIRE(std::abs(m.centsError) < 0.5);
        REQUIRE(m.worstSpurDb < -95.0);
        REQUIRE(m.totalNonFundDb < -88.0);
    }
    std::fflush(stdout);
}
