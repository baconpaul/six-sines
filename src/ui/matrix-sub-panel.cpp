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

#include "matrix-sub-panel.h"
#include <sst/jucegui/layouts/ListLayout.h>

namespace baconpaul::six_sines::ui
{
MatrixSubPanel::MatrixSubPanel(SixSinesEditor &e) : HasEditor(e) { setSelectedIndex(0); };
MatrixSubPanel::~MatrixSubPanel() {}
void MatrixSubPanel::setSelectedIndex(int idx)
{
    index = idx;

    removeAllChildren();

    auto &m = editor.patchCopy.matrixNodes[idx];
    setupDAHDSR(editor, m);
    setupLFO(editor, m);
    setupModulation(editor, m);

    createRescaledComponent(editor, *this, m.lfoToDepth, lfoToDepth, lfoToDepthDA);
    addAndMakeVisible(*lfoToDepth);
    lfoToDepthL = std::make_unique<jcmp::Label>();
    lfoToDepthL->setText("Depth");
    addAndMakeVisible(*lfoToDepthL);

    createRescaledComponent(editor, *this, m.envToLevel, envToLev, envToLevDA);
    addAndMakeVisible(*envToLev);
    envToLevL = std::make_unique<jcmp::Label>();
    envToLevL->setText("Level");
    addAndMakeVisible(*envToLevL);

    createComponent(editor, *this, m.envIsMultiplcative, envMul, envMulD);
    addAndMakeVisible(*envMul);

    modLabelE = std::make_unique<jcmp::RuledLabel>();
    modLabelE->setText(std::string() + "Env" + u8"\U00002192");
    addAndMakeVisible(*modLabelE);

    modLabelL = std::make_unique<jcmp::RuledLabel>();
    modLabelL->setText(std::string() + "LFO" + u8"\U00002192");
    addAndMakeVisible(*modLabelL);

    overdriveTitle = std::make_unique<jcmp::RuledLabel>();
    overdriveTitle->setText("OverDrv");
    addAndMakeVisible(*overdriveTitle);
    createComponent(editor, *this, m.overdrive, overdrive, overdriveD);
    addAndMakeVisible(*overdrive);
    overdrive->setLabel("10x");

    auto op = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->setEnabledState();
    };

    editor.componentRefreshByID[m.envIsMultiplcative.meta.id] = op;
    envMulD->onGuiSetValue = op;
    setEnabledState();
    resized();

    repaint();
}

void MatrixSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());

    pn = pn.translated(uicMargin, 0);
    auto r = layoutLFOAt(pn.getX(), p.getY());

    auto depx = r.getX() + uicMargin;
    auto depy = r.getY();

    namespace jlo = sst::jucegui::layouts;
    auto lo = jlo::HList().at(depx, depy).withAutoGap(uicMargin * 2);

    auto el = jlo::VList().withWidth(uicSubPanelColumnWidth).withAutoGap(uicMargin);
    el.add(titleLabelGaplessLayout(modLabelE));
    el.add(labelKnobLayout(envToLev, envToLevL).centerInParent());
    el.add(jlo::Component(*envMul).withHeight(2 * uicLabelHeight + uicMargin));
    lo.add(el);

    auto ll = jlo::VList().withWidth(uicSubPanelColumnWidth).withAutoGap(uicMargin);
    ll.add(titleLabelGaplessLayout(modLabelL));
    ll.add(labelKnobLayout(lfoToDepth, lfoToDepthL).centerInParent());
    lo.add(ll);

    auto od = jlo::VList().withWidth(uicSubPanelColumnWidth).withAutoGap(uicMargin);
    od.add(titleLabelGaplessLayout(overdriveTitle));
    od.add(jlo::Component(*overdrive).withHeight(uicLabelHeight).withWidth(uicSubPanelColumnWidth));
    lo.add(od);

    lo.doLayout();

    layoutModulation(p);
}

void MatrixSubPanel::setEnabledState()
{
    auto en = envMulD->getValue() < 0.5;
    envToLev->setEnabled(en);
    envToLevL->setEnabled(en);
    repaint();
}
} // namespace baconpaul::six_sines::ui