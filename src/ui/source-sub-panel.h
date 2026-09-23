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

#include <string>
#include <utility>
#include <vector>
#include "sst/jucegui/components/JogUpDownButton.h"
#include "sst/jucegui/components/HSliderFilled.h"
#include "sst/jucegui/components/MultiSwitch.h"
#include "sst/jucegui/components/Knob.h"
#include "six-sines-editor.h"
#include "dahdsr-components.h"
#include "lfo-components.h"
#include "modulation-components.h"
#include "sst/jucegui/components/RuledLabel.h"
#include "sst/jucegui/components/TextPushButton.h"
#include "sst/jucegui/components/GlyphButton.h"
#include "sst/jucegui/components/LineSegment.h"
#include "filesystem/import.h"
#include "clipboard.h"
#include "waveform-display.h"

namespace baconpaul::six_sines::ui
{
struct SourceSubPanel : juce::Component,
                        HasEditor,
                        DAHDSRComponents<SourceSubPanel, Patch::SourceNode>,
                        ModulationComponents<SourceSubPanel, Patch::SourceNode>,
                        LFOComponents<SourceSubPanel, Patch::SourceNode>,
                        SupportsClipboard
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
    std::unique_ptr<WaveformPatchDiscrete> wavButtonD;

    std::unique_ptr<jcmp::RuledLabel> modTitle, lfoModTitle, wavTitle, keyTrackTitle;

    std::unique_ptr<juce::Component> wavPainter;

    std::unique_ptr<jcmp::ToggleButton> keyTrack;
    std::unique_ptr<PatchDiscrete> keyTrackD;

    std::unique_ptr<jcmp::ToggleButton> keyTrackLow;
    std::unique_ptr<PatchDiscrete> keyTrackLowD;

    std::unique_ptr<jcmp::MenuButton> unisonBehaviorB;

    std::unique_ptr<jcmp::LineSegment> extModeRuleLeft, extModeRuleRight;
    std::unique_ptr<jcmp::Label> extModeLabel;
    std::unique_ptr<jcmp::JogUpDownButton> extModeButton;
    std::unique_ptr<PatchDiscrete> extModeButtonD;

    // Extended-mode body components (visibility driven by extModeButton's value)
    std::unique_ptr<jcmp::Label> comingSoonLabel;
    std::unique_ptr<jcmp::MultiSwitch> phaseMapShape;
    std::unique_ptr<PatchDiscrete> phaseMapShapeD;
    std::unique_ptr<jcmp::HSliderFilled> morph;
    std::unique_ptr<jcmp::Knob> envToMorph, lfoToMorph;
    std::unique_ptr<PatchContinuous> morphD;
    std::unique_ptr<PatchContinuous::cubic_t> envToMorphD, lfoToMorphD;
    std::unique_ptr<jcmp::Label> morphL, envToMorphL, lfoToMorphL;

    void showWavetableLoadDialog();
    // Step to the next or previous loadable file in the folder the current table came from.
    void jogWavetableFile(int dir);
    juce::PopupMenu buildPlaybackMenu();

    // Overlaid on the wave display: playback options top left, and arrows that step through
    // the folder the table came from top right.
    std::unique_ptr<jcmp::GlyphButton> wtPlaybackButton, wtJogPrev, wtJogNext;
    void loadWavetableFile(const fs::path &);
    // One entry per installed synth, in discovery order, for the caller to add at whatever
    // level it wants. Empty when none of them are on this machine.
    static std::vector<std::pair<std::string, juce::PopupMenu>>
    buildVendorFactoryMenus(SourceSubPanel *);
    void clearWavetable();
    bool hasWavetable() const;

    std::unique_ptr<jcmp::Knob> extM, envToExtM, lfoToExtM;
    std::unique_ptr<PatchContinuous> extMD;
    // Bipolar mod-depth knobs get a cubic throw so small depths get more of the
    // throw (fine control near zero), matching the matrix panel's depth knobs.
    std::unique_ptr<PatchContinuous::cubic_t> envToExtMD, lfoToExtMD;
    std::unique_ptr<jcmp::Label> extML, envToExtML, lfoToExtML;
    std::unique_ptr<jcmp::Knob> extN, envToExtN, lfoToExtN;
    std::unique_ptr<PatchContinuous> extND;
    std::unique_ptr<PatchContinuous::cubic_t> envToExtND, lfoToExtND;
    std::unique_ptr<jcmp::Label> extNL, envToExtNL, lfoToExtNL;
    std::unique_ptr<jcmp::HSliderFilled> phaseMapReadPhase;
    std::unique_ptr<PatchContinuous> phaseMapReadPhaseD;
    std::unique_ptr<jcmp::Label> phaseMapReadPhaseL;
    std::unique_ptr<jcmp::TextPushButton> readPhaseZero, readPhaseQuarter, readPhaseHalf;
    std::unique_ptr<juce::Component> pdWavPainter;

    // Resonant sweep body components
    std::unique_ptr<jcmp::MultiSwitch> resonantWindowShape;
    std::unique_ptr<PatchDiscrete> resonantWindowShapeD;
    std::unique_ptr<jcmp::JogUpDownButton> resonantSweepDepth;
    std::unique_ptr<PatchDiscrete> resonantSweepDepthD;
    std::unique_ptr<jcmp::Label> resonantSweepDepthL;
    std::unique_ptr<juce::Component> resSweepPainter;

    // Noise body components
    std::unique_ptr<jcmp::JogUpDownButton> noiseMode;
    std::unique_ptr<PatchDiscrete> noiseModeD;
    std::unique_ptr<jcmp::Label> noiseModeL;
    std::unique_ptr<jcmp::JogUpDownButton> noiseType;
    std::unique_ptr<PatchDiscrete> noiseTypeD;
    std::unique_ptr<jcmp::Label> noiseTypeL;
    std::unique_ptr<jcmp::JogUpDownButton> lfsrMode;
    std::unique_ptr<PatchDiscrete> lfsrModeD;
    std::unique_ptr<jcmp::Label> lfsrModeL;
    std::unique_ptr<juce::Component> noisePainter;

    void setExtendedModeVisibility();

    std::unique_ptr<jcmp::HSliderFilled> keyTrackValue;
    std::unique_ptr<PatchContinuous> keyTrackValueD;

    std::unique_ptr<jcmp::HSliderFilled> keyTrackLowValue;
    std::unique_ptr<PatchContinuous::cubic_t> keyTrackLowValueD;

    std::unique_ptr<jcmp::HSliderFilled> absoluteOffset;
    std::unique_ptr<PatchContinuous::cubic_t> absoluteOffsetD;
    std::unique_ptr<jcmp::Label> absoluteOffsetL;

    std::unique_ptr<jcmp::HSliderFilled> startingPhase;
    std::unique_ptr<PatchContinuous> startingPhaseD;
    std::unique_ptr<jcmp::Label> startingPhaseL;

    std::unique_ptr<jcmp::JogUpDownButton> tsposeButton;
    std::unique_ptr<PatchDiscrete> tsposeButtonD;
    std::unique_ptr<jcmp::Label> tsposeButtonL;

    void setEnabledState();
    void showWaveformPopup();
    void showUnisonFeaturesMenu();

    HAS_CLIPBOARD_SUPPORT;
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_SUB_PANEL_H
