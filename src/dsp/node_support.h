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

#include <cassert>
#include <cstring>
#include <string.h>
#include <type_traits>

#include "sst/cpputils/constructors.h"
#include "sst/basic-blocks/modulators/AHDSRShapedSC.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"
#include "sst/basic-blocks/modulators/StepLFO.h"
#include "sst/basic-blocks/modulators/Transport.h"
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
    float aShapeMod{0.f}, dShapeMod{0.f}, rShapeMod{0.f};
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
            env.processBlockWithDelay(
                std::clamp(delay + delayMod, 0.f, 1.f),
                std::clamp(attackv + attackMod, minAttack, 1.f),
                std::clamp(hold + holdMod, 0.f, 1.f), std::clamp(decay + decayMod, 0.f, 1.f),
                sustain + sustainMod, std::clamp(release + releaseMod, 0.f, 1.f),
                std::clamp(ash + aShapeMod, -1.f, 1.f), std::clamp(dsh + dShapeMod, -1.f, 1.f),
                std::clamp(rsh + rShapeMod, -1.f, 1.f), !voiceValues.gated, needsCurve);
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
            env.processBlockWithDelay(
                std::clamp(delay + delayMod, 0.f, 1.f),
                std::clamp(attackv + attackMod, minAttack, 1.f),
                std::clamp(hold + holdMod, 0.f, 1.f), std::clamp(decay + decayMod, 0.f, 1.f),
                sustain + sustainMod, std::clamp(release + releaseMod, 0.f, 1.f),
                std::clamp(ash + aShapeMod, -1.f, 1.f), std::clamp(dsh + dShapeMod, -1.f, 1.f),
                std::clamp(rsh + rShapeMod, -1.f, 1.f), gate, needsCurve);
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
        aShapeMod = 0.f;
        dShapeMod = 0.f;
        rShapeMod = 0.f;
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
        case Patch::DAHDSRMixin::ENV_ASHAPE:
            aShapeMod = depth * *source;
            return true;
        case Patch::DAHDSRMixin::ENV_DSHAPE:
            dShapeMod = depth * *source;
            return true;
        case Patch::DAHDSRMixin::ENV_RSHAPE:
            rShapeMod = depth * *source;
            return true;
        }
        return false;
    }
};

