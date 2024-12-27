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
    : off(*this, 0, "Off"),
      midiCCs(sst::cpputils::make_array_lambda<Source, 128>(
          [this](int i) { return Source(*this, i + 400, "CC " + std::to_string(i + 1)); })),
      channelAT(*this, 100, "Channel AT"),
      macros(sst::cpputils::make_array_lambda<Source, numMacros>(
          [this](int i) { return Source(*this, i + 200, "Macro " + std::to_string(i + 1)); })),
      velocity(*this, voiceLevel + 1, "Velocity"),

      polyAT(*this, voiceLevel + 10, "PolyAT"),

      gated(*this, voiceLevel + 50, "Gated"), released(*this, voiceLevel + 51, "Released")
{
}

ModMatrixConfig::Source::Source(ModMatrixConfig &, uint32_t id, const std::string &v) {}

} // namespace baconpaul::six_sines