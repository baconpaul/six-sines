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

#include "mod_matrix.h"
#include "sst/cpputils/constructors.h"

namespace baconpaul::six_sines
{
ModMatrixConfig::ModMatrixConfig()
{
    add(OFF, "", "OFF");
    add(CHANNEL_AT, "MIDI", "Channel AT");
    add(PITCH_BEND, "MIDI", "Pitch Bend");

    for (int cc = 0; cc < 128; ++cc)
        add(MIDICC_0 + cc, "MIDI CC", "CC " + std::to_string(cc + 1));
    for (int mc = 0; mc < numMacros; ++mc)
        add(MACRO_0 + mc, "Macros", "Macro " + std::to_string(mc + 1));

    add(VELOCITY, "MIDI", "Velocity");
    add(RELEASE_VELOCITY, "MIDI", "Release Velocity");
    add(POLY_AT, "MIDI", "Poly AT");
    add(GATED, "Voice", "Gated");
    add(RELEASED, "Voice", "Released");

    add(MPE_PRESSURE, "MPE", "Pressure");
    add(MPE_TIMBRE, "MPE", "Timbre");
}

void ModMatrixConfig::add(int s, const std::string &group, const std::string &nm) {}

} // namespace baconpaul::six_sines