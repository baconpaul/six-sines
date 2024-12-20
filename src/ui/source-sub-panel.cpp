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

#include "source-sub-panel.h"
#include "patch-data-bindings.h"
#include "ui-constants.h"

namespace baconpaul::six_sines::ui
{
SourceSubPanel::SourceSubPanel(SixSinesEditor &e) : HasEditor(e) { setSelectedIndex(0); };
SourceSubPanel::~SourceSubPanel() {}

void SourceSubPanel::setSelectedIndex(size_t idx)
{
    index = idx;
    repaint();

    removeAllChildren();

    auto &sn = editor.patchCopy.sourceNodes[idx];
    setupDAHDSR(editor, sn);
    createComponent(editor, *this, sn.envToRatio.meta.id, envToRatio, envToRatioD);
    envToRatioL = std::make_unique<jcmp::Label>();
    envToRatioL->setText("Env Ratio Mul");
    addAndMakeVisible(*envToRatioL);
    addAndMakeVisible(*envToRatio);
    resized();
}

void SourceSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    positionKnobAndLabel(pn.getRight() + uicMargin, pn.getY(), envToRatio, envToRatioL);
}
} // namespace baconpaul::six_sines::ui