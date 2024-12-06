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

#ifndef BACONPAUL_FMTHING_DSP_OP_SOURCE_H
#define BACONPAUL_FMTHING_DSP_OP_SOURCE_H

#include <cstdint>

#include "configuration.h"

#include "dsp/sintable.h"

namespace baconpaul::fm
{
struct alignas(16) OpSource
{
    int32_t phaseInput alignas(16)[2][blockSize];
    float rmInput alignas(16)[2][blockSize];
    int32_t feedbackLevel alignas(16)[blockSize];
    float output alignas(16)[2][blockSize];

    bool keytrack{true};
    float ratio{1.0};   // in  frequency multiple
    float absolute{60}; // in midi keys

    // todo waveshape

    uint32_t phase[2];
    uint32_t dPhase[2];

    OpSource() { reset(); }

    void reset()
    {
        for (int i = 0; i < blockSize; ++i)
        {
            rmInput[0][i] = 1.f;
            rmInput[1][i] = 1.f;

            feedbackLevel[i] = 0.f;
        }

        phase[0] = 4 << 27;
        phase[1] = 4 << 27;
        memset(phaseInput, 0, sizeof(phaseInput));
    }

    void setFrequency(float freq)
    {
        dPhase[0] = st.dPhase(freq);
        dPhase[1] = st.dPhase(freq);
    }

    void renderBlock()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                phase[ch] += dPhase[ch];
                auto rm = rmInput[ch][i];
                auto out =
                    st.at(phase[ch] + phaseInput[ch][i] + (int32_t)(feedbackLevel[i] * fbVal[ch])) *
                    rm;
                output[ch][i] = out;
                fbVal[ch] = out;
            }
        }
    }

    SinTable st;
    float fbVal[2]{0.f, 0.f};
};
} // namespace baconpaul::fm

#endif
