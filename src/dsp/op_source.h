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

#ifndef BACONPAUL_SIX_SINES_DSP_OP_SOURCE_H
#define BACONPAUL_SIX_SINES_DSP_OP_SOURCE_H

#include <cstdint>

#include "configuration.h"

#include "dsp/sintable.h"
#include "dsp/node_support.h"
#include "synth/patch.h"

namespace baconpaul::six_sines
{
struct alignas(16) OpSource : public EnvelopeSupport<Patch::SourceNode>,
                              LFOSupport<Patch::SourceNode>
{
    int32_t phaseInput alignas(16)[blockSize];
    int32_t feedbackLevel alignas(16)[blockSize];
    float rmLevel alignas(16)[blockSize];
    bool rmAssigned{false};

    float output alignas(16)[blockSize];

    bool keytrack{true};
    const float &ratio, &activeV, &envToRatio, &lfoToRatio, &lfoByEnv; // in  frequency multiple
    bool active{false};

    // todo waveshape

    uint32_t phase;
    uint32_t dPhase;

    OpSource(const Patch::SourceNode &sn)
        : EnvelopeSupport(sn), LFOSupport(sn), ratio(sn.ratio), activeV(sn.active),
          envToRatio(sn.envToRatio), lfoToRatio(sn.lfoToRatio), lfoByEnv(sn.envLfoSum)
    {
        reset();
    }

    void reset()
    {
        zeroInputs();
        snapActive();
        envAttack();
        lfoAttack();
        phase = 4 << 27;
        fbVal[0] = 0.f;
        fbVal[1] = 0.f;
    }

    void zeroInputs()
    {
        for (int i = 0; i < blockSize; ++i)
        {
            phaseInput[i] = 0;
            feedbackLevel[i] = 0;
            rmLevel[i] = 1.f;
        }
        rmAssigned = false;
    }

    void snapActive() { active = activeV > 0.5; }

    float baseFrequency{0};
    void setBaseFrequency(float freq) { baseFrequency = freq; }

    void renderBlock(bool gated)
    {
        if (!active)
        {
            memset(output, 0, sizeof(output));
            fbVal[0] = 0.f;
            fbVal[1] = 0.f;
            return;
        }
        envProcess(gated);
        lfoProcess();
        auto lfoFac = lfoByEnv > 0.5 ? env.output : 1.f;

        auto rf =
            pow(2.f, ratio + envToRatio * env.output + lfoFac * lfoToRatio * lfo.outputBlock[0]);

        for (int i = 0; i < blockSize; ++i)
        {
            dPhase = st.dPhase(baseFrequency * rf);
            phase += dPhase;
            auto fb = 0.5 * (fbVal[0] + fbVal[1]);
            auto out = st.at(phase + phaseInput[i] + (int32_t)(feedbackLevel[i] * fb)) * rmLevel[i];
            output[i] = out;
            fbVal[1] = fbVal[0];
            fbVal[0] = out;
        }
    }

    SinTable st;
    float fbVal[2]{0.f, 0.f};
};
} // namespace baconpaul::six_sines

#endif
