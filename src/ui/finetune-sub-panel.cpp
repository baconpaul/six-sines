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

#include "main-panel.h"

namespace baconpaul::six_sines::ui
{
FineTuneSubPanel::FineTuneSubPanel(SixSinesEditor &e) : HasEditor(e)
{
    auto &on = editor.patchCopy.fineTuneMod;
    setupDAHDSR(e, on);
    setupModulation(e, on);
    setupLFO(e, on);

    auto travidx{400};
    auto traverse = [&travidx](auto &c)
    { sst::jucegui::component_adapters::setTraversalId(c.get(), travidx++); };

    lfoTitle = std::make_unique<jcmp::RuledLabel>();
    lfoTitle->setText("LFO Depth");
    addAndMakeVisible(*lfoTitle);

    envTitle = std::make_unique<jcmp::RuledLabel>();
    envTitle->setText("Env Depth");
    addAndMakeVisible(*envTitle);

    createRescaledComponent(editor, *this, on.envDepth, envDepth, envDepthDA);
    addAndMakeVisible(*envDepth);
    envDepthLL = std::make_unique<jcmp::Label>();
    envDepthLL->setText(std::string() + "Fine");
    addAndMakeVisible(*envDepthLL);
    traverse(envDepth);

    createRescaledComponent(editor, *this, on.envCoarseDepth, envCDepth, envCDepthDA);
    addAndMakeVisible(*envCDepth);
    envCDepthLL = std::make_unique<jcmp::Label>();
    envCDepthLL->setText(std::string() + "Coarse");
    addAndMakeVisible(*envCDepthLL);
    traverse(envCDepth);

    createRescaledComponent(editor, *this, on.lfoDepth, lfoDep, lfoDepDA);
    addAndMakeVisible(*lfoDep);
    lfoDepL = std::make_unique<jcmp::Label>();
    lfoDepL->setText("Fine");
    addAndMakeVisible(*lfoDepL);
    traverse(lfoDep);

    createRescaledComponent(editor, *this, on.lfoCoarseDepth, lfoCDep, lfoCDepDA);
    addAndMakeVisible(*lfoCDep);
    lfoCDepL = std::make_unique<jcmp::Label>();
    lfoCDepL->setText("Coarse");
    addAndMakeVisible(*lfoCDepL);
    traverse(lfoCDep);

    tuneTitle = std::make_unique<jcmp::RuledLabel>();
    tuneTitle->setText("Tune");
    addAndMakeVisible(*tuneTitle);
    createComponent(editor, *this, on.coarseTune, coarse, coarseD);
    coarseL = std::make_unique<jcmp::Label>();
    coarseL->setText("Coarse");
    addAndMakeVisible(*coarse);
    addAndMakeVisible(*coarseL);

    createComponent(editor, *this, editor.patchCopy.output.fineTune, fine, fineD);
    fineL = std::make_unique<jcmp::Label>();
    fineL->setText("Fine");
    addAndMakeVisible(*fine);
    addAndMakeVisible(*fineL);

    resized();
    repaint();
}

void FineTuneSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto r = layoutLFOAt(pn.getX() + uicMargin, p.getY());

    auto depx = p.getX();
    auto depy = std::min(pn.getBottom(), r.getBottom()) + uicMargin;

    namespace jlo = sst::jucegui::layouts;
    auto lo =
        jlo::VList().at(depx, depy).withWidth(uicKnobSize * 2 + uicMargin).withAutoGap(uicMargin);

    lo.add(titleLabelGaplessLayout(tuneTitle));
    auto tk = jlo::HList().withHeight(uicLabeledKnobHeight).withAutoGap(uicMargin);
    tk.add(labelKnobLayout(fine, fineL).centerInParent());
    tk.add(labelKnobLayout(coarse, coarseL).centerInParent());
    lo.add(tk);

    lo.add(titleLabelGaplessLayout(envTitle));
    auto ek = jlo::HList().withHeight(uicLabeledKnobHeight).withAutoGap(uicMargin);
    ek.add(labelKnobLayout(envDepth, envDepthLL).centerInParent());
    ek.add(labelKnobLayout(envCDepth, envCDepthLL).centerInParent());
    lo.add(ek);

    lo.add(titleLabelGaplessLayout(lfoTitle));
    auto lk = jlo::HList().withHeight(uicLabeledKnobHeight).withAutoGap(uicMargin);
    lk.add(labelKnobLayout(lfoDep, lfoDepL).centerInParent());
    lk.add(labelKnobLayout(lfoCDep, lfoCDepL).centerInParent());
    lo.add(lk);

    lo.doLayout();

    layoutModulation(p);
}

void FineTuneSubPanel::setEnabledState() {}

IMPLEMENTS_CLIPBOARD_SUPPORT(FineTuneSubPanel, fineTuneMod, Clipboard::ClipboardType::NONE);

} // namespace baconpaul::six_sines::ui