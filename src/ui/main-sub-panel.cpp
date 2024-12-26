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

#include "main-sub-panel.h"
#include "ui-constants.h"

namespace baconpaul::six_sines::ui
{
MainSubPanel::MainSubPanel(SixSinesEditor &e) : HasEditor(e), DAHDSRComponents()
{
    setupDAHDSR(e, e.patchCopy.output);

    createComponent(editor, *this, e.patchCopy.output.velSensitivity, velSen, velSenD);
    addAndMakeVisible(*velSen);
    velSenL = std::make_unique<jcmp::Label>();
    velSenL->setText("Vel Sens");
    addAndMakeVisible(*velSenL);

    velTitle = std::make_unique<RuledLabel>();
    velTitle->setText("Play");
    addAndMakeVisible(*velTitle);

    createComponent(editor, *this, e.patchCopy.output.playMode, playMode, playModeD);
    playMode->direction = sst::jucegui::components::MultiSwitch::VERTICAL;
    addAndMakeVisible(*playMode);

    createComponent(editor, *this, e.patchCopy.output.bendUp, bUp, bUpD);
    createComponent(editor, *this, e.patchCopy.output.bendDown, bDn, bDnD);
    bUpL = std::make_unique<jcmp::Label>();
    bUpL->setText("Up");
    bDnL = std::make_unique<jcmp::Label>();
    bDnL->setText("Dn");
    bendTitle = std::make_unique<RuledLabel>();
    bendTitle->setText("Bend");

    addAndMakeVisible(*bendTitle);
    addAndMakeVisible(*bUp);
    addAndMakeVisible(*bUpL);
    addAndMakeVisible(*bDn);
    addAndMakeVisible(*bDnL);
};
MainSubPanel::~MainSubPanel() {}

void MainSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto r = layoutDAHDSRAt(p.getX(), p.getY());

    auto depx = r.getX() + 2 * uicMargin;
    auto depy = r.getY();
    auto xtraW = 4;
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, velTitle);

    depx += xtraW;
    depy += uicTitleLabelHeight;
    positionKnobAndLabel(depx, depy, velSen, velSenL);
    auto bx = velSen->getBounds().translated(0, uicLabeledKnobHeight + uicMargin);
    playMode->setBounds(bx);

    depx = r.getX() + 2 * uicMargin + uicKnobSize + 2 * xtraW + uicMargin;
    depy = r.getY();
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, bendTitle);

    auto bdw = uicKnobSize + 2 * xtraW;
    depy += uicTitleLabelHeight;
    auto bbx = juce::Rectangle<int>(depx, depy, bdw, uicLabelHeight);
    bUp->setBounds(bbx);
    bbx = bbx.translated(0, uicLabelHeight + uicMargin);
    bUpL->setBounds(bbx);
    bbx = bbx.translated(0, uicLabelHeight + uicMargin);
    bDn->setBounds(bbx);
    bbx = bbx.translated(0, uicLabelHeight + uicMargin);
    bDnL->setBounds(bbx);
}

} // namespace baconpaul::six_sines::ui