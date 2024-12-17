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

#include "main-sub-panel.h"

namespace baconpaul::fm::ui
{
MainSubPanel::MainSubPanel(IFMEditor &e) : HasEditor(e){};
MainSubPanel::~MainSubPanel() {}
} // namespace baconpaul::fm::ui