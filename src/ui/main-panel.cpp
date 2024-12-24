/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
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
#include "knob-highlight.h"

namespace baconpaul::six_sines::ui
{
MainPanel::MainPanel(SixSinesEditor &e) : jcmp::NamedPanel("Main"), HasEditor(e)
{
    createComponent(editor, *this, editor.patchCopy.output.level, lev, levData);
    lev->setDrawLabel(false);
    addAndMakeVisible(*lev);
    levLabel = std::make_unique<jcmp::Label>();
    levLabel->setText("Level");
    addAndMakeVisible(*levLabel);

    vuMeter = std::make_unique<jcmp::VUMeter>(jcmp::VUMeter::HORIZONTAL);
    addAndMakeVisible(*vuMeter);

    voiceCount = std::make_unique<jcmp::Label>();
    voiceCount->setJustification(juce::Justification::centredLeft);
    addAndMakeVisible(*voiceCount);
    setVoiceCount(0);

    vcOf = std::make_unique<jcmp::Label>();
    vcOf->setText("/");
    vcOf->setJustification(juce::Justification::centredRight);
    addAndMakeVisible(*vcOf);

    voiceLimit = std::make_unique<jcmp::MenuButton>();
    voiceLimit->setLabel("64");
    voiceLimit->setOnCallback(
        [this]()
        {
            auto p = juce::PopupMenu();
            p.addSectionHeader("Coming Soon");
            p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&editor));
        });
    addAndMakeVisible(*voiceLimit);

    highlight = std::make_unique<KnobHighlight>();
    addChildComponent(*highlight);
}
MainPanel::~MainPanel() = default;

void MainPanel::resized()
{
    auto b = getContentArea().reduced(uicMargin, 0);

    positionKnobAndLabel(b.getX(), b.getY(), lev, levLabel);

    auto vum = b.withTrimmedLeft(uicKnobSize + 2 * uicMargin).withHeight(b.getHeight() / 3);
    vuMeter->setBounds(vum);

    auto lp = vum.translated(0, vum.getHeight() + uicMargin);
    voiceCount->setBounds(lp);
    vcOf->setBounds(lp.withTrimmedRight(vum.getWidth() * 0.6));
    voiceLimit->setBounds(lp.withTrimmedLeft(vum.getWidth() * 0.4 + uicMargin));
}

void MainPanel::beginEdit()
{
    editor.hideAllSubPanels();
    editor.mainSubPanel->setVisible(true);

    editor.singlePanel->setName("Main Output");

    highlight->setVisible(true);
    auto b = getContentArea().reduced(uicMargin, 0);
    highlight->setBounds(b.getX(), b.getY(), uicKnobSize + uicMargin, uicLabeledKnobHeight);
    highlight->toBack();
}

} // namespace baconpaul::six_sines::ui