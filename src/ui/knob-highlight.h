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

#ifndef BACONPAUL_SIX_SINES_UI_KNOB_HIGHLIGHT_H
#define BACONPAUL_SIX_SINES_UI_KNOB_HIGHLIGHT_H

#include "six-sines-editor.h"

namespace baconpaul::six_sines::ui
{
struct KnobHighlight : public juce::Component, HasEditor
{
    KnobHighlight(SixSinesEditor &e) : HasEditor(e)
    {
        setAccessible(false);
        setWantsKeyboardFocus(false);
        setInterceptsMouseClicks(false, false);
    }
    void paint(juce::Graphics &g) override
    {
        auto valCol = editor.style()->getColour(jcmp::base_styles::ValueBearing::styleClass,
                                                jcmp::base_styles::ValueBearing::value);
        g.setColour(valCol.withAlpha(0.3f));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(0.5), 4);

        g.setColour(valCol);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5), 4, 1);
    }
};
} // namespace baconpaul::six_sines::ui
#endif // KNOB_HIGHLIGHT_H
