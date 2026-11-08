/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024-2025, Paul Walker and Various authors, as described in the github
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
#include "dsp/macro_node.h"
#include "synth/mono_values.h"
#include "synth/voice_values.h"

struct MTSClient;

namespace baconpaul::six_sines
{
struct Patch;

// what the voice manager and synth touch outside the render loop, which is typed by block size
struct VoiceBase
{
    VoiceValues voiceValues;
    bool used{false};
    int32_t fadeBlocks{-1};
    VoiceBase *prior{nullptr}, *next{nullptr};

    virtual ~VoiceBase() = default;
    virtual void attack() = 0;
    virtual void cleanup() = 0;
    virtual void beginTerminationFade() = 0;
    virtual void retriggerAllEnvelopesForKeyPress() = 0;
    virtual void retriggerAllEnvelopesForReGate() = 0;
    virtual void setupPortaTo(uint16_t newKey, float log2Seconds) = 0;
    virtual void restartPortaTo(float sourceKey, uint16_t newKey, float log2Seconds,
                                float portaFrac) = 0;
};

template <size_t BS> struct Voice final : VoiceBase
{
    static constexpr size_t blockSize{BS};
    static_assert(BS <= maxEngineBlockSize, "audioInBlock is sized to maxEngineBlockSize");

    const float *const output[2];
    const MonoValues &monoValues;

    Voice(const Patch &, MonoValues &);
    ~Voice() override = default;

    void attack() override;
    void renderBlock();
    void cleanup() override;
    void beginTerminationFade() override { fadeBlocks = fadeOverBlocks; }

    Voice *nextVoice() const { return static_cast<Voice *>(next); }

    std::array<OpSource<BS>, numOps> src;
    std::array<bool, numOps> isKeytrack;
    std::array<float, numOps> cmRatio;
    std::array<float, numOps> freq;

    std::array<MatrixNodeSelf<BS>, numOps> selfNode;
    std::array<MatrixNodeFrom<BS>, matrixSize> matrixNode;

    OpSource<BS> &sourceAtMatrix(size_t pos);
    OpSource<BS> &targetAtMatrix(size_t pos);

    void retriggerAllEnvelopesForKeyPress() override;
    void retriggerAllEnvelopesForReGate() override;
    void setupPortaTo(uint16_t newKey, float log2Seconds) override;
    void restartPortaTo(float sourceKey, uint16_t newKey, float log2Seconds,
                        float portaFrac) override;

    std::array<MixerNode<BS>, numOps> mixerNode;
    std::array<MacroVoiceNode<BS>, numMacros> macroNode;
    // the same 256 engine samples at every block size
    static constexpr int32_t fadeOverBlocks{256 / BS};
    float dFade{1.0 / (blockSize * fadeOverBlocks)};

    OutputNode<BS> out;
};
} // namespace baconpaul::six_sines
#endif // VOICE_H
