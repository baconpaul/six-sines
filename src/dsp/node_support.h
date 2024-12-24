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

#ifndef BACONPAUL_SIX_SINES_DSP_NODE_SUPPORT_H
#define BACONPAUL_SIX_SINES_DSP_NODE_SUPPORT_H

#include <cstring>
#include <string.h>

#include "sst/basic-blocks/modulators/AHDSRShapedSC.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"

#include "dsp/sr_provider.h"
#include "synth/mono_values.h"
#include "synth/voice_values.h"

namespace baconpaul::six_sines
{
namespace sdsp = sst::basic_blocks::dsp;

template <typename T> struct EnvelopeSupport
{
    SRProvider sr;

    const MonoValues &monoValues;
    const VoiceValues &voiceValues;

    const float &delay, &attackv, &hold, &decay, &sustain, &release, &powerV;
    const float &ash, &dsh, &rsh;
    EnvelopeSupport(const T &mn, const MonoValues &mv, const VoiceValues &vv)
        : monoValues(mv), voiceValues(vv), sr(mv), env(&sr), delay(mn.delay), attackv(mn.attack),
          hold(mn.hold), decay(mn.decay), sustain(mn.sustain), release(mn.release),
          powerV(mn.envPower), ash(mn.aShape), dsh(mn.dShape), rsh(mn.rShape)
    {
    }

    bool active{true}, constantEnv{false};
    using range_t = sst::basic_blocks::modulators::TwentyFiveSecondExp;
    using env_t = sst::basic_blocks::modulators::AHDSRShapedSC<SRProvider, blockSize, range_t>;
    env_t env;

    void envAttack()
    {
        env.initializeLuts();
        active = powerV > 0.5;

        auto mn = 0.0001;
        auto mx = 1 - mn;

        if (decay < mn && attackv < mn && hold < mn && delay < mn && release > mx)
        {
            constantEnv = true;
        }
        else
        {
            constantEnv = false;
        }

        if (active && !constantEnv)
            env.attackFromWithDelay(0.f, delay, attackv);
        else if (constantEnv)
        {
            for (int i = 0; i < blockSize; ++i)
                env.outputCache[i] = sustain;
        }
        else
            memset(env.outputCache, 0, sizeof(env.outputCache));
    }
    void envProcess(bool maxIsForever = true)
    {
        if (!active || constantEnv)
            return;

        env.processBlockWithDelay(delay, attackv, hold, decay, sustain, release, ash, dsh, rsh,
                                  voiceValues.gated, true);
    }
};

template <typename T> struct LFOSupport
{
    SRProvider sr;
    const T &paramBundle;
    const MonoValues &monoValues;

    const float &lfoRate, &lfoDeform, &lfoShape, &lfoActiveV, &tempoSyncV;
    bool active;
    sst::basic_blocks::modulators::SimpleLFO<SRProvider, blockSize> lfo;

    LFOSupport(const T &mn, const MonoValues &mv)
        : sr(mv), paramBundle(mn), lfo(&sr), lfoRate(mn.lfoRate), lfoDeform(mn.lfoDeform),
          lfoShape(mn.lfoShape), lfoActiveV(mn.lfoActive), tempoSyncV(mn.tempoSync), monoValues(mv)
    {
    }

    int shape;
    bool tempoSync{false};
    void lfoAttack()
    {
        tempoSync = tempoSyncV > 0.5;
        shape = static_cast<int>(std::round(lfoShape));
        lfo.attack(shape);
    }
    void lfoProcess()
    {
        auto rate = lfoRate;
        if (tempoSync)
            rate = monoValues.tempoSyncRatio * paramBundle.lfoRate.meta.snapToTemposync(rate);

        lfo.process_block(rate, lfoDeform, shape);
    }
};
} // namespace baconpaul::six_sines

#endif // NODE_SUPPORT_H
