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

#include "tuning.h"

#include <cmath>
#include "Tunings.h"
#include "libMTSClient.h"

namespace baconpaul::six_sines
{
float Tuning::retuningInSemitones(int key, int channel) const
{
    key = std::clamp(key, 0, RetuneTable::numKeys - 1);
    switch (mode)
    {
    case TM_MTS:
        if (mtsHasMaster)
            return (float)MTS_RetuningInSemitones(mtsClient, (char)key, (char)channel);
        break;
    case TM_SCL_KBM:
        if (sclKbm.valid)
            return sclKbm.semitones[key];
        break;
    case TM_12TET:
        break;
    }
    return 0.f;
}

bool buildRetuneTable(const std::string &scl, const std::string &kbm, RetuneTable &out,
                      std::string &errorOut)
{
    if (scl.empty())
    {
        errorOut = "No scale (SCL) provided.";
        return false;
    }

    try
    {
        auto scale = Tunings::parseSCLData(scl);
        auto haveKbm = !kbm.empty();
        Tunings::KeyboardMapping mapping;
        if (haveKbm)
            mapping = Tunings::parseKBMData(kbm);

        // voices interpolate between neighbouring keys, so a skipped key must not hold junk
        auto tuning = (haveKbm ? Tunings::Tuning(scale, mapping) : Tunings::Tuning(scale))
                          .withSkippedNotesInterpolated();

        RetuneTable res;
        for (int k = 0; k < RetuneTable::numKeys; ++k)
        {
            auto v = tuning.retuningFromEqualInSemitonesForMidiNote(k);
            if (!std::isfinite(v))
            {
                errorOut = "Scale produced a non-finite pitch at midi note " + std::to_string(k);
                return false;
            }
            res.semitones[k] = (float)v;
        }
        res.valid = true;
        out = res;
        return true;
    }
    catch (const std::exception &e)
    {
        errorOut = e.what();
        return false;
    }
}

bool publishSclKbm(const std::string &scl, const std::string &kbm, TuningHandoff &handoff,
                   std::string &errorOut)
{
    if (scl.empty() && kbm.empty())
    {
        handoff.writeBuffer() = RetuneTable{};
        handoff.publish();
        return true;
    }

    RetuneTable t;
    if (!buildRetuneTable(scl.empty() ? twelveTETSclText() : scl, kbm, t, errorOut))
        return false;

    handoff.writeBuffer() = t;
    handoff.publish();
    return true;
}

std::string twelveTETSclText()
{
    try
    {
        return Tunings::evenTemperament12NoteScale().rawText;
    }
    catch (const std::exception &)
    {
        return {};
    }
}

// spelled out because KeyboardMapping().rawText rounds the reference to 261.626
std::string defaultKbmText()
{
    return R"KBM(! Default keyboard mapping
!
! Size of map. 0 is a linear map of the scale onto the keyboard
0
! First and last MIDI note to map
0
127
! Middle note, where the scale begins
60
! Reference note and its frequency
60
261.6255653
! Scale degree for the formal octave. 0 means the size of the scale
0
)KBM";
}

std::string sclKbmDisplayName(const std::string &scl, const std::string &kbm)
{
    if (scl.empty())
        return kbm.empty() ? std::string() : std::string("12-TET (KBM)");

    try
    {
        auto s = Tunings::parseSCLData(scl);
        return s.description.empty() ? std::to_string(s.count) + " note scale" : s.description;
    }
    catch (const std::exception &)
    {
        return "Invalid scale";
    }
}

} // namespace baconpaul::six_sines
