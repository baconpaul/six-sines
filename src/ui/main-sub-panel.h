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
#include "ruled-label.h"

namespace baconpaul::six_sines::ui
{
struct MainSubPanel : juce::Component,
                      HasEditor,
                      DAHDSRComponents<MainSubPanel, Patch::OutputNode>,
                      ModulationComponents<MainSubPanel, Patch::OutputNode>
{
    MainSubPanel(SixSinesEditor &);
    ~MainSubPanel();
    void resized() override;

    void beginEdit() {}

    std::unique_ptr<jcmp::Knob> velSen;
    std::unique_ptr<PatchContinuous> velSenD;
    std::unique_ptr<jcmp::Label> velSenL;

    std::unique_ptr<RuledLabel> playTitle;
    std::unique_ptr<jcmp::MultiSwitch> playMode;
    std::unique_ptr<PatchDiscrete> playModeD;

    std::unique_ptr<RuledLabel> velTitle, bendTitle, uniTitle, mpeTitle, tsposeTitle;

    std::unique_ptr<jcmp::Label> bUpL, bDnL;
    std::unique_ptr<PatchContinuous> bUpD, bDnD;
    std::unique_ptr<jcmp::DraggableTextEditableValue> bUp, bDn;

    std::unique_ptr<jcmp::TextPushButton> triggerButton;
    void setTriggerButtonLabel();
    void showTriggerButtonMenu();

    std::unique_ptr<jcmp::ToggleButton> pianoModeButton;
    std::unique_ptr<PatchDiscrete> pianoModeButtonD;

    std::unique_ptr<jcmp::Knob> portaTime;
    std::unique_ptr<PatchContinuous> portaTimeD;
    std::unique_ptr<jcmp::Label> portaL;
    void setEnabledState();

    std::unique_ptr<jcmp::JogUpDownButton> uniCt;
    std::unique_ptr<PatchDiscrete> uniCtD;
    std::unique_ptr<jcmp::Label> uniCtL;

    std::unique_ptr<jcmp::ToggleButton> uniRPhase;
    std::unique_ptr<PatchDiscrete> uniRPhaseDD;

    std::unique_ptr<jcmp::Knob> uniSpread;
    std::unique_ptr<PatchContinuous> uniSpreadD;
    std::unique_ptr<jcmp::Label> uniSpreadL;

    std::unique_ptr<jcmp::ToggleButton> mpeActiveButton;
    std::unique_ptr<PatchDiscrete> mpeActiveButtonD;

    std::unique_ptr<jcmp::JogUpDownButton> mpeRange;
    std::unique_ptr<PatchDiscrete> mpeRangeD;
    std::unique_ptr<jcmp::Label> mpeRangeL;

    std::unique_ptr<jcmp::JogUpDownButton> tsposeButton;
    std::unique_ptr<PatchDiscrete> tsposeButtonD;
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_SUB_PANEL_H
