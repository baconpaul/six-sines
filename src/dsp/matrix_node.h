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

#ifndef BACONPAUL_SIX_SINES_DSP_MATRIX_NODE_H
#define BACONPAUL_SIX_SINES_DSP_MATRIX_NODE_H

#include "sst/basic-blocks/modulators/DAHDSREnvelope.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/dsp/PanLaws.h"
#include "dsp/op_source.h"
#include "dsp/sr_provider.h"
#include "dsp/node_support.h"
#include "synth/patch.h"

namespace baconpaul::six_sines
{
namespace mech = sst::basic_blocks::mechanics;

struct MatrixNodeFrom : public EnvelopeSupport<Patch::MatrixNode>,
                        public LFOSupport<Patch::MatrixNode>
{
    OpSource &onto, &from;
    const float &level, &activeV, &pmrmV, &lfoToDepth, &mulLfoV;
    MatrixNodeFrom(const Patch::MatrixNode &mn, OpSource &on, OpSource &fr)
        : onto(on), from(fr), level(mn.level), pmrmV(mn.pmOrRM), activeV(mn.active),
          EnvelopeSupport(mn), LFOSupport(mn), lfoToDepth(mn.lfoToDepth), mulLfoV(mn.envLfoSum)
    {
    }

    bool active{false}, isrm{false}, mulLfo{false};

    void attack()
    {
        active = activeV > 0.5;
        isrm = pmrmV > 0.5;
        mulLfo = mulLfoV > 0.5;
        if (active)
        {
            envAttack();
            lfoAttack();
        }
    }

    void applyBlock(bool gated)
    {
        if (!active)
            return;

        envProcess(gated);
        lfoProcess();
        float mod alignas(16)[blockSize], modB alignas(16)[blockSize];
        mech::mul_block<blockSize>(env.outputCache, from.output, mod);
        mech::scale_by<blockSize>(level, mod);

        if (!mulLfo)
        {
            mech::scale_accumulate_from_to<blockSize>(lfo.outputBlock, lfoToDepth, mod);
        }
        else
        {
            mech::copy_from_to<blockSize>(lfo.outputBlock, modB);
            mech::scale_by<blockSize>(lfoToDepth, modB);
            mech::mul_block<blockSize>(mod, modB, mod);
        }
        if (isrm)
        {
            if (onto.rmAssigned)
            {
                mech::accumulate_from_to<blockSize>(mod, onto.rmLevel);
            }
            else
            {
                onto.rmAssigned = true;

                mech::copy_from_to<blockSize>(mod, onto.rmLevel);
            }
        }
        else
        {
            for (int j = 0; j < blockSize; ++j)
            {
                onto.phaseInput[j] += (int32_t)((1 << 27) * (mod[j]));
            }
        }
    }
};

struct MatrixNodeSelf : EnvelopeSupport<Patch::SelfNode>, LFOSupport<Patch::SelfNode>
{
    OpSource &onto;
    SRProvider sr;
    const float &fbBase, &lfoToFB, &activeV;
    MatrixNodeSelf(const Patch::SelfNode &sn, OpSource &on)
        : onto(on), fbBase(sn.fbLevel), lfoToFB(sn.lfoToFB), activeV(sn.active),
          EnvelopeSupport(sn), LFOSupport(sn){};
    bool active{true};

    void attack()
    {
        active = activeV > 0.5;
        if (active)
        {
            envAttack();
            lfoAttack();
        }
    }
    void applyBlock(bool gated)
    {
        if (!active)
            return;

        envProcess(gated);
        lfoProcess();
        for (int j = 0; j < blockSize; ++j)
        {
            onto.feedbackLevel[j] =
                (int32_t)((1 << 24) * (env.outputCache[j] + lfoToFB * lfo.outputBlock[j]) * fbBase);
        }
    }
};

struct MixerNode : EnvelopeSupport<Patch::MixerNode>
{
    float output alignas(16)[2][blockSize];
    OpSource &from;
    SRProvider sr;

    const float &level, &activeF, &pan;
    bool active{false};

    MixerNode(const Patch::MixerNode &mn, OpSource &f)
        : from(f), pan(mn.pan), level(mn.level), activeF(mn.active), EnvelopeSupport(mn)
    {
        memset(output, 0, sizeof(output));
    }

    void attack()
    {
        active = activeF > 0.5;
        memset(output, 0, sizeof(output));
        if (active)
            envAttack();
    }

    void renderBlock(bool gated)
    {
        if (!active)
        {
            return;
        }
        float vSum alignas(16)[blockSize];

        envProcess(gated);
        for (int j = 0; j < blockSize; ++j)
        {
            // use mech blah
            vSum[j] = level * env.outputCache[j] * from.output[j];
        }

        auto pn = pan;
        if (pn != 0.f)
        {
            pn = (pn + 1) * 0.5;
            sdsp::pan_laws::panmatrix_t pmat;
            sdsp::pan_laws::monoEqualPower(pn, pmat);

            mech::mul_block<blockSize>(vSum, pmat[0], output[0]);
            mech::mul_block<blockSize>(vSum, pmat[3], output[1]);
        }
        else
        {
            mech::copy_from_to<blockSize>(vSum, output[0]);
            mech::copy_from_to<blockSize>(vSum, output[1]);
        }
    }
};

struct OutputNode : EnvelopeSupport<Patch::OutputNode>
{
    float output alignas(16)[2][blockSize];
    std::array<MixerNode, numOps> &fromArr;
    SRProvider sr;

    const float &level;

    OutputNode(const Patch::OutputNode &on, std::array<MixerNode, numOps> &f)
        : fromArr(f), level(on.level), EnvelopeSupport(on)
    {
        memset(output, 0, sizeof(output));
    }

    void attack() { envAttack(); }

    void renderBlock(bool gated)
    {
        for (const auto &from : fromArr)
        {
            mech::accumulate_from_to<blockSize>(from.output[0], output[0]);
            mech::accumulate_from_to<blockSize>(from.output[1], output[1]);
        }

        envProcess(gated, false);
        mech::scale_by<blockSize>(env.outputCache, output[0], output[1]);

        // Apply main output
        auto lv = level;
        lv = 0.2 * lv * lv * lv;
        mech::scale_by<blockSize>(lv, output[0], output[1]);
    }
};
} // namespace baconpaul::six_sines

#endif // MATRIX_NODE_H
