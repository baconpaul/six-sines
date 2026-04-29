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
        add(MACRO_0 + mc, "Macros", "Macro " + std::to_string(mc + 1) + " Amplitude");
    for (int mc = 0; mc < numMacros; ++mc)
        add(MACRO_MOD_0 + mc, "Macros", "Macro " + std::to_string(mc + 1) + " Modulated");

    add(VELOCITY, "MIDI", "Velocity");
    add(RELEASE_VELOCITY, "MIDI", "Release Velocity");
    add(POLY_AT, "MIDI", "Poly AT");
    add(GATED, "Voice", "Gated");
    add(RELEASED, "Voice", "Released");
    add(UNISON_VAL, "Voice", "Unison Position");
    add(KEYTRACK_FROM_60, "Voice", "Keytrack from 60");

    add(MPE_PRESSURE, "MPE", "Pressure");
    add(MPE_TIMBRE, "MPE", "Timbre");

    add(RANDOM_01, "Random", "Rand Unif 01");
    add(RANDOM_PM1, "Random", "Rand Unif Bi");
    add(RANDOM_NORM, "Random", "Rand Normal");
    add(RANDOM_HALFNORM, "Random", "Rand HalfNorm");

    add(INTERNAL_LFO, "Voice Modulators", "LFO");
    add(INTERNAL_ENV, "Voice Modulators", "Envelope");

    std::sort(sources.begin(), sources.end(),
              [](const SourceObj &a, const SourceObj &b)
              {
                  if (a.group != b.group)
                      return a.group < b.group;
                  if (a.name != b.name)
                      return a.name < b.name;
                  return a.id < b.id;
              });

    // Within the Macros group: amplitudes first (by index), separator, then
    // modulated entries (by index). Default alphabetical sort interleaves them.
    auto isMacroAmp = [](const SourceObj &o)
    { return o.id >= MACRO_0 && o.id < MACRO_0 + (int)numMacros; };
    auto isMacroMod = [](const SourceObj &o)
    { return o.id >= MACRO_MOD_0 && o.id < MACRO_MOD_0 + (int)numMacros; };
    auto isMacro = [&](const SourceObj &o) { return isMacroAmp(o) || isMacroMod(o); };
    auto firstMacro = std::find_if(sources.begin(), sources.end(), isMacro);
    auto lastMacro =
        std::find_if(firstMacro, sources.end(), [&](const SourceObj &o) { return !isMacro(o); });
    if (firstMacro != lastMacro)
    {
        std::stable_partition(firstMacro, lastMacro, isMacroAmp);
        std::sort(firstMacro, lastMacro,
                  [](const SourceObj &a, const SourceObj &b) { return a.id < b.id; });
        auto firstMod = std::find_if(firstMacro, lastMacro, isMacroMod);
        if (firstMod != lastMacro)
            firstMod->addSeparatorBefore = true;
    }

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