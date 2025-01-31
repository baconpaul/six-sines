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

#include "finetune-sub-panel.h"

namespace baconpaul::six_sines::ui
{
FineTuneSubPanel::FineTuneSubPanel(SixSinesEditor &e) : HasEditor(e)
{
    auto &on = editor.patchCopy.fineTuneMod;
    setupDAHDSR(e, on);
    setupModulation(e, on);
    setupLFO(e, on);

    depTitle = std::make_unique<jcmp::RuledLabel>();
    depTitle->setText("Tune");
    addAndMakeVisible(*depTitle);

    createRescaledComponent(editor, *this, on.envDepth, envDepth, envDepthDA);
    addAndMakeVisible(*envDepth);
    envDepthLL = std::make_unique<jcmp::Label>();
    envDepthLL->setText(std::string() + "Env " + u8"\U00002192");
    addAndMakeVisible(*envDepthLL);

    createRescaledComponent(editor, *this, on.lfoDepth, lfoDep, lfoDepDA);
    addAndMakeVisible(*lfoDep);
    lfoDepL = std::make_unique<jcmp::Label>();
    lfoDepL->setText(std::string() + "LFO " + u8"\U00002192");
    addAndMakeVisible(*lfoDepL);

    resized();
    repaint();
}

void FineTuneSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto gh = (pn.getHeight() - 2 * uicPowerButtonSize) / 2;
    auto r = layoutLFOAt(pn.getX() + uicMargin, p.getY());

    auto depx = r.getX() + uicMargin;
    auto depy = r.getY();

    namespace jlo = sst::jucegui::layouts;
    auto lo = jlo::HList().at(depx, depy).withAutoGap(uicMargin * 2);

    auto el = jlo::VList().withWidth(uicSubPanelColumnWidth).withAutoGap(uicMargin);
    el.add(titleLabelGaplessLayout(depTitle));
    el.add(labelKnobLayout(envDepth, envDepthLL).centerInParent());
    el.add(labelKnobLayout(lfoDep, lfoDepL).centerInParent());
    lo.add(el);

    lo.doLayout();

    layoutModulation(p);
}

void FineTuneSubPanel::setEnabledState() {}

} // namespace baconpaul::six_sines::ui