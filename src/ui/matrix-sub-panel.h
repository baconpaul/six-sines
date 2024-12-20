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

#ifndef BACONPAUL_SIX_SINES_UI_MATRIX_SUB_PANEL_H
#define BACONPAUL_SIX_SINES_UI_MATRIX_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "six-sines-editor.h"
#include "dahdsr-components.h"

namespace baconpaul::six_sines::ui
{
struct MatrixSubPanel : juce::Component, HasEditor
{
    MatrixSubPanel(SixSinesEditor &);
    ~MatrixSubPanel();
    void paint(juce::Graphics &g)
    {
        g.fillAll(juce::Colours::darkred);

        g.setFont(juce::FontOptions(40));
        g.setColour(juce::Colours::white);
        g.drawText(std::to_string(index), getLocalBounds(), juce::Justification::centred);
    }

    size_t index{0};
    void setSelectedIndex(int idx);
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_SUB_PANEL_H
