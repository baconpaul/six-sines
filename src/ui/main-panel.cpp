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

#include "main-panel.h"
#include "ui-constants.h"
#include "main-sub-panel.h"
#include "mainpan-sub-panel.h"
#include "finetune-sub-panel.h"
#include "knob-highlight.h"

namespace baconpaul::six_sines::ui
{
MainPanel::MainPanel(SixSinesEditor &e) : jcmp::NamedPanel("Main"), HasEditor(e)
{
    createComponent(editor, *this, editor.patchCopy.output.level, lev, levData, 0);
    addAndMakeVisible(*lev);
    levLabel = std::make_unique<jcmp::Label>();
    levLabel->setText("Level");
    addAndMakeVisible(*levLabel);

    createComponent(editor, *this, editor.patchCopy.output.pan, pan, panData, 1);
    addAndMakeVisible(*pan);
    panLabel = std::make_unique<jcmp::Label>();
    panLabel->setText("Pan");
    addAndMakeVisible(*panLabel);

    createComponent(editor, *this, editor.patchCopy.output.fineTune, tun, tunData, 2);
    addAndMakeVisible(*tun);
    tunLabel = std::make_unique<jcmp::Label>();
    tunLabel->setText("Tune");
    addAndMakeVisible(*tunLabel);

    voiceCount = std::make_unique<jcmp::Label>();
    addAndMakeVisible(*voiceCount);
    setVoiceCount(0);

    voiceLabel = std::make_unique<jcmp::Label>();
    voiceLabel->setText("Voices");
    addAndMakeVisible(*voiceLabel);

    voiceLimit = std::make_unique<jcmp::MenuButton>();
    voiceLimit->setLabel(std::to_string(getPolyLimit()));
    voiceLimit->setOnCallback([this]() { showPolyLimitMenu(); });
    editor.componentRefreshByID[editor.patchCopy.output.polyLimit.meta.id] =
        [w = juce::Component::SafePointer(this)]()
    {
        if (!w)
            return;
        w->voiceLimit->setLabel(std::to_string(w->getPolyLimit()));
    };
    addAndMakeVisible(*voiceLimit);

    highlight = std::make_unique<KnobHighlight>();
    addChildComponent(*highlight);
}
MainPanel::~MainPanel() = default;

void MainPanel::resized()
{
    auto b = getContentArea().reduced(uicMargin, 0);

    positionKnobAndLabel(b.getX(), b.getY(), lev, levLabel);
    b = b.withTrimmedLeft(uicKnobSize + uicMargin);
    positionKnobAndLabel(b.getX(), b.getY(), pan, panLabel);

    b = b.withTrimmedLeft(uicKnobSize + uicMargin);
    positionKnobAndLabel(b.getX(), b.getY(), tun, tunLabel);
    b = b.withTrimmedLeft(uicKnobSize + uicMargin);

    auto vb = b.withHeight(uicLabelHeight);
    voiceCount->setBounds(vb);
    vb = vb.translated(0, uicLabelHeight + uicMargin);
    voiceLimit->setBounds(vb);
    vb = vb.translated(0, uicLabelHeight + uicMargin);
    voiceLabel->setBounds(b.withTrimmedTop(uicKnobSize + uicMargin).withHeight(uicLabelHeight));
}

void MainPanel::beginEdit(int which)
{
    editor.hideAllSubPanels();
    if (which == 0)
    {
        editor.mainSubPanel->setVisible(true);
        editor.singlePanel->setName("Main Output");
        auto b = getContentArea().reduced(uicMargin, 0);
        highlight->setBounds(b.getX(), b.getY(), uicKnobSize + uicMargin, uicLabeledKnobHeight);
    }
    else if (which == 1)
    {
        editor.mainPanSubPanel->setVisible(true);
        editor.singlePanel->setName("Main Panning");
        auto b = getContentArea().reduced(uicMargin, 0);
        highlight->setBounds(b.getX() + uicKnobSize + 0.5 * uicMargin, b.getY(),
                             uicKnobSize + uicMargin, uicLabeledKnobHeight);
    }
    else if (which == 2)
    {
        editor.fineTuneSubPanel->setVisible(true);
        editor.singlePanel->setName("Main Tuning");
        auto b = getContentArea().reduced(uicMargin, 0);
        highlight->setBounds(b.getX() + 2 * uicKnobSize + 1.5 * uicMargin, b.getY(),
                             uicKnobSize + uicMargin, uicLabeledKnobHeight);
    }

    highlight->setVisible(true);
    highlight->toBack();
}

void MainPanel::showPolyLimitMenu()
{
    auto p = juce::PopupMenu();
    p.addSectionHeader("Voice Limit");
    p.addSeparator();
    auto currentLimit = getPolyLimit();
    for (auto lim : {4, 6, 8, 12, 16, 24, 32, 48, 64})
    {
        p.addItem(std::to_string(lim), true, lim == currentLimit,
                  [w = juce::Component::SafePointer(this), lim]()
                  {
                      if (!w)
                          return;
                      w->setPolyLimit(lim);
                  });
    }
    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&editor));
}

int MainPanel::getPolyLimit() { return (int)std::round(editor.patchCopy.output.polyLimit.value); }
void MainPanel::setPolyLimit(int plVal)
{
    SXSNLOG("Setting val to " << plVal);
    auto &pl = editor.patchCopy.output.polyLimit;
    pl.value = plVal;
    editor.uiToAudio.push({Synth::UIToAudioMsg::Action::SET_PARAM, pl.meta.id, (float)plVal});
    editor.flushOperator();

    voiceLimit->setLabel(std::to_string(getPolyLimit()));
    voiceLimit->repaint();
}

} // namespace baconpaul::six_sines::ui