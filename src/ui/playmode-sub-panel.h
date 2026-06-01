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

#ifndef BACONPAUL_SIX_SINES_UI_PLAYMODE_SUB_PANEL_H
#define BACONPAUL_SIX_SINES_UI_PLAYMODE_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/component-adapters/DiscreteToReference.h>
#include <sst/jucegui/component-adapters/ContinuousToReference.h>
#include <sst/jucegui/components/TextEditor.h>
#include <sst/jucegui/components/JogUpDownButton.h>
#include <sst/jucegui/components/HSliderFilled.h>
#include "six-sines-editor.h"
#include "dahdsr-components.h"
#include "lfo-components.h"
#include "modulation-components.h"

namespace baconpaul::six_sines::ui
{
// A JogUpDownButton whose arrows / wheel / menu step through a fixed set of values (the
// voice-limit choices) rather than every integer in the range. Defined in the .cpp.
struct SteppedJogButton;

// As with the base JogUpDownButton, the value is shown in the label so a tooltip is redundant.
template <> constexpr bool suppressTooltipByWidget<SteppedJogButton>() { return true; }

struct PlayModeSubPanel : juce::Component, HasEditor
{
    PlayModeSubPanel(SixSinesEditor &e);
    ~PlayModeSubPanel();

    void resized() override;

    void beginEdit() {}

    std::unique_ptr<jcmp::RuledLabel> playTitle;
    std::unique_ptr<jcmp::MultiSwitch> playMode;
    std::unique_ptr<PatchDiscrete> playModeD;

    std::unique_ptr<jcmp::RuledLabel> bendTitle, uniTitle, tsposeTitle, panicTitle, mtsTitle;
    std::unique_ptr<jcmp::Label> mtsStatusLabel;
    int mtsIdleCount{0};
    bool mtsConnected{false};
    void updateMTSStatus();

    std::unique_ptr<jcmp::Label> bUpL, bDnL;
    std::unique_ptr<PatchContinuous> bUpD, bDnD;
    std::unique_ptr<jcmp::DraggableTextEditableValue> bUp, bDn;

    std::unique_ptr<jcmp::TextPushButton> triggerButton;
    void setTriggerButtonLabel();
    void showTriggerButtonMenu();

    std::unique_ptr<jcmp::ToggleButton> pianoModeButton;
    std::unique_ptr<PatchDiscrete> pianoModeButtonD;

    std::unique_ptr<jcmp::HSliderFilled> portaTime;
    std::unique_ptr<PatchContinuous> portaTimeD;
    std::unique_ptr<jcmp::Label> portaL;
    std::unique_ptr<jcmp::TextPushButton> portaContinuationButton;
    void setPortaContinuationLabel();
    void showPortaContinuationMenu();

    void setEnabledState();

    std::unique_ptr<jcmp::JogUpDownButton> uniCt;
    std::unique_ptr<PatchDiscrete> uniCtD;
    std::unique_ptr<jcmp::Label> uniCtL;

    std::unique_ptr<jcmp::ToggleButton> uniRPhase;
    std::unique_ptr<PatchDiscrete> uniRPhaseDD;

    std::unique_ptr<jcmp::HSliderFilled> uniSpread, uniPan;
    std::unique_ptr<PatchContinuous::cubic_t> uniSpreadD;
    std::unique_ptr<PatchContinuous> uniPanD;
    std::unique_ptr<jcmp::GlyphPainter> uniSpreadG, uniPanG;

    // MPE controls bind to engine-instance state (Synth::DawExtraState mirror on the editor),
    // not to patch params. DiscreteToValueReference owns the widget pointer; the JogUpDown
    // variant is subclassed in the .cpp to set its 1..96 range.
    std::unique_ptr<
        sst::jucegui::component_adapters::DiscreteToValueReference<jcmp::ToggleButton, bool>>
        mpeActiveButtonD;

    std::unique_ptr<jcmp::TextEditor> mpeRangeEditor;
    std::unique_ptr<jcmp::Label> mpeRangeL;
    void refreshMpeRangeEditor();
    void commitMpeRangeEditor();

    // "MPE + Smoothing" settings section below the top row.
    std::unique_ptr<jcmp::RuledLabel> smoothingSectionTitle;
    std::unique_ptr<jcmp::Label> mpeRowLabel, smoothingRowLabel, paramSmoothingRowLabel;
    // ContinuousToValueReference owns the HSliderFilled (access via ->widget) and binds it to
    // the ms smoothing floats in editorDawExtraState, which are not patch params.
    std::unique_ptr<
        sst::jucegui::component_adapters::ContinuousToValueReference<jcmp::HSliderFilled>>
        midiSmoothingSliderD, paramSmoothingSliderD;

    std::unique_ptr<jcmp::JogUpDownButton> tsposeButton;
    std::unique_ptr<PatchDiscrete> tsposeButtonD;

    std::unique_ptr<SteppedJogButton> voiceLimit;
    std::unique_ptr<PatchDiscrete> voiceLimitD;
    std::unique_ptr<jcmp::RuledLabel> voiceLimitL;

    std::unique_ptr<jcmp::TextPushButton> panicButton;

    std::unique_ptr<jcmp::RuledLabel> outputControlTitle;

    std::unique_ptr<jcmp::JogUpDownButton> srStrat;
    std::unique_ptr<PatchDiscrete> srStratD;
    std::unique_ptr<jcmp::JogUpDownButton> rsEng;
    std::unique_ptr<PatchDiscrete> rsEngD;

    std::unique_ptr<jcmp::JogUpDownButton> satType, lowpass, bitRate, bitDepth, highpass;
    std::unique_ptr<PatchDiscrete> satTypeD, lowpassD, bitRateD, bitDepthD, highpassD;

    std::unique_ptr<jcmp::HSliderFilled> satDrive;
    std::unique_ptr<PatchContinuous> satDriveD;

    std::unique_ptr<jcmp::HSliderFilled> outGain;
    std::unique_ptr<PatchContinuous> outGainD;
    std::unique_ptr<jcmp::Label> outGainLabel;

    std::unique_ptr<jcmp::Label> sampleRateLabel, downsamplerLabel;
    std::unique_ptr<jcmp::Label> saturationLabel, lowpassLabel, bitRateLabel, bitDepthLabel,
        highpassLabel;

    void showPolyLimitMenu();
};
} // namespace baconpaul::six_sines::ui
#endif // MIXER_SUB_PANE_H
