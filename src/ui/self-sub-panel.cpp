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

    createComponent(editor, *this, n.lfoToFB.meta.id, lfoToFb, lfoToFbD);
    addAndMakeVisible(*lfoToFb);
    lfoToFbL = std::make_unique<jcmp::Label>();
    lfoToFbL->setText("Depth");
    addAndMakeVisible(*lfoToFbL);

    createComponent(editor, *this, n.envLfoSum.meta.id, lfoMul, lfoMulD);
    addAndMakeVisible(*lfoMul);
    lfoMul->direction = jcmp::MultiSwitch::VERTICAL;

    resized();
}

void SelfSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto gh = (pn.getHeight() - 2 * uicPowerButtonSize) / 2;
    lfoMul->setBounds(pn.getX() + uicMargin, pn.getY() + gh, uicPowerButtonSize,
                      2 * uicPowerButtonSize);
    pn = pn.translated(2 * uicMargin + uicPowerButtonSize, 0);
    auto r = layoutLFOAt(pn.getX(), p.getY(), uicMargin + uicKnobSize);

    positionKnobAndLabel(r.getX() - uicKnobSize, r.getY() + uicTitleLabelHeight, lfoToFb, lfoToFbL);
}

} // namespace baconpaul::six_sines::ui