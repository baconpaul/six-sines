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

#include "mixer-sub-panel.h"

namespace baconpaul::six_sines::ui
{
MixerSubPanel::MixerSubPanel(SixSinesEditor &e) : HasEditor(e) { setSelectedIndex(0); };
MixerSubPanel::~MixerSubPanel() {}

void MixerSubPanel::setSelectedIndex(int idx)
{
    index = idx;
    repaint();

    removeAllChildren();

    auto &sn = editor.patchCopy.mixerNodes[idx];
    setupDAHDSR(editor, sn);
    setupLFO(editor, sn);
    setupModulation(editor, sn);

    auto travidx{400};
    auto traverse = [&travidx](auto &c)
    { sst::jucegui::component_adapters::setTraversalId(c.get(), travidx++); };

    createRescaledComponent(editor, *this, sn.lfoToLevel, lfoToLevel, lfoToLevelDA);
    addAndMakeVisible(*lfoToLevel);
    lfoToLevelL = std::make_unique<jcmp::Label>();
    lfoToLevelL->setText("Level");
    addAndMakeVisible(*lfoToLevelL);
    traverse(lfoToLevel);

    createRescaledComponent(editor, *this, sn.lfoToPan, lfoToPan, lfoToPanDA);
    addAndMakeVisible(*lfoToPan);
    lfoToPanL = std::make_unique<jcmp::Label>();
    lfoToPanL->setText("Pan");
    addAndMakeVisible(*lfoToPanL);
    traverse(lfoToPan);

    createRescaledComponent(editor, *this, sn.envToLevel, envToLev, envToLevDA);
    addAndMakeVisible(*envToLev);
    envToLevL = std::make_unique<jcmp::Label>();
    envToLevL->setText("Level");
    addAndMakeVisible(*envToLevL);
    traverse(envToLev);

    createComponent(editor, *this, sn.envIsMultiplcative, envMul, envMulD);
    addAndMakeVisible(*envMul);
    traverse(envMul);

    modLabelE = std::make_unique<jcmp::RuledLabel>();
    modLabelE->setText(std::string() + "Env" + u8"\U00002192");
    addAndMakeVisible(*modLabelE);

    modLabelL = std::make_unique<jcmp::RuledLabel>();
    modLabelL->setText(std::string() + "LFO" + u8"\U00002192");
    addAndMakeVisible(*modLabelL);

    auto op = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->setEnabledState();
    };

    editor.componentRefreshByID[sn.envIsMultiplcative.meta.id] = op;
    envMulD->onGuiSetValue = op;

    setEnabledState();

    resized();
}

void MixerSubPanel::resized()
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
    el.add(titleLabelGaplessLayout(modLabelE));
    el.add(labelKnobLayout(envToLev, envToLevL).centerInParent());
    el.add(jlo::Component(*envMul).withHeight(2 * uicLabelHeight + uicMargin));
    lo.add(el);

    auto ll = jlo::VList().withWidth(uicSubPanelColumnWidth).withAutoGap(uicMargin);
    ll.add(titleLabelGaplessLayout(modLabelL));
    ll.add(labelKnobLayout(lfoToLevel, lfoToLevelL).centerInParent());
    ll.add(labelKnobLayout(lfoToPan, lfoToPanL).centerInParent());
    lo.add(ll);

    lo.doLayout();

    layoutModulation(p);
}

void MixerSubPanel::setEnabledState()
{
    auto en = envMulD->getValue() < 0.5;
    envToLev->setEnabled(en);
    envToLevL->setEnabled(en);
    repaint();
}

} // namespace baconpaul::six_sines::ui