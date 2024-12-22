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
    lfoMul->direction = jcmp::MultiSwitch::HORIZONTAL;

    resized();

    repaint();
}

void MatrixSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto r = layoutDAHDSRAt(p.getX(), p.getY());
    r = layoutLFOAt(r.getX() + uicMargin, p.getY());
    positionKnobAndLabel(r.getX() + uicMargin, r.getY(), lfoToDepth, lfoToDepthL);

    auto bx =
        lfoToDepthL->getBounds().translated(0, lfoToDepthL->getHeight() + uicMargin).withHeight(30);
    lfoMul->setBounds(bx);
}

} // namespace baconpaul::six_sines::ui