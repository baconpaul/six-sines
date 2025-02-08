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
#include "clipboard.h"

namespace baconpaul::six_sines::ui
{
struct MainPanSubPanel : juce::Component,
                         HasEditor,
                         DAHDSRComponents<MainPanSubPanel, Patch::MainPanNode>,
                         LFOComponents<MainPanSubPanel, Patch::MainPanNode>,
                         ModulationComponents<MainPanSubPanel, Patch::MainPanNode>,
                         SupportsClipboard
{
    MainPanSubPanel(SixSinesEditor &e);
    ~MainPanSubPanel() = default;

    void resized() override;
    void beginEdit() {}

    std::unique_ptr<jcmp::Knob> envDepth;
    std::unique_ptr<PatchContinuous::cubic_t> envDepthDA;
    std::unique_ptr<jcmp::Label> envDepthLL;

    std::unique_ptr<jcmp::Knob> lfoDep;
    std::unique_ptr<PatchContinuous::cubic_t> lfoDepDA;
    std::unique_ptr<jcmp::Label> lfoDepL;

    std::unique_ptr<jcmp::RuledLabel> depTitle;

    void setEnabledState();

    HAS_CLIPBOARD_SUPPORT;
};
} // namespace baconpaul::six_sines::ui
#endif // MIXER_SUB_PANE_H
