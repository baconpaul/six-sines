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

#ifndef BACONPAUL_SIX_SINES_UI_WAVEFORM_DISPLAY_H
#define BACONPAUL_SIX_SINES_UI_WAVEFORM_DISPLAY_H

#include <string>

#include "dsp/sintable.h"
#include "synth/patch.h"
#include "ui/patch-data-bindings.h"

namespace baconpaul::six_sines::ui
{

/*
 * WaveformMenuEntry describes the flat popup menu for waveform selection.
 * isSeparator=true: a visual separator (label/waveformValue ignored).
 * isSeparator=false: a selectable entry; waveformValue maps to SinTable::WaveForm integer.
 *
 * Menu order is fully independent of enum integer values — future waveforms
 * can be added here (and to the patch range) without displacing AUDIO_IN.
 */
struct WaveformMenuEntry
{
    bool isSeparator;
    const char *label;
    int waveformValue;
};

// Base waveforms shown for all operators, grouped with separators.
static constexpr WaveformMenuEntry waveformMenuBase[] = {
    {false, "Sin", SinTable::SIN},
    {false, "Sin Fifth", SinTable::SIN_FIFTH},
    {false, "Sin of Cubed", SinTable::SIN_OF_CUBED},
    {false, "Triangle", SinTable::TRIANGLE},
    {false, "Sawish", SinTable::SAWISH},
    {false, "Squarish", SinTable::SQUARISH},

    {true, "", 0},
    {false, "TX2", SinTable::TX2},
    {false, "TX3", SinTable::TX3},
    {false, "TX4", SinTable::TX4},
    {false, "TX5", SinTable::TX5},
    {false, "TX6", SinTable::TX6},
    {false, "TX7", SinTable::TX7},
    {false, "TX8", SinTable::TX8},

    {true, "", 0},
    {false, "Spiky TX2", SinTable::SPIKY_TX2},
    {false, "Spiky TX4", SinTable::SPIKY_TX4},
    {false, "Spiky TX6", SinTable::SPIKY_TX6},
    {false, "Spiky TX8", SinTable::SPIKY_TX8},

    {true, "", 0},
    {false, "Hann Window", SinTable::HANN_WINDOW},
    {false, "Blackman-Harris Window", SinTable::BLACKMAN_HARRIS_WINDOW},
    {false, "Half Blackman-Harris Window", SinTable::HALF_BLACKMAN_HARRIS_WINDOW},
    {false, "Tukey Window", SinTable::TUKEY_WINDOW},
};
static constexpr int waveformMenuBaseCount =
    static_cast<int>(sizeof(waveformMenuBase) / sizeof(waveformMenuBase[0]));

// Extra entries appended for op1 (index 0) which supports audio input.
static constexpr WaveformMenuEntry waveformMenuAudioIn[] = {
    {true, "", 0},
    {false, "Audio In", SinTable::AUDIO_IN},
};
static constexpr int waveformMenuAudioInCount =
    static_cast<int>(sizeof(waveformMenuAudioIn) / sizeof(waveformMenuAudioIn[0]));

/*
 * Return the next selectable waveform value stepping dir (+1/-1) from current,
 * wrapping at the ends. Skips separator entries.
 *
 * USER_TABLE joins the list only when the operator actually has a table loaded. There is
 * nothing to hear on an empty one, so jogging onto it would be a dead stop - but once a table
 * is loaded it has to be in the list, or jogging off it and back lands somewhere else and the
 * loaded table becomes unreachable by the jog that left it.
 */
inline int nextWaveformValue(int current, int dir, bool includeAudioIn, bool includeUserTable)
{
    // Build a flat list of selectable values in display order.
    int values[waveformMenuBaseCount + waveformMenuAudioInCount + 1];
    int count = 0;
    for (int i = 0; i < waveformMenuBaseCount; ++i)
        if (!waveformMenuBase[i].isSeparator)
            values[count++] = waveformMenuBase[i].waveformValue;
    if (includeAudioIn)
        for (int i = 0; i < waveformMenuAudioInCount; ++i)
            if (!waveformMenuAudioIn[i].isSeparator)
                values[count++] = waveformMenuAudioIn[i].waveformValue;
    // last, matching where it sits in the menu
    if (includeUserTable || current == (int)SinTable::USER_TABLE)
        values[count++] = (int)SinTable::USER_TABLE;

    // Find current position.
    int idx = 0;
    for (int i = 0; i < count; ++i)
        if (values[i] == current)
        {
            idx = i;
            break;
        }

    idx = (idx + dir + count) % count;
    return values[idx];
}

/*
 * WaveformPatchDiscrete overrides jog() and getMax() so the waveform
 * selector navigates in menu display order and respects the AUDIO_IN
 * opt-in flag without subclassing DiscreteParamMenuBuilder.
 */
struct WaveformPatchDiscrete : PatchDiscrete
{
    bool includeAudioIn{false};
    // Set by the panel so the button can name the loaded table rather than just saying
    // "Wavetable", which tells you nothing about which one you are hearing.
    const Patch::SourceNode *sourceNode{nullptr};

