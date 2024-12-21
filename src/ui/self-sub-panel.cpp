/*
 * Six Sines A Sinnin'
 *
 * A mess, with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
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
    lfoToFbL->setText("Lfo Depth");
    addAndMakeVisible(*lfoToFbL);
    resized();
}

void SelfSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto r = layoutDAHDSRAt(p.getX(), p.getY());
    r = layoutLFOAt(r.getX() + uicMargin, p.getY());
    positionKnobAndLabel(r.getX() + uicMargin, r.getY(), lfoToFb, lfoToFbL);
}

} // namespace baconpaul::six_sines::ui