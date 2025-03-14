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

#ifndef BACONPAUL_SIX_SINES_UI_SELF_SUB_PANEL_H
#define BACONPAUL_SIX_SINES_UI_SELF_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "six-sines-editor.h"
#include "dahdsr-components.h"
#include "lfo-components.h"
#include "modulation-components.h"
#include "clipboard.h"

namespace baconpaul::six_sines::ui
{
struct SelfSubPanel : juce::Component,
                      HasEditor,
                      DAHDSRComponents<SelfSubPanel, Patch::SelfNode>,
                      LFOComponents<SelfSubPanel, Patch::SelfNode>,
                      ModulationComponents<SelfSubPanel, Patch::SelfNode>,
                      SupportsClipboard
{
    SelfSubPanel(SixSinesEditor &);
    ~SelfSubPanel();

    void resized() override;
    void beginEdit() {}

    size_t index{0};
    void setSelectedIndex(int idx);

    std::unique_ptr<jcmp::Knob> lfoToFb;
    std::unique_ptr<PatchContinuous::cubic_t> lfoToFbDA;
    std::unique_ptr<jcmp::Label> lfoToFbL;

    std::unique_ptr<jcmp::RuledLabel> modLabelE, modLabelL;
    std::unique_ptr<jcmp::Knob> envToLev;
    std::unique_ptr<PatchContinuous::cubic_t> envToLevDA;
    std::unique_ptr<jcmp::Label> envToLevL;

    std::unique_ptr<jcmp::MultiSwitch> envMul;
    std::unique_ptr<PatchDiscrete> envMulD;

    std::unique_ptr<jcmp::ToggleButton> overdrive;
    std::unique_ptr<PatchDiscrete> overdriveD;
    std::unique_ptr<jcmp::RuledLabel> overdriveTitle;

    void setEnabledState();

    HAS_CLIPBOARD_SUPPORT;
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_SUB_PANEL_H
