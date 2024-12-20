/*
 * Six Sines A Sinnin'
 *
 * A mess, with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#ifndef BACONPAUL_SIX_SINES_DSP_SR_PROVIDER_H
#define BACONPAUL_SIX_SINES_DSP_SR_PROVIDER_H

#include "configuration.h"

namespace baconpaul::six_sines
{
struct SRProvider
{
    float envelope_rate_linear_nowrap(float f) { return (blockSize / gSampleRate) * pow(2.0, -f); }
    static constexpr float samplerate{gSampleRate};
};
} // namespace baconpaul::six_sines
#endif // SR_PROVIDER_H
