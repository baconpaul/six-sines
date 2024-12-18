/*
 * BaconPaul's FM Atrocity
 *
 * A mess, with FM.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#include "matrix-panel.h"

#include "matrix-sub-panel.h"

namespace baconpaul::fm::ui
{
MatrixPanel::MatrixPanel(IFMEditor &e) : jcmp::NamedPanel("Matrix"), HasEditor(e) {}
MatrixPanel::~MatrixPanel() = default;

void MatrixPanel::resized()
{
    auto b = getContentArea();
    auto h = getContentArea().getHeight() - 18;
    auto q = b.withWidth(h);
}

void MatrixPanel::beginEdit()
{
    editor.hideAllSubPanels();
    editor.matrixSubPanel->setVisible(true);
}

} // namespace baconpaul::fm::ui