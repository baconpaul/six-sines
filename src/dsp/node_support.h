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

#ifndef BACONPAUL_FMTHING_DSP_NODE_SUPPORT_H
#define BACONPAUL_FMTHING_DSP_NODE_SUPPORT_H

#include "sst/basic-blocks/modulators/DAHDSREnvelope.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"

#include "dsp/sr_provider.h"

namespace baconpaul::fm
{
namespace sdsp = sst::basic_blocks::dsp;

template <typename T> struct EnvelopeSupport
{
    SRProvider sr;

    const float &delay, &attackv, &hold, &decay, &sustain, &release, &powerV;
    EnvelopeSupport(const T &mn)
        : env(&sr), delay(mn.delay), attackv(mn.attack), hold(mn.hold), decay(mn.decay),
          sustain(mn.sustain), release(mn.release), powerV(mn.envPower)
    {
    }

    bool active{true};
    sst::basic_blocks::modulators::DAHDSREnvelope<SRProvider, blockSize> env;

    void envAttack()
    {
        active = powerV > 0.5;
        if (active)
            env.attack(delay);
        else
            memset(env.outputCache, 0, sizeof(env.outputCache));
    }
    void envProcess(bool gated, bool maxIsForever = true)
    {
        if (!active)
            return;

        if (!gated && maxIsForever &&
            release > sst::basic_blocks::modulators::TenSecondRange::etMax - 0.0001)
        {
            for (int i = 0; i < blockSize; ++i)
                env.outputCache[i] = 1.f;
        }
        else
        {
            env.processBlockScaledAD(delay, attackv, hold, decay, sustain, release, gated);
        }
    }
};
} // namespace baconpaul::fm

#endif // NODE_SUPPORT_H
