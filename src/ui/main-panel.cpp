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

#include "main-panel.h"
#include "ui-constants.h"
#include "main-sub-panel.h"
#include "knob-highlight.h"

namespace baconpaul::six_sines::ui
{
MainPanel::MainPanel(SixSinesEditor &e) : jcmp::NamedPanel("Main"), HasEditor(e)
{
    createComponent(editor, *this, editor.patchCopy.output.level.meta.id, lev, levData);
    lev->setDrawLabel(false);
    addAndMakeVisible(*lev);
    levLabel = std::make_unique<jcmp::Label>();
    levLabel->setText("Main Level");
    addAndMakeVisible(*levLabel);

    vuMeter = std::make_unique<jcmp::VUMeter>(jcmp::VUMeter::VERTICAL);
    addAndMakeVisible(*vuMeter);

    highlight = std::make_unique<KnobHighlight>();
    addChildComponent(*highlight);
}
MainPanel::~MainPanel() = default;

void MainPanel::resized()
{
    auto b = getContentArea().reduced(uicMargin, 0);

    b = juce::Rectangle<int>(b.getX(), b.getY(), uicKnobSize + uicPowerButtonSize + uicMargin,
                             uicKnobSize);
    lev->setBounds(b.withTrimmedLeft(uicPowerButtonSize + uicMargin));
    vuMeter->setBounds(
        b.withWidth(uicPowerButtonSize).withTrimmedRight(2).withTrimmedTop(2).withTrimmedLeft(2));
    levLabel->setBounds(b.translated(0, uicKnobSize + uicLabelGap).withHeight(uicLabelHeight));
}

void MainPanel::beginEdit()
{
    editor.hideAllSubPanels();
    editor.mainSubPanel->setVisible(true);

    editor.singlePanel->setName("Main Output");

    highlight->setVisible(true);
    auto b = getContentArea().reduced(uicMargin, 0);
    highlight->setBounds(b.getX(), b.getY(), uicPowerKnobWidth + 2, uicLabeledKnobHeight);
    highlight->toBack();
}

} // namespace baconpaul::six_sines::ui