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
#include "synth/patch.h"

namespace baconpaul::fm
{
namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;
struct MatrixNodeFrom
{
    OpSource &onto, &from;
    SRProvider sr;
    MatrixNodeFrom(const Patch::MatrixNode &mn, OpSource &on, OpSource &fr)
        : onto(on), from(fr), env(&sr)
    {
    }

    sst::basic_blocks::modulators::DAHDSREnvelope<SRProvider, blockSize> env;

    float fmLevel{0.0};

    void attack() { env.attack(0.2); }

    void applyBlock(bool gated)
    {
        env.processBlock01AD(0.2, 0.4, 0.05, 0.3, 0.3, 0.3, gated);
        for (int j = 0; j < blockSize; ++j)
        {
            onto.phaseInput[0][j] +=
                (int32_t)((1 << 27) * fmLevel * env.outputCache[j] * from.output[0][j]);
            onto.phaseInput[1][j] +=
                (int32_t)((1 << 27) * fmLevel * env.outputCache[j] * from.output[1][j]);
        }
    }
};

struct MatrixNodeSelf
{
    OpSource &onto;
    SRProvider sr;
    MatrixNodeSelf(const Patch::SelfNode &sn, OpSource &on) : onto(on), env(&sr){};
    float fbBase{0.0};
    bool active{true};

    sst::basic_blocks::modulators::DAHDSREnvelope<SRProvider, blockSize> env;

    void attack() { env.attack(0.1); }
    void applyBlock(bool gated)
    {
        env.processBlock01AD(0.1, 0.1, 0.00, 0.7, 0.2, 0.7, gated);
        for (int j = 0; j < blockSize; ++j)
        {
            onto.feedbackLevel[j] = (int32_t)((1 << 24) * env.outputCache[j] * fbBase);
        }
    }
};

struct MixerNode
{
    float output alignas(16)[2][blockSize];
    OpSource &from;
    SRProvider sr;

    const float &level;
    const float &delay, &attackv, &hold, &decay, &sustain, &release;

    MixerNode(const Patch::MixerNode &mn, OpSource &f)
        : from(f), env(&sr), level(mn.level), delay(mn.delay), attackv(mn.attack), hold(mn.hold),
          decay(mn.decay), sustain(mn.sustain), release(mn.release)
    {
        memset(output, 0, sizeof(output));
    }

    float baseLevel{1.0};

    sst::basic_blocks::modulators::DAHDSREnvelope<SRProvider, blockSize> env;

    void attack() { env.attack(0.0); }

    void renderBlock(bool gated)
    {
        env.processBlock01AD(0.0, 0.6, 0.00, 0.6, 0.2, 0.7, gated);
        for (int j = 0; j < blockSize; ++j)
        {
            // use mech blah
            output[0][j] = baseLevel * env.outputCache[j] * from.output[0][j];
            output[1][j] = baseLevel * env.outputCache[j] * from.output[1][j];
        }
    }
};

struct OutputNode
{
    float output alignas(16)[2][blockSize];
    std::array<MixerNode, numOps> &fromArr;
    SRProvider sr;

    const float &level, &pan;
    const float &delay, &attackv, &hold, &decay, &sustain, &release;

    OutputNode(const Patch::OutputNode &on, std::array<MixerNode, numOps> &f)
        : fromArr(f), env(&sr), level(on.level), pan(on.pan), delay(on.delay), attackv(on.attack),
          hold(on.hold), decay(on.decay), sustain(on.sustain), release(on.release)
    {
        memset(output, 0, sizeof(output));
    }

    sst::basic_blocks::modulators::DAHDSREnvelope<SRProvider, blockSize> env;

    void attack() { env.attack(delay); }

    void renderBlock(bool gated)
    {
        env.processBlockScaledAD(delay, attackv, hold, decay, sustain, release, gated);
        memset(output, 0, sizeof(output));
        for (const auto &from : fromArr)
        {
            for (int j = 0; j < blockSize; ++j)
            {
                output[0][j] += from.output[0][j];
                output[1][j] += from.output[1][j];
            }
        }

        mech::scale_by<blockSize>(env.outputCache, output[0], output[1]);
        mech::scale_by<blockSize>(0.5, output[0], output[1]);

        // Apply main output
        auto lv = level;
        lv = lv * lv * lv;
        mech::scale_by<blockSize>(lv, output[0], output[1]);

        auto pn = pan;
        if (pn != 0.f)
        {
            pn = (pn + 1) * 0.5;
            sdsp::pan_laws::panmatrix_t pmat;
            sdsp::pan_laws::stereoEqualPower(pn, pmat);

            for (int i = 0; i < blockSize; ++i)
            {
                auto il = output[0][i];
                auto ir = output[1][i];
                output[0][i] = pmat[0] * il + pmat[2] * ir;
                output[1][i] = pmat[1] * ir + pmat[3] * il;
            }
        }
    }
};
} // namespace baconpaul::fm

#endif // MATRIX_NODE_H
