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

#include "voice.h"
#include "sst/cpputils/constructors.h"
#include "synth/matrix_index.h"
#include "synth/patch.h"
#include "libMTSClient.h"

namespace baconpaul::six_sines
{

namespace scpu = sst::cpputils;

Voice::Voice(const Patch &p, MonoValues &mv)
    : monoValues(mv), out(p.output, p.mainPanMod, p.fineTuneMod, mixerNode, mv, voiceValues),
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
    std::fill(cmRatio.begin(), cmRatio.end(), 1.f);
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

    voiceValues.setGated(true);
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
    retuneKey += voiceValues.portaDiff * voiceValues.portaSign + voiceValues.mpeBendInSemis +
                 voiceValues.noteExpressionTuningInSemis;
    retuneKey += out.fineTune * 0.01 + out.ftModNode.coarseTune + out.ftModNode.level * 2 +
                 out.ftModNode.coarseLevel;

    if (voiceValues.portaDiff > 1e-5)
    {
        voiceValues.portaDiff -= voiceValues.dPorta;
        voiceValues.portaFrac += voiceValues.dPortaFrac;
    }
    else
    {
        voiceValues.portaDiff = 0;
        voiceValues.portaFrac = 0;
    }

    auto octSh = std::clamp((int)std::round(out.octTranspose), -3, 3);
    static constexpr float octFac[7] = {1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0, 1.0, 2.0, 4.0, 8.0};

    auto baseFreq = monoValues.tuningProvider.note_to_pitch(retuneKey - 69) * 440.0;

    for (int i = 0; i < numOps; ++i)
    {
        if (!src[i].active)
        {
            src[i].clearOutputs();
            continue;
        }
        src[i].zeroInputs();
        auto octPer = std::clamp((int)std::round(src[i].octTranspose), -3, 3);

        src[i].setBaseFrequency(baseFreq, octFac[octSh + 3] * octFac[octPer + 3]);
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

void Voice::retriggerAllEnvelopesForKeyPress()
{
    auto dtm = out.defaultTrigger;
    auto mtm = [dtm](auto tm)
    {
        if (tm == ON_RELEASE)
            return false;

        auto res = (tm == TriggerMode::KEY_PRESS ||
                    (tm == TriggerMode::PATCH_DEFAULT && dtm == TriggerMode::KEY_PRESS));
        return res;
    };
    for (auto &s : src)
        if (s.active)
            if (mtm(s.triggerMode))
            {
                s.envAttack();
                if (voiceValues.rephaseOnRetrigger)
                    s.softResetPhase();
            }

    for (auto &s : selfNode)
        if (s.active)
            if (mtm(s.triggerMode))
                s.envAttack();

    for (auto &s : mixerNode)
        if (s.active)
            if (mtm(s.triggerMode))
                s.envAttack();

    for (auto &s : matrixNode)
        if (s.active)
            if (mtm(s.triggerMode))
                s.envAttack();

    if (mtm(out.triggerMode))
        out.envAttack();
    if (mtm(out.ftModNode.triggerMode))
        out.ftModNode.envAttack();
    if (mtm(out.panModNode.triggerMode))
        out.panModNode.envAttack();
}

void Voice::retriggerAllEnvelopesForReGate()
{
    auto dtm = out.defaultTrigger;

    auto mtm = [dtm](auto tm)
    {
        if (tm == ON_RELEASE)
            return false;

        if (tm == PATCH_DEFAULT && dtm == NEW_VOICE)
            return false;

        return (tm != TriggerMode::NEW_VOICE);
    };

    for (auto &s : src)
        if (s.active)
            if (mtm(s.triggerMode))
            {
                s.envAttack();
                if (voiceValues.rephaseOnRetrigger)
                    s.softResetPhase();
            }

    for (auto &s : selfNode)
        if (s.active)
            if (mtm(s.triggerMode))
                s.envAttack();

    for (auto &s : mixerNode)
        if (s.active)
            if (mtm(s.triggerMode))
                s.envAttack();

    for (auto &s : matrixNode)
        if (s.active)
            if (mtm(s.triggerMode))
                s.envAttack();

    if (mtm(out.triggerMode))
        out.envAttack();
    if (mtm(out.ftModNode.triggerMode))
        out.ftModNode.envAttack();
    if (mtm(out.panModNode.triggerMode))
        out.panModNode.envAttack();
}

void Voice::cleanup()
{
    used = false;
    fadeBlocks = -1;
    voiceValues.setGated(false);
    voiceValues.portaDiff = 0;
    voiceValues.portaFrac = 0;
    voiceValues.portaSign = 0;
    voiceValues.dPorta = 0;
    voiceValues.dPortaFrac = 0;
    voiceValues.mpeBendInSemis = 0;

    for (auto &s : src)
    {
        s.envCleanup();
    }

    for (auto &s : selfNode)
    {
        s.envCleanup();
    }

    for (auto &s : mixerNode)
    {
        s.envCleanup();
    }

    for (auto &s : matrixNode)
    {
        s.envCleanup();
    }

    out.envCleanup();
    out.ftModNode.envCleanup();
    out.panModNode.envCleanup();
}

void Voice::setupPortaTo(uint16_t newKey, float log2Time)
{
    if (log2Time < -8 + 1e-5)
    {
        voiceValues.portaSign = 0;
        voiceValues.portaDiff = 0;
        voiceValues.dPorta = 0;
        voiceValues.portaFrac = 0;
        voiceValues.dPortaFrac = 0;
        return;
    }
    auto blocks = monoValues.twoToTheX.twoToThe(log2Time) * monoValues.sr.sampleRate / blockSize;

    auto sign = 0;
    if (voiceValues.portaDiff > 1e-5)
    {
        auto sourceKey = voiceValues.key + voiceValues.portaDiff * voiceValues.portaSign;
        sign = newKey > sourceKey ? -1 : 1; // we are headed towards the source so set sign properly

        voiceValues.portaDiff = std::abs(sourceKey - newKey);
    }
    else
    {
        sign = newKey > voiceValues.key ? -1 : 1;

        voiceValues.portaDiff = std::abs(voiceValues.key - newKey);
    }
    voiceValues.dPorta = voiceValues.portaDiff / blocks;
    voiceValues.dPortaFrac = 1.0 / blocks;
    voiceValues.portaFrac = 0;
    voiceValues.portaSign = sign;
}

void Voice::restartPortaTo(float sourceKey, uint16_t newKey, float log2Time, float portaFrac)
{
    if (log2Time < -8 + 1e-5)
    {
        voiceValues.portaSign = 0;
        voiceValues.portaDiff = 0;
        voiceValues.dPorta = 0;
        voiceValues.portaFrac = 0;
        voiceValues.dPortaFrac = 0;
        return;
    }
    auto blocks = monoValues.twoToTheX.twoToThe(log2Time) * monoValues.sr.sampleRate / blockSize;

    auto sign = 0;
    sign = newKey > sourceKey ? -1 : 1; // we are headed towards the source so set sign properly

    voiceValues.portaDiff = std::abs(sourceKey - newKey);

    voiceValues.dPorta = voiceValues.portaDiff / (blocks * (1 - portaFrac));
    voiceValues.dPortaFrac = 1.0 / blocks;
    voiceValues.portaFrac = portaFrac;
    voiceValues.portaSign = sign;
}

} // namespace baconpaul::six_sines