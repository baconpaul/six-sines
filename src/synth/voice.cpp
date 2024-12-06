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

#include "voice.h"

namespace baconpaul::fm
{
void Voice::renderBlock()
{
    src.setFrequency(440.0 * pow(2.0, (key - 69) / 12.0));
    src.renderBlock();
    out.renderBlock(gated);
}

} // namespace baconpaul::fm