template <typename Parent, typename T, bool needsSmoothing = true> struct LFOSupport
{
    const T &paramBundle;
    MonoValues &monoValues; // non-const so we can read the RNG

    const float &lfoRate, &lfoDeform, &lfoShape, &lfoActiveV, &tempoSyncV, &bipolarV,
        &lfoIsEnvelopedV, &lfoStartPhase, &runModeV;
    bool active, doSmooth{false};
    using lfo_t = sst::basic_blocks::modulators::SimpleLFO<SRProvider, blockSize>;
    lfo_t lfo;
    sst::basic_blocks::dsp::OnePoleLag<float, false> lag;

    using stepLfo_t = sst::basic_blocks::modulators::StepLFO<blockSize>;
    stepLfo_t stepLFO;
    typename stepLfo_t::Storage stepStorage;
    sst::basic_blocks::modulators::Transport stepTransport;
    std::array<const float *, numSeqSteps> stepValues;
    const float *stepCountValue{nullptr};
    const float *stepCycleModeValue{nullptr};

    LFOSupport(const T &mn, MonoValues &mv)
        : paramBundle(mn), lfo(&mv.sr, mv.rng), stepLFO(mv.tuningProvider), lfoRate(mn.lfoRate),
          lfoDeform(mn.lfoDeform), lfoShape(mn.lfoShape), lfoActiveV(mn.lfoActive),
          tempoSyncV(mn.tempoSync), monoValues(mv), bipolarV(mn.lfoBipolar),
          lfoIsEnvelopedV(mn.lfoIsEnveloped), lfoStartPhase(mn.lfoStartPhase), runModeV(mn.runMode),
          stepValues(sst::cpputils::make_array_lambda<const float *, numSeqSteps>(
              [&mn](int i) { return &mn.lfoSeqSteps[i].value; })),
          stepCountValue(&mn.lfoStepCount.value), stepCycleModeValue(&mn.lfoCycleMode.value)
    {
    }

    void snapStepStorageFromParams()
    {
        for (size_t i = 0; i < numSeqSteps; ++i)
            stepStorage.data[i] = *stepValues[i];
        for (size_t i = numSeqSteps; i < stepLfo_t::Storage::stepLfoSteps; ++i)
            stepStorage.data[i] = 0.f;
        auto count = (int)std::round(*stepCountValue);
        stepStorage.repeat = (int16_t)std::clamp(count, 1, (int)numSeqSteps);
        auto cycleOn = stepCycleModeValue && *stepCycleModeValue > 0.5f;
        stepStorage.rateIsForSingleStep = !cycleOn;
        stepStorage.smooth = std::clamp(lfoDeform + lfoDeformMod, -1.f, 1.f);
    }

    float lfoRateMod{0.f}, lfoDeformMod{0.f}, lfoStartMod{0.f};

    int shape{0};
    bool tempoSync{false};
    bool bipolar{true};
    bool lfoIsEnveloped{false};
    int runMode{Patch::LFOMixin::VOICE_TRIGGER};

    // Map the engine song position onto an LFO phase in [0,1) for SONGPOS run mode.
    // rate is the log2 LFO frequency; the instantaneous frequency is tsScale * 2^rate Hz
    // (tsScale = tempoSyncRatio under temposync, matching SimpleLFO::process_block), so the
    // phase at songPosSeconds is (songPosSeconds * tsScale * 2^rate) wrapped to one cycle.
    float songPosToPhase(float rate, double tsScale = 1.0) const
    {
        auto freqHz = monoValues.twoToTheX.twoToThe(rate) * tsScale;
        auto phase = monoValues.songPosSeconds * freqHz;
        phase -= std::floor(phase);
        return (float)phase;
    }

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
        runMode = static_cast<int>(std::round(runModeV));

        // Shape is latched at attack time; lfoProcess assumes the matching
        // oscillator was initialized here. If shape is ever allowed to change
        // between attack and process, both branches must be reconciled.
        if (shape == Patch::LFOMixin::Shape::Step)
        {
            snapStepStorageFromParams();
            stepLFO.setSampleRate(monoValues.sr.sampleRate, monoValues.sr.sampleRateInv);
            stepTransport.tempo = monoValues.tempoSyncRatio * 120.0;
            // Snap to temposync as lfoProcess does, so the song-position lock and the
            // free-run increment use the same rate.
            auto snapRate = lfoRate;
            if (tempoSync)
                snapRate = -paramBundle.lfoRate.meta.snapToTemposync(-snapRate);
            auto useRate = std::clamp(snapRate + lfoRateMod, paramBundle.lfoRate.meta.minVal,
                                      paramBundle.lfoRate.meta.maxVal);
            stepLFO.assign(&stepStorage, useRate, &stepTransport, monoValues.rng, tempoSync);

            // Start position expressed as a continuous step index (integer part = step,
            // fractional part = phase within the step). The user start phase is a fraction
            // of the whole sequence.
            double total =
                std::clamp(lfoStartPhase + lfoStartMod, 0.f, 0.999f) * stepStorage.repeat;
            if (runMode == Patch::LFOMixin::SONGPOS)
            {
                // Steps advance at 2^rate per second, scaled by the whole-sequence length
                // in cycle mode (rateIsForSingleStep == false) and by tempo when synced,
                // matching StepLFO::UpdatePhaseIncrement.
                double stepsPerSec =
                    monoValues.twoToTheX.twoToThe(useRate) *
                    (stepStorage.rateIsForSingleStep ? 1.0 : (double)stepStorage.repeat);
                if (tempoSync)
                    stepsPerSec *= monoValues.tempoSyncRatio;
                total += monoValues.songPosSeconds * stepsPerSec;
            }
            long totalFloor = (long)std::floor(total);
            double phaseFr = total - (double)totalFloor;
            int step = (int)(((totalFloor % stepStorage.repeat) + stepStorage.repeat) %
                             stepStorage.repeat);
            stepLFO.setPhaseTo(step, (float)phaseFr);
        }
        else
        {
            lfo.attack(shape);
            float phaseOffset = lfoStartPhase + lfoStartMod;
            if (runMode == Patch::LFOMixin::SONGPOS)
            {
                // Derive the start phase from the song position rather than the note
                // attack. Mirror the effective rate lfoProcess uses so the locked phase
                // matches the free-run frequency.
                auto effRate = lfoRate;
                if (tempoSync)
                    effRate = -paramBundle.lfoRate.meta.snapToTemposync(-effRate);
                effRate = std::clamp(effRate + lfoRateMod, paramBundle.lfoRate.meta.minVal,
                                     paramBundle.lfoRate.meta.maxVal);
                phaseOffset += songPosToPhase(effRate, tempoSync ? monoValues.tempoSyncRatio : 1.0);
                phaseOffset -= std::floor(phaseOffset);
            }
            lfo.applyPhaseOffset(phaseOffset);
        }
        if (needsSmoothing)
        {
            using shp = Patch::LFOMixin::Shape;
            auto tshape = (shp)shape;
            switch (tshape)
            {
            case shp::Ramp:
            case shp::Saw:
            case shp::Pulse:
            case shp::SandH:
            case shp::Step:
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

        if (shape == Patch::LFOMixin::Shape::Step)
        {
            snapStepStorageFromParams();
            stepTransport.tempo = monoValues.tempoSyncRatio * 120.0;
            auto useRate = std::clamp(rate + lfoRateMod, paramBundle.lfoRate.meta.minVal,
                                      paramBundle.lfoRate.meta.maxVal);
            stepLFO.process(useRate, 0, tempoSync, false, blockSize);
            for (int j = 0; j < blockSize; ++j)
                lfo.outputBlock[j] = stepLFO.output;
        }
        else
        {
            lfo.process_block(std::clamp(rate + lfoRateMod, paramBundle.lfoRate.meta.minVal,
                                         paramBundle.lfoRate.meta.maxVal),
                              std::clamp(lfoDeform + lfoDeformMod, -1.f, 1.f), shape, false,
                              tempoSync ? monoValues.tempoSyncRatio : 1.0);
        }

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
        if (!bipolar && shape != Patch::LFOMixin::Shape::Step)
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

    float modr01{0.f}, modrpm1{0.f}, modrnorm{0.f}, modrhalfnorm{0.f};

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

        if (sv >= ModMatrixConfig::Source::MACRO_MOD_0 &&
            sv < ModMatrixConfig::Source::MACRO_MOD_0 + numMacros)
        {
            // Macros are processed in index order; reading a higher-indexed
            // MACRO_MOD_k would see last block's value. UI prevents this;
            // assert catches a malformed patch.
#ifdef DEBUG
            if constexpr (std::is_same_v<Bundle, Patch::MacroNode>)
            {
                int mySrcIdx = sv - ModMatrixConfig::Source::MACRO_MOD_0;
                assert(mySrcIdx < paramBundle.index &&
                       "macro mod-source must reference a lower-indexed macro");
            }
#endif
            sourcePointers[which] =
                &voiceValues.macroOut[sv - ModMatrixConfig::Source::MACRO_MOD_0];
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
            sourcePointers[which] = &voiceValues.velocityLag.v;
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
            sourcePointers[which] = &voiceValues.mpePressureLag.v;
            break;
        case ModMatrixConfig::Source::MPE_TIMBRE:
            sourcePointers[which] = &voiceValues.mpeTimbreLag.v;
            break;
        case ModMatrixConfig::Source::MPE_PITCHBEND:
            sourcePointers[which] = &voiceValues.mpeBendInSemisLag.v;
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
