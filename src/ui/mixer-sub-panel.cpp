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

#include "mixer-sub-panel.h"

namespace baconpaul::fm::ui
{
MixerSubPanel::MixerSubPanel(IFMEditor &e) : HasEditor(e){};
MixerSubPanel::~MixerSubPanel() {}
} // namespace baconpaul::fm::ui