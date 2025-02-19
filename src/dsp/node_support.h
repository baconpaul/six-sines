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
#include "synth/patch.h"

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

    const float &delay, &attackv, &hold, &decay, &sustain, &release, &powerV, &tmV, &emV, &oneShV,
        &fromZV;
    const float &ash, &dsh, &rsh;
    EnvelopeSupport(const T &mn, const MonoValues &mv, const VoiceValues &vv)
        : monoValues(mv), voiceValues(vv), env(&mv.sr), delay(mn.delay), attackv(mn.attack),
          hold(mn.hold), decay(mn.decay), sustain(mn.sustain), release(mn.release),
          powerV(mn.envPower), ash(mn.aShape), dsh(mn.dShape), rsh(mn.rShape), tmV(mn.triggerMode),
          emV(mn.envIsMultiplcative), oneShV(mn.envIsOneShot), fromZV(mn.envTriggersFromZero)
    {
    }

    TriggerMode triggerMode{NEW_GATE};
    bool envIsOneShot{false};
    bool allowVoiceTrigger{true};

    bool active{true}, constantEnv{false};
    using range_t = sst::basic_blocks::modulators::TwentyFiveSecondExp;
    using env_t = sst::basic_blocks::modulators::AHDSRShapedSC<SRProvider, blockSize, range_t>;
    env_t env;

    float delayMod{0.f}, attackMod{0.f}, holdMod{0.f}, decayMod{0.f}, sustainMod{0.f},
        releaseMod{0.f};
    bool releaseEnvStarted{false}, releaseEnvUngated{false};
    bool envIsMult{true};

    static constexpr float minAttackOnRetrig{0.05}; // the main dahdsr default
    float minAttack{0.f};
    bool retriggerHasFloor{true};

    void envAttack()
    {
        triggerMode = (TriggerMode)std::round(tmV);
        envIsMult = emV > 0.5;
        envIsOneShot = oneShV > 0.5;
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

        float startingValue = 0.f;
        minAttack = 0.f;
        if (running)
        {
            startingValue = env.outputCache[blockSize - 1];
            minAttack =
                (retriggerHasFloor && monoValues.attackFloorOnRetrig) ? minAttackOnRetrig : 0.f;
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
                auto svs = startingValue;
                if (fromZV > 0.5)
                {
                    startingValue = 0;
                }
                env.attackFromWithDelay(startingValue, std::clamp(delay + delayMod, 0.f, 1.f),
                                        std::clamp(attackv + attackMod, minAttack, 1.f));
                if (fromZV > 0.5)
                {
                    static constexpr float dbs{1.f / blockSize};
                    auto v = 1.0;
                    for (int i = 0; i < blockSize; ++i)
                    {
                        env.outputCache[i] = v * svs;
                        v -= dbs;
                    }
                }
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
                    env.attackFromWithDelay(0.f, std::clamp(delay + delayMod, 0.f, 1.f),
                                            std::clamp(attackv + attackMod, minAttack, 1.f));
                    releaseEnvStarted = true;
                }
                else if (releaseEnvUngated)
                {
                    env.attackFromWithDelay(env.outputCache[blockSize - 1],
                                            std::clamp(delay + delayMod, 0.f, 1.f),
                                            std::clamp(attackv + attackMod, minAttack, 1.f));
                    releaseEnvStarted = true;
                    releaseEnvUngated = false;
                }
            }
            env.processBlockWithDelay(std::clamp(delay + delayMod, 0.f, 1.f),
                                      std::clamp(attackv + attackMod, minAttack, 1.f),
                                      std::clamp(hold + holdMod, 0.f, 1.f),
                                      std::clamp(decay + decayMod, 0.f, 1.f), sustain + sustainMod,
                                      std::clamp(release + releaseMod, 0.f, 1.f), ash, dsh, rsh,
                                      !voiceValues.gated, needsCurve);
        }
        else
        {
            if (env.stage > env_t::s_release ||
                (voiceValues.gated && (env.stage == env_t::s_sustain) && (sustain == 0.f)))
            {
                memset(env.outputCache, 0, sizeof(env.outputCache));
                env.output = 0;
                env.outBlock0 = 0;
                return;
            }

            auto gate = envIsOneShot ? env.stage < env_t::s_sustain : voiceValues.gated;
            env.processBlockWithDelay(std::clamp(delay + delayMod, 0.f, 1.f),
                                      std::clamp(attackv + attackMod, minAttack, 1.f),
                                      std::clamp(hold + holdMod, 0.f, 1.f),
                                      std::clamp(decay + decayMod, 0.f, 1.f), sustain + sustainMod,
                                      std::clamp(release + releaseMod, 0.f, 1.f), ash, dsh, rsh,
                                      gate, needsCurve);
        }
    }

    void envCleanup()
    {
        memset(env.outputCache, 0, sizeof(env.outputCache));
        env.stage = env_t::s_complete;
        active = false;
    }

    void envResetMod()
    {
        delayMod = 0.f;
        attackMod = 0.f;
        holdMod = 0.f;
        decayMod = 0.f;
        sustainMod = 0.f;
        releaseMod = 0.f;
    }

    bool envHandleModulationValue(int target, const float depth, const float *source)
    {
        switch (target)
        {
        case Patch::DAHDSRMixin::ENV_DELAY:
            delayMod = depth * *source;
            return true;
        case Patch::DAHDSRMixin::ENV_ATTACK:
            attackMod = depth * *source;
            return true;
        case Patch::DAHDSRMixin::ENV_HOLD:
            holdMod = depth * *source;
            return true;
        case Patch::DAHDSRMixin::ENV_DECAY:
            decayMod = depth * *source;
            return true;
        case Patch::DAHDSRMixin::ENV_SUSTAIN:
            sustainMod = depth * *source;
            return true;
        case Patch::DAHDSRMixin::ENV_RELEASE:
            releaseMod = depth * *source;
            return true;
        }
        return false;
    }
};

