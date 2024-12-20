/*
 * BaconPaul's FM Atrocity
 *
 * A mess, with FM.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#ifndef BACONPAUL_FMTHING_UI_MIXER_SUB_PANEL_H
#define BACONPAUL_FMTHING_UI_MIXER_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "ifm-editor.h"
#include "dahdsr-components.h"

namespace baconpaul::fm::ui
{
struct MixerSubPanel : juce::Component, HasEditor, DAHDSRComponents<MixerSubPanel, Patch::MixerNode>
{
    MixerSubPanel(IFMEditor &);
    ~MixerSubPanel();
    void paint(juce::Graphics &g) override
    {
        g.setFont(juce::FontOptions(40));
        g.setColour(juce::Colours::white);
        g.drawText(std::to_string(index), getLocalBounds(), juce::Justification::centred);
    }
    void resized() override;

    size_t index{0};
    void setSelectedIndex(int i);
};
} // namespace baconpaul::fm::ui
#endif // MIXER_SUB_PANE_H
