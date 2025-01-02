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

#ifndef BACONPAUL_SIX_SINES_DSP_NODE_SUPPORT_H
#define BACONPAUL_SIX_SINES_DSP_NODE_SUPPORT_H

#include <cstring>
#include <string.h>

#include "sst/cpputils/constructors.h"
#include "sst/basic-blocks/modulators/AHDSRShapedSC.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"
#include "sst/basic-blocks/dsp/Lag.h"

#include "synth/mono_values.h"
#include "synth/voice_values.h"

namespace baconpaul::six_sines
{
namespace sdsp = sst::basic_blocks::dsp;

enum TriggerMode
{
    NEW_GATE,
    NEW_VOICE,
    KEY_PRESS,
    PATCH_DEFAULT,
    ON_RELEASE
};

static const char *TriggerModeName[5]{"On Start or In Release (Legato)", "On Start Voice Only",
                                      "On Any Key Press", "Patch Default", "On Release"};

template <typename T> struct EnvelopeSupport
{
    const MonoValues &monoValues;
    const VoiceValues &voiceValues;

    const float &delay, &attackv, &hold, &decay, &sustain, &release, &powerV, &tmV, &emV;
    const float &ash, &dsh, &rsh;
    EnvelopeSupport(const T &mn, const MonoValues &mv, const VoiceValues &vv)
        : monoValues(mv), voiceValues(vv), env(&mv.sr), delay(mn.delay), attackv(mn.attack),
          hold(mn.hold), decay(mn.decay), sustain(mn.sustain), release(mn.release),
          powerV(mn.envPower), ash(mn.aShape), dsh(mn.dShape), rsh(mn.rShape), tmV(mn.triggerMode),
          emV(mn.envIsMultiplcative)
    {
    }

    TriggerMode triggerMode{NEW_GATE};
    bool allowVoiceTrigger{true};

    bool active{true}, constantEnv{false};
    using range_t = sst::basic_blocks::modulators::TwentyFiveSecondExp;
    using env_t = sst::basic_blocks::modulators::AHDSRShapedSC<SRProvider, blockSize, range_t>;
    env_t env;

    float attackMod{0.f};
    bool releaseEnvStarted{false}, releaseEnvUngated{false};
    bool envIsMult{true};

    static constexpr float minAttackOnRetrig{0.0599}; // about 1.8 ms
    float minAttack{0.f};
    bool retriggerHasFloor{true};

    void envAttack()
    {
        triggerMode = (TriggerMode)std::round(tmV);
        envIsMult = emV > 0.5;
        if (triggerMode == NEW_VOICE && !allowVoiceTrigger)
            triggerMode = NEW_GATE;

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

        bool running = env.stage <= env_t::s_release;
        bool nodel = delay < 0.00001;

        float startingValue = 0.f;
        minAttack = 0.f;
        if (running && nodel)
        {
            startingValue = env.outputCache[blockSize - 1];
            minAttack =
                (retriggerHasFloor && monoValues.attackFloorOnRetrig) ? minAttackOnRetrig : 0.f;
            ;
        }

        if (active && !constantEnv)
        {
            if (triggerMode == ON_RELEASE)
            {
                releaseEnvStarted = false;
                releaseEnvUngated = false;
            }
            else
            {
                env.attackFromWithDelay(startingValue, delay,
                                        std::clamp(attackv + attackMod, minAttack, 1.f));
            }
        }
        else if (constantEnv)
        {
            for (int i = 0; i < blockSize; ++i)
                env.outputCache[i] = sustain;
        }
        else
            memset(env.outputCache, 0, sizeof(env.outputCache));
    }

    void envProcess(bool maxIsForever = true, bool needsCurve = true)
    {
        if (!active || constantEnv)
            return;

        if (triggerMode == ON_RELEASE)
        {
            if (voiceValues.gated && !releaseEnvStarted)
            {
                memset(env.outputCache, 0, sizeof(env.outputCache));
                return;
            }

            if (voiceValues.gated)
            {
                // regated
                releaseEnvUngated = true;
            }
            if (!voiceValues.gated)
            {
                if (!releaseEnvStarted)
                {
                    // never started - so attack from zero
                    env.attackFromWithDelay(0.f, delay,
                                            std::clamp(attackv + attackMod, minAttack, 1.f));
                    releaseEnvStarted = true;
                }
                else if (releaseEnvUngated)
                {
                    env.attackFromWithDelay(env.outputCache[blockSize - 1], delay,
                                            std::clamp(attackv + attackMod, minAttack, 1.f));
                    releaseEnvStarted = true;
                    releaseEnvUngated = false;
                }
            }
            env.processBlockWithDelay(delay, std::clamp(attackv + attackMod, minAttack, 1.f), hold,
                                      decay, sustain, release, ash, dsh, rsh, !voiceValues.gated,
                                      needsCurve);
        }
        else
        {
            env.processBlockWithDelay(delay, std::clamp(attackv + attackMod, minAttack, 1.f), hold,
                                      decay, sustain, release, ash, dsh, rsh, voiceValues.gated,
                                      needsCurve);
        }
    }

    void envCleanup()
    {
        memset(env.outputCache, 0, sizeof(env.outputCache));
        env.stage = env_t::s_complete;
        active = false;
    }
};

template <typename T, bool needsSmoothing = true> struct LFOSupport
{
    const T &paramBundle;
    const MonoValues &monoValues;

    const float &lfoRate, &lfoDeform, &lfoShape, &lfoActiveV, &tempoSyncV, &bipolarV,
        &lfoIsEnvelopedV;
    bool active, doSmooth{false};
    using lfo_t = sst::basic_blocks::modulators::SimpleLFO<SRProvider, blockSize>;
    lfo_t lfo;
    sst::basic_blocks::dsp::OnePoleLag<float, false> lag;

    LFOSupport(const T &mn, MonoValues &mv)
        : paramBundle(mn), lfo(&mv.sr, mv.rng), lfoRate(mn.lfoRate), lfoDeform(mn.lfoDeform),
          lfoShape(mn.lfoShape), lfoActiveV(mn.lfoActive), tempoSyncV(mn.tempoSync), monoValues(mv),
          bipolarV(mn.lfoBipolar), lfoIsEnvelopedV(mn.lfoIsEnveloped)
    {
    }

    float rateMod{0.f};

    int shape;
    bool tempoSync{false};
    bool bipolar{true};
    bool lfoIsEnveloped{false};
    void lfoAttack()
    {
        tempoSync = tempoSyncV > 0.5;
        bipolar = bipolarV > 0.5;
        lfoIsEnveloped = lfoIsEnvelopedV > 0.5;
        shape = static_cast<int>(std::round(lfoShape));

        lfo.attack(shape);
        if (needsSmoothing)
        {
            auto tshape = (lfo_t::Shape)shape;
            switch (tshape)
            {
            case lfo_t::RAMP:
            case lfo_t::DOWN_RAMP:
            case lfo_t::PULSE:
            case lfo_t::SH_NOISE:
                doSmooth = true;
                lag.setRateInMilliseconds(10, monoValues.sr.samplerate, 1.0);
                lag.snapTo(0.f);
                break;
            default:
                doSmooth = false;
                break;
            }
        }
        else
        {
            doSmooth = false;
        }
    }
    void lfoProcess()
    {
        auto rate = lfoRate;

        if (tempoSync)
        {
            rate = -paramBundle.lfoRate.meta.snapToTemposync(-rate);
        }

        lfo.process_block(rate + rateMod, lfoDeform, shape, false, monoValues.tempoSyncRatio);

        if (doSmooth)
        {
            for (int j = 0; j < blockSize; ++j)
            {
                lag.setTarget(lfo.outputBlock[j]);
                lag.process();
                lfo.outputBlock[j] = lag.v;
            }
        }
        if (!bipolar)
        {
            for (int j = 0; j < blockSize; ++j)
            {
                lfo.outputBlock[j] = (lfo.outputBlock[j] + 1) * 0.5;
            }
        }
    }
};

template <typename T> struct ModulationSupport
{
    const T &paramBundle;
    MonoValues &monoValues;
    const VoiceValues &voiceValues;

    // array makes ref clumsy so show pointers instead
    bool anySources{false};
    std::array<const float *, numModsPer> sourcePointers;
    std::array<const float *, numModsPer> depthPointers;
    std::array<float, numModsPer> priorModulation;

    float modr01, modrpm1, modrnorm, modrhalfnorm;

    ModulationSupport(const T &mn, MonoValues &mv, const VoiceValues &vv)
        : paramBundle(mn), monoValues(mv), voiceValues(vv),
          depthPointers(sst::cpputils::make_array_lambda<const float *, numModsPer>(
              [this](int i) { return &paramBundle.moddepth[i].value; }))
    {
        std::fill(sourcePointers.begin(), sourcePointers.end(), nullptr);
        std::fill(priorModulation.begin(), priorModulation.end(), 0.f);
    }

    void bindModulation()
    {
        bool changed{false};
        for (int i = 0; i < numModsPer; ++i)
        {
            if (priorModulation[i] != paramBundle.modsource[i].value)
            {
                rebindPointer(i);
                changed = true;
                priorModulation[i] = paramBundle.modsource[i].value;
            }
        }
        if (changed)
        {
            anySources = false;
            for (int i = 0; i < numModsPer; ++i)
            {
                anySources |= (sourcePointers[i] != nullptr);
            }
        }

        if (anySources)
        {
            modr01 = monoValues.rng.unif01();
            modrpm1 = monoValues.rng.unifPM1();
            modrnorm = monoValues.rng.normPM1();
            modrhalfnorm = monoValues.rng.half01();
        }
    }

    void rebindPointer(int which)
    {
        auto sv = (int)std::round(paramBundle.modsource[which].value);
        if (sv == ModMatrixConfig::Source::OFF)
        {
            sourcePointers[which] = nullptr;
            return;
        }

        if (sv >= ModMatrixConfig::Source::MIDICC_0 && sv < ModMatrixConfig::Source::MIDICC_0 + 128)
        {
            sourcePointers[which] =
                &(monoValues.midiCCFloat[sv - ModMatrixConfig::Source::MIDICC_0]);
            return;
        }

        if (sv >= ModMatrixConfig::Source::MACRO_0 &&
            sv < ModMatrixConfig::Source::MACRO_0 + numMacros)
        {
            sourcePointers[which] = monoValues.macroPtr[sv - ModMatrixConfig::Source::MACRO_0];
            return;
        }

        sourcePointers[which] = nullptr;

        switch (sv)
        {
        case ModMatrixConfig::Source::CHANNEL_AT:
            sourcePointers[which] = &monoValues.channelAT;
            break;
        case ModMatrixConfig::Source::PITCH_BEND:
            sourcePointers[which] = &monoValues.pitchBend;
            break;

        case ModMatrixConfig::Source::VELOCITY:
            sourcePointers[which] = &voiceValues.velocity;
            break;
        case ModMatrixConfig::Source::RELEASE_VELOCITY:
            sourcePointers[which] = &voiceValues.releaseVelocity;
            break;
        case ModMatrixConfig::Source::UNISON_VAL:
            sourcePointers[which] = &voiceValues.uniPMScale;
            break;
        case ModMatrixConfig::Source::POLY_AT:
            sourcePointers[which] = &voiceValues.polyAt;
            break;

        case ModMatrixConfig::Source::GATED:
            sourcePointers[which] = &voiceValues.gatedFloat;
            break;
        case ModMatrixConfig::Source::RELEASED:
            sourcePointers[which] = &voiceValues.ungatedFloat;
            break;

        case ModMatrixConfig::Source::MPE_PRESSURE:
            sourcePointers[which] = &voiceValues.mpePressure;
            break;
        case ModMatrixConfig::Source::MPE_TIMBRE:
            sourcePointers[which] = &voiceValues.mpeTimbre;
            break;
        case ModMatrixConfig::Source::MPE_PITCHBEND:
            sourcePointers[which] = &voiceValues.mpeBendInSemis;
            break;

        case ModMatrixConfig::Source::RANDOM_01:
            sourcePointers[which] = &modr01;
            break;
        case ModMatrixConfig::Source::RANDOM_PM1:
            sourcePointers[which] = &modrpm1;
            break;
        case ModMatrixConfig::Source::RANDOM_NORM:
            sourcePointers[which] = &modrnorm;
            break;
        case ModMatrixConfig::Source::RANDOM_HALFNORM:
            sourcePointers[which] = &modrhalfnorm;
            break;

        default:
            SXSNLOG("Fell Through on Mod Assignment " << which << " " << sv);
            break;
        }
    }
};
} // namespace baconpaul::six_sines

#endif // NODE_SUPPORT_H
