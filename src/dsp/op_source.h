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

#ifndef BACONPAUL_SIX_SINES_DSP_OP_SOURCE_H
#define BACONPAUL_SIX_SINES_DSP_OP_SOURCE_H

#include <cstdint>

#include "configuration.h"

#include "dsp/sintable.h"
#include "dsp/node_support.h"
#include "synth/patch.h"

namespace baconpaul::six_sines
{
struct alignas(16) OpSource : public EnvelopeSupport<Patch::SourceNode>
{
    int32_t phaseInput alignas(16)[blockSize];
    int32_t feedbackLevel alignas(16)[blockSize];
    float output alignas(16)[blockSize];

    bool keytrack{true};
    const float &ratio, &activeV, &envToRatio; // in  frequency multiple
    bool active{false};

    // todo waveshape

    uint32_t phase;
    uint32_t dPhase;

    OpSource(const Patch::SourceNode &sn)
        : EnvelopeSupport(sn), ratio(sn.ratio), activeV(sn.active), envToRatio(sn.envToRatio)
    {
        reset();
    }

    void reset()
    {
        zeroInputs();
        snapActive();
        envAttack();
        phase = 4 << 27;
    }

    void zeroInputs()
    {
        for (int i = 0; i < blockSize; ++i)
        {
            phaseInput[i] = 0;
            feedbackLevel[i] = 0;
        }
    }

    void snapActive() { active = activeV > 0.5; }

    float baseFrequency{0};
    void setBaseFrequency(float freq) { baseFrequency = freq; }

    void renderBlock(bool gated)
    {
        if (!active)
        {
            memset(output, 0, sizeof(output));
            fbVal = 0.f;
            return;
        }
        envProcess(gated);
        auto rf = pow(2.f, ratio + envToRatio * env.output);

        for (int i = 0; i < blockSize; ++i)
        {
            dPhase = st.dPhase(baseFrequency * rf);
            phase += dPhase;
            auto out = st.at(phase + phaseInput[i] + (int32_t)(feedbackLevel[i] * fbVal));
            output[i] = out;
            fbVal = out;
        }
    }

    SinTable st;
    float fbVal{0.f};
};
} // namespace baconpaul::six_sines

#endif
