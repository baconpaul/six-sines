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

#ifndef BACONPAUL_SIX_SINES_SYNTH_MONO_VALUES_H
#define BACONPAUL_SIX_SINES_SYNTH_MONO_VALUES_H

#include <sst/basic-blocks/tables/EqualTuningProvider.h>
#include <sst/basic-blocks/tables/TwoToTheXProvider.h>
#include <sst/basic-blocks/dsp/RNG.h>

#include "mod_matrix.h"

struct MTSClient;

namespace baconpaul::six_sines
{
struct MonoValues
{
    MonoValues()
    {
        tuningProvider.init();
        twoToTheX.init();
        std::fill(macroPtr.begin(), macroPtr.end(), nullptr);
    }

    float tempoSyncRatio{1};
    float pitchBend{0.f};
    std::array<int8_t, 128> midiCC;
    std::array<float, 128> midiCCFloat; // 0...1 normed
    float channelAT{0.f};

    std::array<float *, numMacros> macroPtr;

    MTSClient *mtsClient{nullptr};

    sst::basic_blocks::tables::EqualTuningProvider tuningProvider;
    sst::basic_blocks::tables::TwoToTheXProvider twoToTheX;

    sst::basic_blocks::dsp::RNG rng;

    ModMatrixConfig modMatrixConfig;
};
};     // namespace baconpaul::six_sines
#endif // MONO_VALUES_H
