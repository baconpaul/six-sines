/*
 * Six Sines A Sinnin'
 *
 * A mess, with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#ifndef BACONPAUL_SIX_SINES_UI_SOURCE_SUB_PANEL_H
#define BACONPAUL_SIX_SINES_UI_SOURCE_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "six-sines-editor.h"

namespace baconpaul::six_sines::ui
{
struct SourceSubPanel : juce::Component, HasEditor
{
    SourceSubPanel(SixSinesEditor &);
    ~SourceSubPanel();
    void paint(juce::Graphics &g)
    {
        g.fillAll(juce::Colours::orange);
        g.setFont(juce::FontOptions(40));
        g.setColour(juce::Colours::red);
        g.drawText(std::to_string(index), getLocalBounds(), juce::Justification::centred);
    }

    size_t index{0};
    void setIndex(size_t i)
    {
        index = i;
        repaint();
    }
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_SUB_PANEL_H
