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

#include "mod_matrix.h"
#include <sst/basic-blocks/tables/MIDICCNames.h>
#include <fmt/core.h>

namespace baconpaul::six_sines
{
ModMatrixConfig::ModMatrixConfig()
{
    add(OFF, "", "Off");
    add(CHANNEL_AT, "MIDI", "Channel AT");
    add(PITCH_BEND, "MIDI", "Pitch Bend");
    add(MIDICC_0 + 1, "MIDI", "Mod Wheel");

    for (int cc = 2; cc < 128; ++cc)
    {
        if (cc >= 98 && cc <= 101)
        {
            // nprn
            continue;
        }
        if (cc == 120 || cc == 123)
        {
            // note sound off
            continue;
        }
        auto s = sst::basic_blocks::tables::MIDI1CCVeryShortName(cc);
        if (!s.empty())
            s = " - " + s;
        auto q = fmt::format("CC {:03d}{}", cc, s);
        add(MIDICC_0 + cc, "MIDI CC", q);
    }
    for (int mc = 0; mc < numMacros; ++mc)
        add(MACRO_0 + mc, "Macros", "Macro " + std::to_string(mc + 1));

    add(VELOCITY, "MIDI", "Velocity");
    add(RELEASE_VELOCITY, "MIDI", "Release Velocity");
    add(POLY_AT, "MIDI", "Poly AT");
    add(GATED, "Voice", "Gated");
    add(RELEASED, "Voice", "Released");
    add(UNISON_VAL, "Voice", "Unison Position");

    add(MPE_PRESSURE, "MPE", "Pressure");
    add(MPE_TIMBRE, "MPE", "Timbre");

    add(RANDOM_01, "Random", "Rand Unif 01");
    add(RANDOM_PM1, "Random", "Rand Unif Bi");
    add(RANDOM_NORM, "Random", "Rand Normal");
    add(RANDOM_HALFNORM, "Random", "Rand HalfNorm");

    std::sort(sources.begin(), sources.end(),
              [](const SourceObj &a, const SourceObj &b)
              {
                  if (a.group != b.group)
                      return a.group < b.group;
                  if (a.name != b.name)
                      return a.name < b.name;
                  return a.id < b.id;
              });
    for (auto &s : sources)
    {
        sourceByID[s.id] = s;
    }
}

void ModMatrixConfig::add(int s, const std::string &group, const std::string &nm)
{
    sources.push_back({s, group, nm});
}

} // namespace baconpaul::six_sines