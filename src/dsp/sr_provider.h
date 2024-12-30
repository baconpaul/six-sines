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

#ifndef BACONPAUL_SIX_SINES_DSP_SR_PROVIDER_H
#define BACONPAUL_SIX_SINES_DSP_SR_PROVIDER_H

#include "configuration.h"
#include "synth/mono_values.h"

namespace baconpaul::six_sines
{
struct SRProvider
{
    const MonoValues &monoValues;
    SRProvider(const MonoValues &mv) : monoValues(mv) {}
    float envelope_rate_linear_nowrap(float f)
    {
        return (blockSize / gSampleRate) * monoValues.twoToTheX.twoToThe(-f);
    }
    static constexpr double samplerate{gSampleRate};
    static constexpr double sampleRate{gSampleRate};
    static constexpr double sampleRateInv{1.0 / gSampleRate};
};
} // namespace baconpaul::six_sines
#endif // SR_PROVIDER_H
