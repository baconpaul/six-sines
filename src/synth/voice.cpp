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

#include "voice.h"
#include "sst/cpputils/constructors.h"
#include "synth/matrix_index.h"
#include "synth/patch.h"
#include "libMTSClient.h"

namespace baconpaul::six_sines
{

namespace scpu = sst::cpputils;

Voice::Voice(const Patch &p, const MonoValues &mv)
    : monoValues(mv), out(p.output, mixerNode, mv, voiceValues),
      output{out.output[0], out.output[1]},
      src(scpu::make_array_lambda<OpSource, numOps>(
          [this, &p, &mv](auto i) { return OpSource(p.sourceNodes[i], mv, voiceValues); })),
      mixerNode(scpu::make_array_lambda<MixerNode, numOps>(
          [this, &p, &mv](auto i)
          { return MixerNode(p.mixerNodes[i], this->src[i], mv, voiceValues); })),
      selfNode(scpu::make_array_lambda<MatrixNodeSelf, numOps>(
          [this, &p, &mv](auto i)
          { return MatrixNodeSelf(p.selfNodes[i], this->src[i], mv, voiceValues); })),
      matrixNode(scpu::make_array_lambda<MatrixNodeFrom, matrixSize>(
          [&p, this, &mv](auto i)
          {
              return MatrixNodeFrom(p.matrixNodes[i], this->targetAtMatrix(i),
                                    this->sourceAtMatrix(i), mv, voiceValues);
          }))
{
    std::fill(isKeytrack.begin(), isKeytrack.end(), true);
    std::fill(cmRatio.begin(), cmRatio.end(), 1.0);
}

void Voice::attack()
{
    out.attack();
    for (auto &n : mixerNode)
        n.attack();
    for (auto &n : src)
        n.reset();
    for (auto &n : selfNode)
        n.attack();
    for (auto &n : matrixNode)
        n.attack();

    voiceValues.gated = true;
}

void Voice::renderBlock()
{
    float retuneKey = voiceValues.key;
    if (monoValues.mtsClient && MTS_HasMaster(monoValues.mtsClient))
    {
        retuneKey +=
            MTS_RetuningInSemitones(monoValues.mtsClient, voiceValues.key, voiceValues.channel);
    }
    retuneKey += ((monoValues.pitchBend >= 0) ? out.bendUp : out.bendDown) * monoValues.pitchBend;
    auto baseFreq = monoValues.tuningProvider.note_to_pitch(retuneKey - 69) * 440.0;

    for (int i = 0; i < numOps; ++i)
    {
        if (!src[i].activeV)
        {
            continue;
        }
        src[i].zeroInputs();
        src[i].setBaseFrequency(baseFreq);
        for (auto j = 0; j < i; ++j)
        {
            auto pos = MatrixIndex::positionForSourceTarget(j, i);
            matrixNode[pos].applyBlock();
        }
        selfNode[i].applyBlock();
        src[i].renderBlock();
        mixerNode[i].renderBlock();
    }

    out.renderBlock();

    if (fadeBlocks > 0)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            auto fp = fadeBlocks * blockSize - i;
            auto at = fp * dFade;
            out.output[0][i] *= at;
            out.output[1][i] *= at;
        }
        fadeBlocks--;
    }
}

static_assert(numOps == 6, "Rebuild this table if not");

OpSource &Voice::sourceAtMatrix(size_t pos) { return src[MatrixIndex::sourceIndexAt(pos)]; }
OpSource &Voice::targetAtMatrix(size_t pos) { return src[MatrixIndex::targetIndexAt(pos)]; }

void Voice::retriggerAllEnvelopes()
{
    for (auto &s : src)
        if (s.active)
            s.envAttack();

    for (auto &s : selfNode)
        if (s.active)
            s.envAttack();

    for (auto &s : mixerNode)
        if (s.active)
            s.envAttack();

    for (auto &s : matrixNode)
        if (s.active)
            s.envAttack();

    out.envAttack();
}

} // namespace baconpaul::six_sines