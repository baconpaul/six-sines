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

    createComponent(editor, *this, n.lfoToFB, lfoToFb, lfoToFbD);
    addAndMakeVisible(*lfoToFb);
    lfoToFbL = std::make_unique<jcmp::Label>();
    lfoToFbL->setText("Depth");
    addAndMakeVisible(*lfoToFbL);

    createComponent(editor, *this, n.envToFB, envToLev, envToLevD);
    addAndMakeVisible(*envToLev);
    envToLevL = std::make_unique<jcmp::Label>();
    envToLevL->setText("Level");
    addAndMakeVisible(*envToLevL);

    createComponent(editor, *this, n.envIsMultiplcative, envMul, envMulD);
    addAndMakeVisible(*envMul);

    modLabelE = std::make_unique<RuledLabel>();
    modLabelE->setText(std::string() + "Env" + u8"\U00002192");
    addAndMakeVisible(*modLabelE);

    modLabelL = std::make_unique<RuledLabel>();
    modLabelL->setText(std::string() + "LFO" + u8"\U00002192");
    addAndMakeVisible(*modLabelL);

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

    auto xtraW = 4;
    auto depx = r.getX() + uicMargin;
    auto depy = r.getY();

    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, modLabelE);
    positionKnobAndLabel(depx + xtraW, r.getY() + uicTitleLabelHeight, envToLev, envToLevL);
    envMul->setBounds(depx, r.getY() + uicTitleLabelHeight + uicLabeledKnobHeight + uicMargin,
                      uicKnobSize + 2 * xtraW, 2 * uicLabelHeight + uicMargin);

    depx += uicKnobSize + uicMargin + 2 * xtraW;
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, modLabelL);

    positionKnobAndLabel(depx + xtraW, r.getY() + uicTitleLabelHeight, lfoToFb, lfoToFbL);

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