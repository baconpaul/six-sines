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

#ifndef BACONPAUL_SIX_SINES_UI_MAINPAN_SUB_PANEL_H
#define BACONPAUL_SIX_SINES_UI_MAINPAN_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "six-sines-editor.h"
#include "dahdsr-components.h"
#include "lfo-components.h"
#include "modulation-components.h"

namespace baconpaul::six_sines::ui
{
struct MainPanSubPanel : juce::Component, HasEditor
// DAHDSRComponents<MixerSubPanel, Patch::MixerNode>,
// LFOComponents<MixerSubPanel, Patch::MixerNode>,
// ModulationComponents<MixerSubPanel, Patch::MixerNode>
{
    MainPanSubPanel(SixSinesEditor &e) : HasEditor(e) {}
    ~MainPanSubPanel() = default;

    void paint(juce::Graphics &g)
    {
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(40));
        g.drawText("Main Pan Screen Soon", getLocalBounds(), juce::Justification::centred);
    }
};
} // namespace baconpaul::six_sines::ui
#endif // MIXER_SUB_PANE_H
