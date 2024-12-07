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
    const float *const output[2];

    Voice();
    ~Voice() = default;

    void attack();
    void renderBlock();

    bool gated{false}, used{false};
    int key{0};

    std::array<OpSource, numOps> src;
    std::array<bool, numOps> isKeytrack;
    std::array<float, numOps> cmRatio;
    std::array<float, numOps> freq;

    std::array<MatrixNodeSelf, numOps> selfNode;
    // std::array<MatrixNodeFrom, matrixSize> matrixNode;

    std::array<MixerNode, numOps> mixerNode;

    OutputNode out;

    Voice *prior{nullptr}, *next{nullptr};
};
} // namespace baconpaul::fm
#endif // VOICE_H
