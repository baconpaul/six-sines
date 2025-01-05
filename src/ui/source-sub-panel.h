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

#ifndef BACONPAUL_SIX_SINES_UI_SOURCE_SUB_PANEL_H
#define BACONPAUL_SIX_SINES_UI_SOURCE_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "sst/jucegui/components/JogUpDownButton.h"
#include "sst/jucegui/components/HSliderFilled.h"
#include "six-sines-editor.h"
#include "dahdsr-components.h"
#include "lfo-components.h"
#include "modulation-components.h"
#include "sst/jucegui/components/RuledLabel.h"

namespace baconpaul::six_sines::ui
{
struct SourceSubPanel : juce::Component,
                        HasEditor,
                        DAHDSRComponents<SourceSubPanel, Patch::SourceNode>,
                        ModulationComponents<SourceSubPanel, Patch::SourceNode>,
                        LFOComponents<SourceSubPanel, Patch::SourceNode>
{
    SourceSubPanel(SixSinesEditor &);
    ~SourceSubPanel();

    void resized() override;

    size_t index{0};
    void setSelectedIndex(size_t i);

    void beginEdit() {}

    std::unique_ptr<jcmp::Knob> envToRatio;
    std::unique_ptr<PatchContinuous> envToRatioD;
    std::unique_ptr<jcmp::Knob> envToRatioFine;
    std::unique_ptr<PatchContinuous> envToRatioFineD;
    std::unique_ptr<jcmp::Label> envToRatioL;
    std::unique_ptr<jcmp::Label> envToRatioFineL;

    std::unique_ptr<jcmp::Knob> lfoToRatio;
    std::unique_ptr<PatchContinuous> lfoToRatioD;
    std::unique_ptr<jcmp::Knob> lfoToRatioFine;
    std::unique_ptr<PatchContinuous> lfoToRatioFineD;
    std::unique_ptr<jcmp::Label> lfoToRatioL;
    std::unique_ptr<jcmp::Label> lfoToRatioFineL;

    std::unique_ptr<jcmp::JogUpDownButton> wavButton;
    std::unique_ptr<PatchDiscrete> wavButtonD;

    std::unique_ptr<jcmp::RuledLabel> modTitle, lfoModTitle, wavTitle, keyTrackTitle;

    std::unique_ptr<juce::Component> wavPainter;

    std::unique_ptr<jcmp::ToggleButton> keyTrack;
    std::unique_ptr<PatchDiscrete> keyTrackD;

    std::unique_ptr<jcmp::Knob> keyTrackValue;
    std::unique_ptr<PatchContinuous> keyTrackValueD;
    std::unique_ptr<jcmp::Label> keyTrackValueLL;

    std::unique_ptr<jcmp::HSliderFilled> startingPhase;
    std::unique_ptr<PatchContinuous> startingPhaseD;
    std::unique_ptr<jcmp::Label> startingPhaseL;

    std::unique_ptr<jcmp::JogUpDownButton> tsposeButton;
    std::unique_ptr<PatchDiscrete> tsposeButtonD;
    std::unique_ptr<jcmp::Label> tsposeButtonL;

    void setEnabledState();
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_SUB_PANEL_H
