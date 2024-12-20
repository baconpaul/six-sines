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

#include "matrix-sub-panel.h"

namespace baconpaul::six_sines::ui
{
MatrixSubPanel::MatrixSubPanel(SixSinesEditor &e) : HasEditor(e){};
MatrixSubPanel::~MatrixSubPanel() {}
void MatrixSubPanel::setSelectedIndex(int idx)
{
    index = idx;
    repaint();
}

} // namespace baconpaul::six_sines::ui