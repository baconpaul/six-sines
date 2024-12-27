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

#ifndef BACONPAUL_SIX_SINES_SYNTH_MOD_MATRIX_H
#define BACONPAUL_SIX_SINES_SYNTH_MOD_MATRIX_H

#include <cstdint>
#include "configuration.h"

namespace baconpaul::six_sines
{
struct ModMatrixConfig
{
    static constexpr uint32_t voiceLevel{1024};
    static constexpr uint32_t offId{0};

    ModMatrixConfig();

    struct Source
    {
        Source(ModMatrixConfig &, uint32_t id, const std::string &v);
        uint32_t id;
        const std::string name;
    };

    // Mono sources
    Source off;
    std::array<Source, 128> midiCCs;
    Source channelAT;
    std::array<Source, numMacros> macros;

    Source velocity;
    Source gated, released;
    Source polyAT;
};
} // namespace baconpaul::six_sines
#endif // MOD_MATRIX_H
