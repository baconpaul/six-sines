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

#include "main-sub-panel.h"
#include "ui-constants.h"

namespace baconpaul::six_sines::ui
{
MainSubPanel::MainSubPanel(SixSinesEditor &e)
    : HasEditor(e), DAHDSRComponents(), ModulationComponents()
{
    auto &on = editor.patchCopy.output;
    voiceTrigerAllowed = false;
    setupDAHDSR(e, on);
    setupModulation(e, on);
    setupLFO(e, on);

    auto travidx{400};
    auto traverse = [&travidx](auto &c)
    { sst::jucegui::component_adapters::setTraversalId(c.get(), travidx++); };

    velTitle = std::make_unique<jcmp::RuledLabel>();
    velTitle->setText("Mod");
    addAndMakeVisible(*velTitle);

    createComponent(editor, *this, on.velSensitivity, velSen, velSenD);
    addAndMakeVisible(*velSen);
    velSenL = std::make_unique<jcmp::Label>();
    velSenL->setText("Vel Sens");
    addAndMakeVisible(*velSenL);
    traverse(velSen);

    createRescaledComponent(editor, *this, on.lfoDepth, lfoDep, lfoDepDA);
    addAndMakeVisible(*lfoDep);
    lfoDepL = std::make_unique<jcmp::Label>();
    lfoDepL->setText(std::string() + "LFO " + u8"\U00002192");
    addAndMakeVisible(*lfoDepL);
    traverse(lfoDep);
};

MainSubPanel::~MainSubPanel() {}

void MainSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto r = layoutDAHDSRAt(p.getX(), p.getY());
    r = layoutLFOAt(r.getX() + uicMargin, p.getY());

    auto depx = r.getX() + uicMargin;
    auto depy = r.getY();

    namespace jlo = sst::jucegui::layouts;
    auto lo = jlo::HList().at(depx, depy).withAutoGap(uicMargin * 2);

    auto el = jlo::VList().withWidth(uicSubPanelColumnWidth).withAutoGap(uicMargin);
    el.add(titleLabelGaplessLayout(velTitle));
    el.add(labelKnobLayout(velSen, velSenL).centerInParent());
    el.add(labelKnobLayout(lfoDep, lfoDepL).centerInParent());
    lo.add(el);

    lo.doLayout();

    layoutModulation(p);
}

} // namespace baconpaul::six_sines::ui