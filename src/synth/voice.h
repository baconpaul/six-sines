/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work will be
 * released under GPL3.
 *
 * The source code and license are at https://github.com/baconpaul/six-sines
 */

#ifndef BACONPAUL_SIX_SINES_SYNTH_VOICE_H
#define BACONPAUL_SIX_SINES_SYNTH_VOICE_H

#include <sst/basic-blocks/tables/EqualTuningProvider.h>
#include "dsp/op_source.h"
#include "dsp/matrix_node.h"
#include "synth/mono_values.h"
#include "synth/voice_values.h"

struct MTSClient;

namespace baconpaul::six_sines
{
struct Patch;

struct Voice
{
    const float *const output[2];
    const MonoValues &monoValues;
    VoiceValues voiceValues;

    Voice(const Patch &, const MonoValues &);
    ~Voice() = default;

    void attack();
    void renderBlock();
    void cleanup();

    bool used{false};

    std::array<OpSource, numOps> src;
    std::array<bool, numOps> isKeytrack;
    std::array<float, numOps> cmRatio;
    std::array<float, numOps> freq;

    std::array<MatrixNodeSelf, numOps> selfNode;
    std::array<MatrixNodeFrom, matrixSize> matrixNode;

    OpSource &sourceAtMatrix(size_t pos);
    OpSource &targetAtMatrix(size_t pos);

    void retriggerAllEnvelopesForKeyPress();
    void retriggerAllEnvelopesForReGate();
    void setupPortaTo(uint16_t newKey, float log2Seconds);

    std::array<MixerNode, numOps> mixerNode;
    static constexpr int32_t fadeOverBlocks{32};
    float dFade{1.0 / (blockSize * fadeOverBlocks)};
    int32_t fadeBlocks{-1};

    OutputNode out;

    Voice *prior{nullptr}, *next{nullptr};
};
} // namespace baconpaul::six_sines
#endif // VOICE_H
