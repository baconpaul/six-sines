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

#ifndef BACONPAUL_FMTHING_UI_MAIN_SUB_PANEL_H
#define BACONPAUL_FMTHING_UI_MAIN_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "ifm-editor.h"

namespace baconpaul::fm::ui
{
struct MainSubPanel : juce::Component, HasEditor
{
    MainSubPanel(IFMEditor &);
    ~MainSubPanel();
    void paint(juce::Graphics &g) { g.fillAll(juce::Colours::orchid); }
};
} // namespace baconpaul::fm::ui
#endif // MAIN_SUB_PANEL_H
