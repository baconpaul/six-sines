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

#ifndef BACONPAUL_FMTHING_DSP_MATRIX_NODE_H
#define BACONPAUL_FMTHING_DSP_MATRIX_NODE_H

#include "sst/basic-blocks/modulators/DAHDSREnvelope.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/dsp/PanLaws.h"
#include "dsp/op_source.h"
#include "dsp/sr_provider.h"
#include "dsp/node_support.h"
#include "synth/patch.h"

namespace baconpaul::fm
{
namespace mech = sst::basic_blocks::mechanics;

struct MatrixNodeFrom : public EnvelopeSupport<Patch::MatrixNode>
{
    OpSource &onto, &from;
    const float &level, &activeV;
    MatrixNodeFrom(const Patch::MatrixNode &mn, OpSource &on, OpSource &fr)
        : onto(on), from(fr), level(mn.level), activeV(mn.active), EnvelopeSupport(mn)
    {
    }

    bool active{false};

    void attack()
    {
        active = activeV > 0.5;
        envAttack();
    }

    void applyBlock(bool gated)
    {
        if (!active)
            return;

        envProcess(gated);
        for (int j = 0; j < blockSize; ++j)
        {
            onto.phaseInput[j] +=
                (int32_t)((1 << 27) * level * env.outputCache[j] * from.output[j]);
        }
    }
};

struct MatrixNodeSelf : EnvelopeSupport<Patch::SelfNode>
{
    OpSource &onto;
    SRProvider sr;
    const float &fbBase, &activeV;
    MatrixNodeSelf(const Patch::SelfNode &sn, OpSource &on)
        : onto(on), fbBase(sn.fbLevel), activeV(sn.active), EnvelopeSupport(sn){};
    bool active{true};

    void attack()
    {
        active = activeV > 0.5;
        if (active)
            envAttack();
    }
    void applyBlock(bool gated)
    {
        if (!active)
            return;

        envProcess(gated);
        for (int j = 0; j < blockSize; ++j)
        {
            onto.feedbackLevel[j] = (int32_t)((1 << 24) * env.outputCache[j] * fbBase);
        }
    }
};

struct MixerNode : EnvelopeSupport<Patch::MixerNode>
{
    float output alignas(16)[blockSize];
    OpSource &from;
    SRProvider sr;

    const float &level, &activeF;
    bool active{false};

    MixerNode(const Patch::MixerNode &mn, OpSource &f)
        : from(f), level(mn.level), activeF(mn.active), EnvelopeSupport(mn)
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
        envProcess(gated);
        for (int j = 0; j < blockSize; ++j)
        {
            // use mech blah
            output[j] = level * env.outputCache[j] * from.output[j];
        }
    }
};

struct OutputNode : EnvelopeSupport<Patch::OutputNode>
{
    float output alignas(16)[2][blockSize];
    std::array<MixerNode, numOps> &fromArr;
    SRProvider sr;

    const float &level, &pan;

    OutputNode(const Patch::OutputNode &on, std::array<MixerNode, numOps> &f)
        : fromArr(f), level(on.level), pan(on.pan), EnvelopeSupport(on)
    {
        memset(output, 0, sizeof(output));
    }

    void attack() { envAttack(); }

    void renderBlock(bool gated)
    {
        float vSum alignas(16)[blockSize];
        memset(vSum, 0, sizeof(vSum));
        for (const auto &from : fromArr)
        {
            mech::accumulate_from_to<blockSize>(from.output, vSum);
        }

        envProcess(gated);
        mech::scale_by<blockSize>(env.outputCache, vSum);
        mech::scale_by<blockSize>(0.5, vSum);

        // Apply main output
        auto lv = level;
        lv = lv * lv * lv;
        mech::scale_by<blockSize>(lv, vSum);

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
} // namespace baconpaul::fm

#endif // MATRIX_NODE_H
