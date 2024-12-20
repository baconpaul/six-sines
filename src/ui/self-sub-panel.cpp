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

#include "self-sub-panel.h"

namespace baconpaul::fm::ui
{
SelfSubPanel::SelfSubPanel(IFMEditor &e) : HasEditor(e){};
SelfSubPanel::~SelfSubPanel() {}
void SelfSubPanel::setSelectedIndex(int idx)
{
    index = idx;
    repaint();

    removeAllChildren();

    setupDAHDSR(editor, editor.patchCopy.selfNodes[idx]);
    resized();
}

void SelfSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    layoutDAHDSRAt(p.getX(), p.getY());
}


} // namespace baconpaul::fm::ui