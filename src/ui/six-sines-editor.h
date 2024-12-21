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

#ifndef BACONPAUL_SIX_SINES_UI_SIX_SINES_EDITOR_H
#define BACONPAUL_SIX_SINES_UI_SIX_SINES_EDITOR_H

#include <functional>
#include <utility>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sst/jucegui/components/NamedPanel.h>
#include <sst/jucegui/components/WindowPanel.h>
#include <sst/jucegui/components/Knob.h>
#include <sst/jucegui/components/ToolTip.h>
#include <sst/jucegui/components/ToggleButton.h>
#include <sst/jucegui/data/Continuous.h>

#include "synth/synth.h"

namespace jcmp = sst::jucegui::components;
namespace jdat = sst::jucegui::data;

namespace baconpaul::six_sines::ui
{
struct MainPanel;
struct MainSubPanel;
struct MatrixPanel;
struct MatrixSubPanel;
struct SelfSubPanel;
struct MixerPanel;
struct MixerSubPanel;
struct SourcePanel;
struct SourceSubPanel;

struct SixSinesEditor : jcmp::WindowPanel
{
    Patch patchCopy;
    Synth::audioToUIQueue_t &audioToUI;
    Synth::uiToAudioQueue_T &uiToAudio;
    std::function<void()> flushOperator;
    SixSinesEditor(Synth::audioToUIQueue_t &atou, Synth::uiToAudioQueue_T &utoa,
                   std::function<void()> flushOperator);
    virtual ~SixSinesEditor();

    void paint(juce::Graphics &g) override;
    void resized() override;

    void idle();
    std::unique_ptr<juce::Timer> idleTimer;

    std::unique_ptr<jcmp::NamedPanel> singlePanel;

    std::unique_ptr<MainPanel> mainPanel;
    std::unique_ptr<MainSubPanel> mainSubPanel;

    std::unique_ptr<MatrixPanel> matrixPanel;
    std::unique_ptr<MatrixSubPanel> matrixSubPanel;
    std::unique_ptr<SelfSubPanel> selfSubPanel;

    std::unique_ptr<MixerPanel> mixerPanel;
    std::unique_ptr<MixerSubPanel> mixerSubPanel;

    std::unique_ptr<SourcePanel> sourcePanel;
    std::unique_ptr<SourceSubPanel> sourceSubPanel;

    std::unique_ptr<jcmp::ToolTip> toolTip;
    void showTooltipOn(juce::Component *c);
    void updateTooltip(jdat::Continuous *c);
    void updateTooltip(jdat::Discrete *d);
    void hideTooltip();

    void hideAllSubPanels();
    std::unordered_map<uint32_t, juce::Component::SafePointer<juce::Component>> componentByID;
};

struct HasEditor
{
    SixSinesEditor &editor;
    HasEditor(SixSinesEditor &e) : editor(e) {}
};
} // namespace baconpaul::six_sines::ui
#endif
