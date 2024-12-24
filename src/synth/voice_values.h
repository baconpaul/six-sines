/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work will be
 * released under GPL3.
 *
 * The source code and license are at https://github.com/baconpaul/six-sines
 */

#ifndef BACONPAUL_SIX_SINES_SYNTH_VOICE_VALUES_H
#define BACONPAUL_SIX_SINES_SYNTH_VOICE_VALUES_H

#include <sst/basic-blocks/tables/EqualTuningProvider.h>
#include <sst/basic-blocks/tables/TwoToTheXProvider.h>

struct MTSClient;

namespace baconpaul::six_sines
{
struct VoiceValues
{
    VoiceValues() {}

    bool gated{false};
    int key{0}, channel{0};
    float velocity{0}, releaseVelocity{0};
};
};     // namespace baconpaul::six_sines
#endif // MONO_VALUES_H
