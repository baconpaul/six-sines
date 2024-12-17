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

#include "ifm-editor.h"
#include "patch-continuous.h"
#include "main-panel.h"
#include "main-sub-panel.h"
#include "matrix-panel.h"
#include "matrix-sub-panel.h"
#include "mixer-panel.h"
#include "mixer-sub-panel.h"
#include "source-panel.h"
#include "source-sub-panel.h"

namespace baconpaul::fm::ui
{
struct IdleTimer : juce::Timer
{
    IFMEditor &editor;
    IdleTimer(IFMEditor &e) : editor(e) {}
    void timerCallback() override { editor.idle(); }
};

IFMEditor::IFMEditor(Synth::audioToUIQueue_t &atou, Synth::uiToAudioQueue_T &utoa,
                     std::function<void()> fo)
    : audioToUI(atou), uiToAudio(utoa), flushOperator(fo)
{
    sst::jucegui::style::StyleSheet::initializeStyleSheets([]() {});

    setStyle(sst::jucegui::style::StyleSheet::getBuiltInStyleSheet(
        sst::jucegui::style::StyleSheet::LIGHT));

    matrixPanel = std::make_unique<MatrixPanel>(*this);
    mixerPanel = std::make_unique<MixerPanel>(*this);
    singlePanel = std::make_unique<jcmp::NamedPanel>("Edit");
    sourcePanel = std::make_unique<SourcePanel>(*this);
    mainPanel = std::make_unique<MainPanel>(*this);
    addAndMakeVisible(*matrixPanel);
    addAndMakeVisible(*mixerPanel);
    addAndMakeVisible(*singlePanel);
    addAndMakeVisible(*sourcePanel);
    addAndMakeVisible(*mainPanel);

    mainSubPanel = std::make_unique<MainSubPanel>(*this);
    singlePanel->addChildComponent(*mainSubPanel);
    matrixSubPanel = std::make_unique<MatrixSubPanel>(*this);
    singlePanel->addChildComponent(*matrixSubPanel);
    mixerSubPanel = std::make_unique<MixerSubPanel>(*this);
    singlePanel->addChildComponent(*mixerSubPanel);
    sourceSubPanel = std::make_unique<SourceSubPanel>(*this);
    singlePanel->addChildComponent(*sourceSubPanel);

    auto startMsg = Synth::UIToAudioMsg{Synth::UIToAudioMsg::REQUEST_REFRESH};
    uiToAudio.push(startMsg);
    flushOperator();

    idleTimer = std::make_unique<IdleTimer>(*this);
    idleTimer->startTimer(1000. / 60.);

    setSize(800, 920);

    auto q = std::make_unique<PatchContinuous>(*this, patchCopy.output.level.meta.id);
}
IFMEditor::~IFMEditor() { idleTimer->stopTimer(); }

void IFMEditor::idle()
{
    auto aum = audioToUI.pop();
    while (aum.has_value())
    {
        if (aum->action == Synth::AudioToUIMsg::UPDATE_PARAM)
        {
            patchCopy.paramMap[aum->paramId]->value = aum->value;
            auto pit = componentByID.find(aum->paramId);
            if (pit != componentByID.end() && pit->second)
                pit->second->repaint();
        }
        aum = audioToUI.pop();
    }
}

void IFMEditor::resized()
{
    auto rdx{1};

    auto tp = 100;

    auto rb = getLocalBounds().withTrimmedTop(tp);
    auto edH = 300;
    auto mp = rb.withTrimmedBottom(edH);
    mp = mp.withWidth(mp.getHeight());

    matrixPanel->setBounds(mp.reduced(rdx));

    auto sp = rb.withTrimmedBottom(mp.getHeight());
    sp = sp.translated(0, mp.getHeight());
    singlePanel->setBounds(sp.reduced(rdx));
    mainSubPanel->setBounds(singlePanel->getContentArea());
    matrixSubPanel->setBounds(singlePanel->getContentArea());
    mixerSubPanel->setBounds(singlePanel->getContentArea());
    sourceSubPanel->setBounds(singlePanel->getContentArea());

    auto mx = rb.withTrimmedBottom(edH).withTrimmedLeft(mp.getWidth());
    mixerPanel->setBounds(mx.reduced(rdx));

    auto ta = getLocalBounds().withHeight(tp);
    auto sr = ta.withWidth(mp.getWidth());
    sourcePanel->setBounds(sr.reduced(rdx));

    auto mn = ta.withTrimmedLeft(sr.getWidth());
    mainPanel->setBounds(mn.reduced(rdx));
}

void IFMEditor::hideAllSubPanels()
{
    for (auto &c : singlePanel->getChildren())
    {
        c->setVisible(false);
    }
}

} // namespace baconpaul::fm::ui