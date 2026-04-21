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

    auto travidx{400};
    auto traverse = [&travidx](auto &c)
    { sst::jucegui::component_adapters::setTraversalId(c.get(), travidx++); };

    depTitle = std::make_unique<jcmp::RuledLabel>();
    depTitle->setText("Pan");
    addAndMakeVisible(*depTitle);

    createRescaledComponent(editor, *this, on.envDepth, envDepth, envDepthDA);
    addAndMakeVisible(*envDepth);
    envDepthLL = std::make_unique<jcmp::Label>();
    envDepthLL->setText(std::string() + "Env " + u8"\U00002192");
    addAndMakeVisible(*envDepthLL);
    traverse(envDepth);

    createRescaledComponent(editor, *this, on.lfoDepth, lfoDep, lfoDepDA);
    addAndMakeVisible(*lfoDep);
    lfoDepL = std::make_unique<jcmp::Label>();
    lfoDepL->setText(std::string() + "LFO " + u8"\U00002192");
    addAndMakeVisible(*lfoDepL);
    traverse(lfoDep);

    resized();
    repaint();
}

void MainPanSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto r = layoutLFOAt(pn.getX() + uicMargin, p.getY());

    auto depx = p.getX();
    auto depy = std::min(pn.getBottom(), r.getBottom()) + uicMargin;

    namespace jlo = sst::jucegui::layouts;
    auto lo = jlo::HList().at(depx, depy).withAutoGap(uicMargin * 2);

    auto el = jlo::VList().withWidth(uicKnobSize * 2 + uicMargin).withAutoGap(uicMargin);
    el.add(titleLabelGaplessLayout(depTitle));
    auto kl = jlo::HList().withHeight(uicLabeledKnobHeight).withAutoGap(uicMargin);
    kl.add(labelKnobLayout(envDepth, envDepthLL).centerInParent());
    kl.add(labelKnobLayout(lfoDep, lfoDepL).centerInParent());
    el.add(kl);
    lo.add(el);

    lo.doLayout();

    layoutModulation(p);
}

void MainPanSubPanel::setEnabledState() {}

IMPLEMENTS_CLIPBOARD_SUPPORT(MainPanSubPanel, mainPanMod, Clipboard::ClipboardType::NONE);

} // namespace baconpaul::six_sines::ui