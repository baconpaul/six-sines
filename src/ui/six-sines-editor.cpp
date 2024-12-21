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

#include "six-sines-editor.h"
#include "patch-data-bindings.h"
#include "main-panel.h"
#include "main-sub-panel.h"
#include "matrix-panel.h"
#include "matrix-sub-panel.h"
#include "self-sub-panel.h"
#include "mixer-panel.h"
#include "mixer-sub-panel.h"
#include "source-panel.h"
#include "source-sub-panel.h"
#include "ui-constants.h"

namespace baconpaul::six_sines::ui
{
struct IdleTimer : juce::Timer
{
    SixSinesEditor &editor;
    IdleTimer(SixSinesEditor &e) : editor(e) {}
    void timerCallback() override { editor.idle(); }
};

SixSinesEditor::SixSinesEditor(Synth::audioToUIQueue_t &atou, Synth::uiToAudioQueue_T &utoa,
                               std::function<void()> fo)
    : audioToUI(atou), uiToAudio(utoa), flushOperator(fo)
{
    sst::jucegui::style::StyleSheet::initializeStyleSheets([]() {});

    setStyle(sst::jucegui::style::StyleSheet::getBuiltInStyleSheet(
        sst::jucegui::style::StyleSheet::DARK));

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
    selfSubPanel = std::make_unique<SelfSubPanel>(*this);
    singlePanel->addChildComponent(*matrixSubPanel);
    singlePanel->addChildComponent(*selfSubPanel);
    mixerSubPanel = std::make_unique<MixerSubPanel>(*this);
    singlePanel->addChildComponent(*mixerSubPanel);
    sourceSubPanel = std::make_unique<SourceSubPanel>(*this);
    singlePanel->addChildComponent(*sourceSubPanel);

    auto startMsg = Synth::UIToAudioMsg{Synth::UIToAudioMsg::REQUEST_REFRESH};
    uiToAudio.push(startMsg);
    flushOperator();

    idleTimer = std::make_unique<IdleTimer>(*this);
    idleTimer->startTimer(1000. / 60.);

    toolTip = std::make_unique<jcmp::ToolTip>();
    addChildComponent(*toolTip);

    setSize(800, 830);

    auto q = std::make_unique<PatchContinuous>(*this, patchCopy.output.level.meta.id);
}
SixSinesEditor::~SixSinesEditor() { idleTimer->stopTimer(); }

void SixSinesEditor::idle()
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

void SixSinesEditor::resized()
{
    auto rdx{1};

    int tpt{40};
    auto area = getLocalBounds().withTrimmedTop(tpt);

    auto tp = 100;

    auto rb = area.withTrimmedTop(tp);
    auto edH = 250 - tpt;
    ;
    auto mp = rb.withTrimmedBottom(edH);
    mp = mp.withWidth(numOps * (uicPowerKnobWidth + uicMargin) + 2 * uicMargin + 10);

    matrixPanel->setBounds(mp.reduced(rdx));

    auto sp = rb.withTrimmedBottom(mp.getHeight());
    sp = sp.translated(0, mp.getHeight());
    singlePanel->setBounds(sp.reduced(rdx));

    auto mx = rb.withTrimmedBottom(edH).withTrimmedLeft(mp.getWidth());
    mixerPanel->setBounds(mx.reduced(rdx));

    auto ta = area.withHeight(tp);
    auto sr = ta.withWidth(mp.getWidth());
    sourcePanel->setBounds(sr.reduced(rdx));

    auto mn = ta.withTrimmedLeft(sr.getWidth());
    mainPanel->setBounds(mn.reduced(rdx));

    mainSubPanel->setBounds(singlePanel->getContentArea());
    matrixSubPanel->setBounds(singlePanel->getContentArea());
    selfSubPanel->setBounds(singlePanel->getContentArea());
    mixerSubPanel->setBounds(singlePanel->getContentArea());
    sourceSubPanel->setBounds(singlePanel->getContentArea());
}

void SixSinesEditor::hideAllSubPanels()
{
    for (auto &c : singlePanel->getChildren())
    {
        c->setVisible(false);
    }
}

void SixSinesEditor::showTooltipOn(juce::Component *c)
{
    int x = 0;
    int y = 0;
    juce::Component *component = c;
    while (component != this)
    {
        auto bounds = component->getBoundsInParent();
        x += bounds.getX();
        y += bounds.getY();

        component = component->getParentComponent();
    }
    y += c->getHeight();
    toolTip->resetSizeFromData();
    if (y + toolTip->getHeight() > getHeight())
    {
        y -= c->getHeight() - 3 - toolTip->getHeight();
    }

    toolTip->setTopLeftPosition(x, y);
    toolTip->setVisible(true);
}
void SixSinesEditor::updateTooltip(jdat::Continuous *c)
{
    toolTip->setTooltipTitleAndData(c->getLabel(), {c->getValueAsString()});
    toolTip->resetSizeFromData();
}
void SixSinesEditor::updateTooltip(jdat::Discrete *d)
{
    toolTip->setTooltipTitleAndData(d->getLabel(), {d->getValueAsString()});
    toolTip->resetSizeFromData();
}
void SixSinesEditor::hideTooltip() { toolTip->setVisible(false); }

} // namespace baconpaul::six_sines::ui