template <typename Parent, typename T, bool needsSmoothing = true> struct LFOSupport
{
    const T &paramBundle;
    const MonoValues &monoValues;

    const float &lfoRate, &lfoDeform, &lfoShape, &lfoActiveV, &tempoSyncV, &bipolarV,
        &lfoIsEnvelopedV, &lfoStartPhase;
    bool active, doSmooth{false};
    using lfo_t = sst::basic_blocks::modulators::SimpleLFO<SRProvider, blockSize>;
    lfo_t lfo;
    sst::basic_blocks::dsp::OnePoleLag<float, false> lag;

    LFOSupport(const T &mn, MonoValues &mv)
        : paramBundle(mn), lfo(&mv.sr, mv.rng), lfoRate(mn.lfoRate), lfoDeform(mn.lfoDeform),
          lfoShape(mn.lfoShape), lfoActiveV(mn.lfoActive), tempoSyncV(mn.tempoSync), monoValues(mv),
          bipolarV(mn.lfoBipolar), lfoIsEnvelopedV(mn.lfoIsEnveloped),
          lfoStartPhase(mn.lfoStartPhase)
    {
    }

    float lfoRateMod{0.f}, lfoDeformMod{0.f}, lfoStartMod{0.f};

    int shape;
    bool tempoSync{false};
    bool bipolar{true};
    bool lfoIsEnveloped{false};

    bool runLfo{false};
    int32_t runLfoCheck{0};

    void lfoAttack()
    {
        runLfo = static_cast<Parent *>(this)->checkLfoUsed();
        runLfoCheck = 0;

        tempoSync = tempoSyncV > 0.5;
        bipolar = bipolarV > 0.5;
        lfoIsEnveloped = lfoIsEnvelopedV > 0.5;
        shape = static_cast<int>(std::round(lfoShape));

        lfo.attack(shape);
        lfo.applyPhaseOffset(lfoStartPhase + lfoStartMod);
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
        if (!runLfo)
        {
            if (runLfoCheck++ == 64)
            {
                runLfoCheck = 0;
                runLfo = static_cast<Parent *>(this)->checkLfoUsed();
            }

            return;
        }

        auto rate = lfoRate;

        if (tempoSync)
        {
            rate = -paramBundle.lfoRate.meta.snapToTemposync(-rate);
        }

        lfo.process_block(rate + lfoRateMod, std::clamp(lfoDeform + lfoDeformMod, -1.f, 1.f), shape,
                          false, tempoSync ? monoValues.tempoSyncRatio : 1.0);

        if constexpr (needsSmoothing)
        {
            if (doSmooth)
            {
                for (int j = 0; j < blockSize; ++j)
                {
                    lag.setTarget(lfo.outputBlock[j]);
                    lag.process();
                    lfo.outputBlock[j] = lag.v;
                }
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

    void lfoResetMod()
    {
        lfoRateMod = 0.f;
        lfoDeformMod = 0.f;
        lfoStartMod = 0.f;
    }

    bool lfoHandleModulationValue(int target, const float depth, const float *source)
    {
        switch (target)
        {
        case Patch::LFOMixin::LFO_RATE:
            lfoRateMod = depth * *source * 4;
            return true;
        case Patch::LFOMixin::LFO_DEFORM:
            lfoDeformMod = depth * *source;
            return true;
        case Patch::LFOMixin::LFO_STARTPHASE:
            lfoStartMod = depth * *source;
            return true;
        }
        return false;
    }
};

template <typename Bundle, typename Node> struct ModulationSupport
{
    const Bundle &paramBundle;
    MonoValues &monoValues;
    const VoiceValues &voiceValues;
    Node *enclosingNode{nullptr};

    // array makes ref clumsy so show pointers instead
    bool anySources{false};
    std::array<const float *, numModsPer> sourcePointers;
    std::array<const float *, numModsPer> depthPointers;
    std::array<float, numModsPer> priorModulation;

    float modr01, modrpm1, modrnorm, modrhalfnorm;

    ModulationSupport(const Bundle &mn, Node *p, MonoValues &mv, const VoiceValues &vv)
        : paramBundle(mn), enclosingNode(p), monoValues(mv), voiceValues(vv),
          depthPointers(sst::cpputils::make_array_lambda<const float *, numModsPer>(
              [this](int i) { return &paramBundle.moddepth[i].value; }))
    {
        std::fill(sourcePointers.begin(), sourcePointers.end(), nullptr);
        std::fill(priorModulation.begin(), priorModulation.end(), 0.f);
    }

    bool lfoUsedAsModulationSource{false};

    void bindModulation()
    {
        lfoUsedAsModulationSource = isLfoBoundToModulation();

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
        case ModMatrixConfig::Source::KEYTRACK_FROM_60:
            sourcePointers[which] = &voiceValues.keytrackFrom60;
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

        case ModMatrixConfig::Source::INTERNAL_LFO:
            sourcePointers[which] = &(enclosingNode->lfo.outputBlock[blockSize - 1]);
            break;

        case ModMatrixConfig::Source::INTERNAL_ENV:
            sourcePointers[which] = &(enclosingNode->env.outputCache[blockSize - 1]);
            break;

        default:
            SXSNLOG("Fell Through on Mod Assignment " << which << " " << sv);
            break;
        }
    }

    bool isLfoBoundToModulation()
    {
        auto res{false};

        for (int i = 0; i < numModsPer; ++i)
        {
            auto sv = (int)std::round(paramBundle.modsource[i].value);
            res = res || sv == ModMatrixConfig::Source::INTERNAL_LFO;
        }

        return res;
    }
};
} // namespace baconpaul::six_sines

#endif // NODE_SUPPORT_H
