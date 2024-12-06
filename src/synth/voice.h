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

#ifndef BACONPAUL_FMTHING_SYNTH_VOICE_H
#define BACONPAUL_FMTHING_SYNTH_VOICE_H

#include "dsp/op_source.h"
#include "dsp/matrix_node.h"

namespace baconpaul::fm
{

struct Voice
{
    Voice() : out(src) {}
    ~Voice() = default;

    void attack() { out.attack(); }
    bool gated{false}, used{false};
    int key{0};
    OpSource src;
    MixerNode out;
};
} // namespace baconpaul::fm
#endif // VOICE_H
