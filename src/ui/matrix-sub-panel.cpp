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

    createComponent(editor, *this, m.lfoToDepth, lfoToDepth, lfoToDepthD);
    addAndMakeVisible(*lfoToDepth);
    lfoToDepthL = std::make_unique<jcmp::Label>();
    lfoToDepthL->setText("Depth");
    addAndMakeVisible(*lfoToDepthL);

    createComponent(editor, *this, m.envToLevel, envToLev, envToLevD);
    addAndMakeVisible(*envToLev);
    envToLevL = std::make_unique<jcmp::Label>();
    envToLevL->setText("Level");
    addAndMakeVisible(*envToLevL);

    createComponent(editor, *this, m.envIsMultiplcative, envMul, envMulD);
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

    auto xtraW = 4;
    auto depx = r.getX() + uicMargin;
    auto depy = r.getY();

    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, modLabelE);
    positionKnobAndLabel(depx + xtraW, r.getY() + uicTitleLabelHeight, envToLev, envToLevL);
    envMul->setBounds(depx, r.getY() + uicTitleLabelHeight + uicLabeledKnobHeight + uicMargin,
                      uicKnobSize + 2 * xtraW, 2 * uicLabelHeight + uicMargin);

    depx += uicKnobSize + uicMargin + 2 * xtraW;
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, modLabelL);

    positionKnobAndLabel(depx + xtraW, r.getY() + uicTitleLabelHeight, lfoToDepth, lfoToDepthL);

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