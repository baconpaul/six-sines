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

#ifndef BACONPAUL_SIX_SINES_UI_MAIN_SUB_PANEL_H
#define BACONPAUL_SIX_SINES_UI_MAIN_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "six-sines-editor.h"
#include "dahdsr-components.h"

namespace baconpaul::six_sines::ui
{
struct MainSubPanel : juce::Component, HasEditor, DAHDSRComponents<MainSubPanel, Patch::OutputNode>
{
    MainSubPanel(SixSinesEditor &);
    ~MainSubPanel();
    void resized() override;
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_SUB_PANEL_H
