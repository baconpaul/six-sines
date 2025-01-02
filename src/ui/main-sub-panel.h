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

#ifndef BACONPAUL_SIX_SINES_UI_MAIN_SUB_PANEL_H
#define BACONPAUL_SIX_SINES_UI_MAIN_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "sst/jucegui/components/Knob.h"
#include "sst/jucegui/components/MultiSwitch.h"
#include "sst/jucegui/components/DraggableTextEditableValue.h"
#include "sst/jucegui/components/TextPushButton.h"
#include "sst/jucegui/components/JogUpDownButton.h"

#include "patch-data-bindings.h"
#include "six-sines-editor.h"
#include "dahdsr-components.h"
#include "modulation-components.h"
#include "lfo-components.h"
#include "ruled-label.h"

namespace baconpaul::six_sines::ui
{
struct MainSubPanel : juce::Component,
                      HasEditor,
                      DAHDSRComponents<MainSubPanel, Patch::OutputNode>,
                      LFOComponents<MainSubPanel, Patch::OutputNode>,
                      ModulationComponents<MainSubPanel, Patch::OutputNode>
{
    MainSubPanel(SixSinesEditor &);
    ~MainSubPanel();
    void resized() override;

    void beginEdit() {}

    std::unique_ptr<jcmp::Knob> velSen;
    std::unique_ptr<PatchContinuous> velSenD;
    std::unique_ptr<jcmp::Label> velSenL;

    std::unique_ptr<jcmp::Knob> lfoDep;
    std::unique_ptr<PatchContinuous::cubic_t> lfoDepDA;

    std::unique_ptr<jcmp::Label> lfoDepL;

    std::unique_ptr<RuledLabel> velTitle;
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_SUB_PANEL_H
