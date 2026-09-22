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

#ifndef BACONPAUL_SIX_SINES_DSP_WAVETABLE_BUILD_H
#define BACONPAUL_SIX_SINES_DSP_WAVETABLE_BUILD_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "dsp/wavetable.h"

namespace baconpaul::six_sines
{
/*
 * Whatever a parser pulled out of a .wt or .wav: frame-major single cycles, all the
 * same length. Every format funnels through this, so nothing downstream branches on
 * provenance.
 */
struct SourceFrames
{
    uint32_t cycleLength{0};
    uint32_t nFrames{0};
    std::vector<float> samples; // nFrames * cycleLength, frame major
    std::string name;
};

/*
 * One frame as (re, im) pairs per harmonic, index 0 = DC. This is the intermediate the
 * whole build runs through: band-limiting is a truncation, differentiation is a multiply by
 * j*2*pi*n, and resampling the frame axis is a lerp of the coefficients (identical to
 * lerping the samples, since the transform is linear).
 */
struct HarmonicFrames
{
    uint32_t nHarmonics{0}; // highest harmonic index stored
    uint32_t nFrames{0};
    std::vector<float> coeffs; // nFrames * (nHarmonics + 1) * 2

    const float *frame(uint32_t f) const
    {
        return coeffs.data() + static_cast<size_t>(f) * (nHarmonics + 1) * 2;
    }
    float *frame(uint32_t f)
    {
        return coeffs.data() + static_cast<size_t>(f) * (nHarmonics + 1) * 2;
    }
};

// true when pffft can transform this length: (2^a)(3^b)(5^c) with a >= 5
bool isUsableFFTLength(uint32_t n);

// nullptr when the source geometry is unusable
std::unique_ptr<HarmonicFrames> analyzeSource(const SourceFrames &);

void resampleFrameAxis(HarmonicFrames &, uint32_t targetFrames);

std::unique_ptr<Wavetable> buildWavetable(const HarmonicFrames &, uint64_t hash,
                                          const std::string &name,
                                          WavetableBandLimit mode = WavetableBandLimit::BAND_LIMITED,
                                          bool mipChain = true);

// the whole pipeline; nullptr when the source is unusable
std::unique_ptr<Wavetable> buildWavetableFromSource(
    const SourceFrames &, uint64_t hash, const std::string &name,
    WavetableBandLimit mode = WavetableBandLimit::BAND_LIMITED, bool mipChain = true);

// Mode is part of a table's identity: the same file in two modes is two tables.
uint64_t saltHashWithMode(uint64_t hash, WavetableBandLimit mode, bool mipChain);

// Harmonic magnitudes of one built level+frame, for tests. Not used by the engine.
std::vector<double> analyzeLevelForTest(const Wavetable &, uint8_t level, uint32_t frame);
} // namespace baconpaul::six_sines
#endif // BACONPAUL_SIX_SINES_DSP_WAVETABLE_BUILD_H
