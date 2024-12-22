/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
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

    createComponent(editor, *this, m.lfoToDepth.meta.id, lfoToDepth, lfoToDepthD);
    addAndMakeVisible(*lfoToDepth);
    lfoToDepthL = std::make_unique<jcmp::Label>();
    lfoToDepthL->setText("Depth");
    addAndMakeVisible(*lfoToDepthL);

    createComponent(editor, *this, m.envLfoSum.meta.id, lfoMul, lfoMulD);
    addAndMakeVisible(*lfoMul);
    lfoMul->direction = jcmp::MultiSwitch::VERTICAL;

    resized();

    repaint();
}

void MatrixSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto gh = (pn.getHeight() - 2 * uicPowerButtonSize) / 2;
    lfoMul->setBounds(pn.getX() + uicMargin, pn.getY() + gh, uicPowerButtonSize,
                      2 * uicPowerButtonSize);
    pn = pn.translated(2 * uicMargin + uicPowerButtonSize, 0);
    auto r = layoutLFOAt(pn.getX(), p.getY(), uicMargin + uicKnobSize);

    positionKnobAndLabel(r.getX() - uicKnobSize, r.getY() + uicTitleLabelHeight, lfoToDepth,
                         lfoToDepthL);
}

} // namespace baconpaul::six_sines::ui