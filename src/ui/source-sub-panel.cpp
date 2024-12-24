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

#include "source-sub-panel.h"
#include "patch-data-bindings.h"
#include "ui-constants.h"

namespace baconpaul::six_sines::ui
{
SourceSubPanel::SourceSubPanel(SixSinesEditor &e) : HasEditor(e) { setSelectedIndex(0); };
SourceSubPanel::~SourceSubPanel() {}

void SourceSubPanel::setSelectedIndex(size_t idx)
{
    index = idx;
    repaint();

    removeAllChildren();

    auto &sn = editor.patchCopy.sourceNodes[idx];
    setupDAHDSR(editor, sn);
    setupLFO(editor, sn);

    createComponent(editor, *this, sn.envToRatio, envToRatio, envToRatioD);
    envToRatioL = std::make_unique<jcmp::Label>();
    envToRatioL->setText("Env");
    addAndMakeVisible(*envToRatioL);
    addAndMakeVisible(*envToRatio);

    createComponent(editor, *this, sn.lfoToRatio, lfoToRatio, lfoToRatioD);
    addAndMakeVisible(*lfoToRatio);
    lfoToRatioL = std::make_unique<jcmp::Label>();
    lfoToRatioL->setText("LFO");
    addAndMakeVisible(*lfoToRatioL);

    createComponent(editor, *this, sn.envLfoSum, lfoMul, lfoMulD);
    addAndMakeVisible(*lfoMul);
    lfoMul->direction = jcmp::MultiSwitch::VERTICAL;

    modTitle = std::make_unique<RuledLabel>();
    modTitle->setText("Depth");
    addAndMakeVisible(*modTitle);

    resized();
}

void SourceSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto gh = (pn.getHeight() - 2 * uicPowerButtonSize) / 2;
    lfoMul->setBounds(pn.getX() + uicMargin, pn.getY() + gh, uicPowerButtonSize,
                      2 * uicPowerButtonSize);
    pn = pn.translated(2 * uicMargin + uicPowerButtonSize, 0);
    auto r = layoutLFOAt(pn.getX(), p.getY());

    auto depx = r.getX() + 2 * uicMargin;
    auto depy = r.getY();
    auto xtraW = 4;
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, modTitle);

    depx += xtraW;
    depy += uicTitleLabelHeight;
    positionKnobAndLabel(depx, depy, envToRatio, envToRatioL);
    depy += uicLabeledKnobHeight + uicMargin;
    positionKnobAndLabel(depx, depy, lfoToRatio, lfoToRatioL);
}
} // namespace baconpaul::six_sines::ui