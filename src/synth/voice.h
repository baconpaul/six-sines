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

#ifndef BACONPAUL_SIX_SINES_SYNTH_VOICE_H
#define BACONPAUL_SIX_SINES_SYNTH_VOICE_H

#include <sst/basic-blocks/tables/EqualTuningProvider.h>
#include "dsp/op_source.h"
#include "dsp/matrix_node.h"

struct MTSClient;

namespace baconpaul::six_sines
{
struct Patch;

struct Voice
{
    const float *const output[2];
    const sst::basic_blocks::tables::EqualTuningProvider &tuningProvider;

    MTSClient *mtsClient{nullptr};

    Voice(const Patch &, const sst::basic_blocks::tables::EqualTuningProvider &);
    ~Voice() = default;

    void attack();
    void renderBlock();

    bool gated{false}, used{false};
    int key{0};
    int channel{0};

    std::array<OpSource, numOps> src;
    std::array<bool, numOps> isKeytrack;
    std::array<float, numOps> cmRatio;
    std::array<float, numOps> freq;

    std::array<MatrixNodeSelf, numOps> selfNode;
    std::array<MatrixNodeFrom, matrixSize> matrixNode;

    OpSource &sourceAtMatrix(size_t pos);
    OpSource &targetAtMatrix(size_t pos);

    std::array<MixerNode, numOps> mixerNode;

    OutputNode out;

    Voice *prior{nullptr}, *next{nullptr};
};
} // namespace baconpaul::six_sines
#endif // VOICE_H
