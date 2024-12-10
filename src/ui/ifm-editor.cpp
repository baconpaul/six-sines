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

    matrixPanel = std::make_unique<jcmp::NamedPanel>("Matrix");
    mixerPanel = std::make_unique<jcmp::NamedPanel>("Mixer");
    singlePanel = std::make_unique<jcmp::NamedPanel>("Edit");
    sourcesPanel = std::make_unique<jcmp::NamedPanel>("Sources");
    mainPanel = std::make_unique<jcmp::NamedPanel>("Main");
    addAndMakeVisible(*matrixPanel);
    addAndMakeVisible(*mixerPanel);
    addAndMakeVisible(*singlePanel);
    addAndMakeVisible(*sourcesPanel);
    addAndMakeVisible(*mainPanel);

    auto startMsg = Synth::UIToAudioMsg{Synth::UIToAudioMsg::REQUEST_REFRESH};
    uiToAudio.push(startMsg);
    flushOperator();

    idleTimer = std::make_unique<IdleTimer>(*this);
    idleTimer->startTimer(1000. / 60.);

    setSize(800, 920);
}
IFMEditor::~IFMEditor() { idleTimer->stopTimer(); }

void IFMEditor::idle()
{
    auto aum = audioToUI.pop();
    while (aum.has_value())
    {
        patchCopy.paramMap[aum->paramId]->value = aum->value;
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

    auto mx = rb.withTrimmedBottom(edH).withTrimmedLeft(mp.getWidth());
    mixerPanel->setBounds(mx.reduced(rdx));

    auto ta = getLocalBounds().withHeight(tp);
    auto sr = ta.withWidth(mp.getWidth());
    sourcesPanel->setBounds(sr.reduced(rdx));

    auto mn = ta.withTrimmedLeft(sr.getWidth());
    mainPanel->setBounds(mn.reduced(rdx));
}

} // namespace baconpaul::fm::ui