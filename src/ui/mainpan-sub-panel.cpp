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

#include "mainpan-sub-panel.h"

namespace baconpaul::six_sines::ui
{
MainPanSubPanel::MainPanSubPanel(SixSinesEditor &e) : HasEditor(e)
{
    auto &on = editor.patchCopy.mainPanMod;
    setupDAHDSR(e, on);
    setupModulation(e, on);
    setupLFO(e, on);

    depTitle = std::make_unique<RuledLabel>();
    depTitle->setText("Pan");
    addAndMakeVisible(*depTitle);

    createComponent(editor, *this, on.envDepth, envDepth, envDepthD);
    addAndMakeVisible(*envDepth);
    envDepthLL = std::make_unique<jcmp::Label>();
    envDepthLL->setText(std::string() + "Env " + u8"\U00002192");
    addAndMakeVisible(*envDepthLL);

    createComponent(editor, *this, on.lfoDepth, lfoDep, lfoDepD);
    addAndMakeVisible(*lfoDep);
    lfoDepL = std::make_unique<jcmp::Label>();
    lfoDepL->setText(std::string() + "LFO " + u8"\U00002192");
    addAndMakeVisible(*lfoDepL);

    resized();
    repaint();
}

void MainPanSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto gh = (pn.getHeight() - 2 * uicPowerButtonSize) / 2;
    auto r = layoutLFOAt(pn.getX() + uicMargin, p.getY());

    auto xtraW = 10;
    auto depx = r.getX() + 2 * uicMargin;
    auto depy = r.getY();

    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, depTitle);

    positionKnobAndLabel(depx + xtraW, r.getY() + uicTitleLabelHeight, envDepth, envDepthLL);
    positionKnobAndLabel(depx + xtraW,
                         r.getY() + uicTitleLabelHeight + uicMargin + uicLabeledKnobHeight, lfoDep,
                         lfoDepL);

    layoutModulation(p);
}

void MainPanSubPanel::setEnabledState() {}

} // namespace baconpaul::six_sines::ui