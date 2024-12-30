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

#ifndef BACONPAUL_SIX_SINES_SYNTH_MOD_MATRIX_H
#define BACONPAUL_SIX_SINES_SYNTH_MOD_MATRIX_H

#include <cstdint>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "configuration.h"

namespace baconpaul::six_sines
{
struct ModMatrixConfig
{
    // These ids stream
    static constexpr uint32_t voiceLevel{5000};
    enum Source : uint32_t
    {
        OFF = 0,

        // Monophonic Midi
        CHANNEL_AT = 100,
        PITCH_BEND = 101,

        MIDICC_0 = 200, // leave 126 gap after this

        // Macros
        MACRO_0 = 400, // leave numMacros gap after this

        // Voice level
        VELOCITY = voiceLevel + 0,
        RELEASE_VELOCITY = voiceLevel + 1,
        POLY_AT = voiceLevel + 2,
        GATED = voiceLevel + 50,
        RELEASED = voiceLevel + 51,
        UNISON_VAL = voiceLevel + 60,

        MPE_PRESSURE = voiceLevel + 100,
        MPE_TIMBRE = voiceLevel + 101,
        MPE_PITCHBEND = voiceLevel + 102,

        RANDOM_01 = voiceLevel + 200,
        RANDOM_PM1 = voiceLevel + 201,
        RANDOM_NORM = voiceLevel + 202,
        RANDOM_HALFNORM = voiceLevel + 203
    };
    ModMatrixConfig();

    void add(int s, const std::string &group, const std::string &nm);
    struct SourceObj
    {
        int id;
        std::string group;
        std::string name;
    };
    std::vector<SourceObj> sources;
    std::unordered_map<uint32_t, SourceObj> sourceByID;
};
} // namespace baconpaul::six_sines
#endif // MOD_MATRIX_H
