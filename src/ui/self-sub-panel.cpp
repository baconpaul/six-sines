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

#include "self-sub-panel.h"
#include "patch-data-bindings.h"

namespace baconpaul::six_sines::ui
{
SelfSubPanel::SelfSubPanel(SixSinesEditor &e) : HasEditor(e) { setSelectedIndex(0); };
SelfSubPanel::~SelfSubPanel() {}
void SelfSubPanel::setSelectedIndex(int idx)
{
    index = idx;
    repaint();

    removeAllChildren();

    auto &n = editor.patchCopy.selfNodes[idx];
    setupDAHDSR(editor, editor.patchCopy.selfNodes[idx]);
    setupLFO(editor, editor.patchCopy.selfNodes[idx]);
    setupModulation(editor, editor.patchCopy.selfNodes[idx]);

    createRescaledComponent(editor, *this, n.lfoToFB, lfoToFb, lfoToFbDA);
    addAndMakeVisible(*lfoToFb);
    lfoToFbL = std::make_unique<jcmp::Label>();
    lfoToFbL->setText("Depth");
    addAndMakeVisible(*lfoToFbL);

    createRescaledComponent(editor, *this, n.envToFB, envToLev, envToLevDA);
    addAndMakeVisible(*envToLev);
    envToLevL = std::make_unique<jcmp::Label>();
    envToLevL->setText("Level");
    addAndMakeVisible(*envToLevL);

    createComponent(editor, *this, n.envIsMultiplcative, envMul, envMulD);
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
    createComponent(editor, *this, n.overdrive, overdrive, overdriveD);
    addAndMakeVisible(*overdrive);
    overdrive->setLabel("10x");

    auto op = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->setEnabledState();
    };

    editor.componentRefreshByID[n.envIsMultiplcative.meta.id] = op;
    envMulD->onGuiSetValue = op;
    setEnabledState();

    resized();
}

void SelfSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto gh = (pn.getHeight() - 2 * uicPowerButtonSize) / 2;
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
    ll.add(labelKnobLayout(lfoToFb, lfoToFbL).centerInParent());
    lo.add(ll);

    auto od = jlo::VList().withWidth(uicSubPanelColumnWidth).withAutoGap(uicMargin);
    od.add(titleLabelGaplessLayout(overdriveTitle));
    od.add(jlo::Component(*overdrive).withHeight(uicLabelHeight).withWidth(uicSubPanelColumnWidth));
    lo.add(od);

    lo.doLayout();
    layoutModulation(p);
}

void SelfSubPanel::setEnabledState()
{
    auto en = envMulD->getValue() < 0.5;
    envToLev->setEnabled(en);
    envToLevL->setEnabled(en);
    repaint();
}

} // namespace baconpaul::six_sines::ui