    WaveformPatchDiscrete(SixSinesEditor &e, uint32_t id) : PatchDiscrete(e, id) {}

    std::string getValueAsStringFor(int i) const override
    {
        if (i == (int)SinTable::USER_TABLE && sourceNode)
        {
            if (sourceNode->wavetable)
            {
                auto &n = sourceNode->wavetable->name;
                return "WT: " + (n.empty() ? std::string("(unnamed)") : n);
            }
            return "WT: Sine to Saw";
        }
        return PatchDiscrete::getValueAsStringFor(i);
    }

    void jog(int dir) override
    {
        // always: with nothing loaded the operator gets the built in table, so there is no
        // longer a dead stop to protect against
        setValueFromGUI(nextWaveformValue(getValue(), dir, includeAudioIn, true));
    }

    int getMax() const override
    {
        if (includeAudioIn)
            return static_cast<int>(SinTable::AUDIO_IN);
        // Exclude AUDIO_IN (last entry before NUM_WAVEFORMS) for ops 2–5.
        // As new synthesized waveforms are added before AUDIO_IN, this automatically
        // expands to cover them without any change here.
        return static_cast<int>(SinTable::NUM_WAVEFORMS) - 2;
    }
};

} // namespace baconpaul::six_sines::ui

// Compile-time validation: every waveform value must appear in
// waveformMenuBase exactly once, and AUDIO_IN exactly once in waveformMenuAudioIn.
static_assert(
    []() constexpr
    {
        using namespace baconpaul::six_sines;
        using namespace baconpaul::six_sines::ui;
        // Check every synthesized waveform (0..NUM_WAVEFORMS-1, excluding AUDIO_IN and
        // USER_TABLE) appears exactly once in waveformMenuBase. Future waveforms added to
        // the enum will automatically be caught here without needing to update this assert.
        //
        // USER_TABLE is excluded because it has no static table, same as AUDIO_IN. It IS
        // jogged onto and it IS in the menu - but its menu row is built by hand alongside
        // load, clear and playback, so it is not one of these plain value picks.
        for (int wf = 0; wf < static_cast<int>(SinTable::NUM_WAVEFORMS); ++wf)
        {
            if (wf == static_cast<int>(SinTable::AUDIO_IN) ||
                wf == static_cast<int>(SinTable::USER_TABLE))
                continue;
            int count = 0;
            for (int i = 0; i < waveformMenuBaseCount; ++i)
                if (!waveformMenuBase[i].isSeparator && waveformMenuBase[i].waveformValue == wf)
                    ++count;
            if (count != 1)
                return false;
        }
        return true;
    }(),
    "waveformMenuBase must contain each synthesized waveform (all except AUDIO_IN) exactly once");

static_assert(
    []() constexpr
    {
        using namespace baconpaul::six_sines;
        using namespace baconpaul::six_sines::ui;
        int count = 0;
        for (int i = 0; i < waveformMenuAudioInCount; ++i)
            if (!waveformMenuAudioIn[i].isSeparator &&
                waveformMenuAudioIn[i].waveformValue == static_cast<int>(SinTable::AUDIO_IN))
                ++count;
        return count == 1;
    }(),
    "waveformMenuAudioIn must contain AUDIO_IN exactly once");

#endif // BACONPAUL_SIX_SINES_UI_WAVEFORM_DISPLAY_H
