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

#ifndef BACONPAUL_SIX_SINES_DSP_OP_SOURCE_H
#define BACONPAUL_SIX_SINES_DSP_OP_SOURCE_H

#include <cstdint>
#include <cmath>

#include "configuration.h"

#include "dsp/sintable.h"
#include "dsp/node_support.h"
#include "synth/patch.h"
#include "synth/mono_values.h"
#include "synth/voice_values.h"

namespace baconpaul::six_sines
{
struct alignas(16) OpSource : public EnvelopeSupport<Patch::SourceNode>,
                              LFOSupport<Patch::SourceNode, false>,
                              ModulationSupport<Patch::SourceNode, OpSource>
{
    int32_t phaseInput alignas(16)[blockSize];
    int32_t feedbackLevel alignas(16)[blockSize];
    float rmLevel alignas(16)[blockSize];
    bool rmAssigned{false};

    float output alignas(16)[blockSize];

    const Patch::SourceNode &sourceNode;
    MonoValues &monoValues;
    const VoiceValues &voiceValues;

    bool keytrack{true};
    const float &ratio, &activeV, &envToRatio, &lfoToRatio, &envToRatioFine,
        &lfoToRatioFine; // in  frequency multiple
    const float &waveForm, &kt, &ktv, &ktlo, &ktlov, &startPhase, &octTranspose; // in octaves;
    bool active{false};
    bool unisonParticipatesPan{true}, unisonParticipatesTune{true};
    bool operatorOutputsToMain{true}, operatorOutputsToOp{true};

    // todo waveshape

    uint32_t phase;
    uint32_t dPhase;

    static constexpr float centsScale{1.0 / (12 * 100)};

    OpSource(const Patch::SourceNode &sn, MonoValues &mv, const VoiceValues &vv)
        : sourceNode(sn), monoValues(mv), voiceValues(vv), EnvelopeSupport(sn, mv, vv),
          LFOSupport(sn, mv), ModulationSupport(sn, this, mv, vv), ratio(sn.ratio),
          activeV(sn.active), envToRatio(sn.envToRatio), lfoToRatio(sn.lfoToRatio),
          waveForm(sn.waveForm), kt(sn.keyTrack), ktv(sn.keyTrackValue),
          ktlo(sn.keyTrackValueIsLow), ktlov(sn.keyTrackLowFrequencyValue),
          startPhase(sn.startingPhase), octTranspose(sn.octTranspose),
          lfoToRatioFine(sn.lfoToRatioFine), envToRatioFine(sn.envToRatioFine)
    {
        reset();
    }

    float *lfoFacP{nullptr};
    float one{1.f}; // to point to
    void reset()
    {
        st.setSampleRate(monoValues.sr.sampleRate);
        firstTime = true;
        zeroInputs();
        snapActive();

        if (active)
        {
            retriggerHasFloor = false;
            bindModulation();
            calculateModulation();
            envAttack();
            lfoAttack();

            resetPhaseOnly();
            fbVal[0] = 0.f;
            fbVal[1] = 0.f;
            auto wf = (SinTable::WaveForm)std::round(waveForm);
            st.setWaveForm(wf);

            if (lfoIsEnveloped)
            {
                lfoFacP = &env.outputCache[blockSize - 1];
            }
            else
            {
                lfoFacP = &one;
            }

            unisonParticipatesPan = (int)(sourceNode.unisonParticipation.value) & 2;
            unisonParticipatesTune = (int)(sourceNode.unisonParticipation.value) & 1;

            auto u2m = (int)(sourceNode.unisonToMain.value);
            auto u2op = (int)(sourceNode.unisonToOpOut.value);

            operatorOutputsToMain = true;
            if (u2m == 1)
            {
                operatorOutputsToMain = !(voiceValues.hasCenterVoice) || voiceValues.isCenterVoice;
            }
            else if (u2m == 2)
            {
                operatorOutputsToMain = !(voiceValues.hasCenterVoice) || !voiceValues.isCenterVoice;
            }
            else if (u2m == 3)
            {
                operatorOutputsToMain = false;
            }

            operatorOutputsToOp = true;
            if (u2op == 1)
            {
                operatorOutputsToOp = !(voiceValues.hasCenterVoice) || voiceValues.isCenterVoice;
            }
            else if (u2op == 2)
            {
                operatorOutputsToOp = !(voiceValues.hasCenterVoice) || !voiceValues.isCenterVoice;
            }
        }
    }

    void resetPhaseOnly()
    {
        phase = 4 << 27;
        unisonParticipatesTune = (int)(sourceNode.unisonParticipation.value) & 1;
        if (voiceValues.phaseRandom && unisonParticipatesTune)
        {
            phase += monoValues.rng.unifU32() & ((1 << 27) - 1);
        }
        phase += (1 << 26) * (startPhase + phaseMod);
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

    void clearOutputs() { memset(output, 0, sizeof(output)); }

    void snapActive() { active = activeV > 0.5; }

    float baseFrequency{0};
    void setBaseFrequency(float freq, float octFac)
    {
        if (kt > 0.5)
        {
            baseFrequency = freq * octFac;
        }
        else
        {
            if (ktlo > 0.5)
            {
                baseFrequency = ktlov; // its just in hertz
            }
            else
            {
                // Consciously do *not* retune absolute mode oscillators
                baseFrequency = 440 * monoValues.twoToTheX.twoToThe(ktv / 12);
            }
        }
    }

    float ratioMod{0.f};
    float envRatioAtten{1.0};
    float lfoRatioAtten{1.0};
    float phaseMod{0.f};
    float priorRF{0.f};

    bool firstTime{true};
    void renderBlock()
    {
        if (!active)
        {
            memset(output, 0, sizeof(output));
            fbVal[0] = 0.f;
            fbVal[1] = 0.f;
            return;
        }

        /*
         * Apply modulation
         */
        calculateModulation();

        envProcess();
        lfoProcess();
        auto lfoFac = *lfoFacP;

        auto rf = monoValues.twoToTheX.twoToThe(
                      ratio +
                      envRatioAtten * (envToRatio + centsScale * envToRatioFine) *
                          env.outputCache[blockSize - 1] +
                      lfoFac * lfoRatioAtten * (lfoToRatio + centsScale * lfoToRatioFine) *
                          lfo.outputBlock[0] +
                      ratioMod) *
                  (unisonParticipatesTune ? voiceValues.uniRatioMul : 1.f);

        if (firstTime)
            priorRF = rf;
        firstTime = false;
        auto dRF = (priorRF - rf) / blockSize;
        std::swap(rf, priorRF);

        if (softResetPhaseCount > 0)
        {
            float newOutput alignas(16)[blockSize];
            innerLoop(output, fbVal, rf, dRF, phase);
            innerLoop(newOutput, softFb, rf, dRF, softPhase);
            float p0 = 1.f * softResetPhaseCount / softPhaseCount;

            for (int i = 0; i < blockSize; ++i)
            {
                auto fac = p0 - i * dSoftPhase;
                output[i] = fac * output[i] + (1 - fac) * newOutput[i];
            }
            softResetPhaseCount--;

            if (softResetPhaseCount == 0)
            {
                phase = softPhase;
                fbVal[0] = softFb[0];
                fbVal[1] = softFb[1];
            }
        }
        else
        {
            innerLoop(output, fbVal, rf, dRF, phase);
        }
    }

    static constexpr int softPhaseCount{16};
    static constexpr float dSoftPhase{1.f / (blockSize * softPhaseCount)};
    int softResetPhaseCount{-1};
    uint32_t softPhase;
    float softFb[2];
    void softResetPhase()
    {
        softPhase = 4 << 27;
        softFb[0] = 0.f;
        softFb[1] = 0.f;
        softResetPhaseCount = softPhaseCount;
    }

    void innerLoop(float *onto, float *fbv, float rf, const float dRF, uint32_t &phs)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            dPhase = st.dPhase(baseFrequency * rf);
            rf += dRF;

            phs += dPhase;
            auto fb = 0.5 * (fbv[0] + fbv[1]);
            auto sb = std::signbit(feedbackLevel[i]);
            // fb = sb ? fb * fb : fb. Ugh a branch. but bool = 0/1, so
            // (1-sb) * fb + sb * fb * fb - 3 mul, 2 add
            // fb - sb * fb + sb * fb * fb - 3 nul 2 add
            // fb * ( 1 - sb * ( 1 - fb)) - 2 mul 2 add
            fb = fb * (1 - sb * (1 - fb));

            auto ph = phs + phaseInput[i] + (int32_t)(feedbackLevel[i] * fb);
            auto out = st.at(ph);

            out = out * rmLevel[i];
            onto[i] = out;
            fbv[1] = fbv[0];
            fbv[0] = out;
        }
    }

    void calculateModulation()
    {
        envRatioAtten = 1.f;
        lfoRatioAtten = 1.f;
        ratioMod = 0.f;
        phaseMod = 0.f;

        envResetMod();
        lfoResetMod();

        if (!anySources)
            return;

        for (int i = 0; i < numModsPer; ++i)
        {
            if (sourcePointers[i] &&
                (int)sourceNode.modtarget[i].value != Patch::SourceNode::TargetID::NONE)
            {
                // targets: env depth atten, lfo dept atten, direct adjust, env attack, lfo rate
                auto dp = depthPointers[i];
                if (!dp)
                    continue;
                auto d = *dp;

                auto handled =
                    envHandleModulationValue(sourceNode.modtarget[i].value, d, sourcePointers[i]) ||
                    lfoHandleModulationValue(sourceNode.modtarget[i].value, d, sourcePointers[i]);

                if (!handled)
                {
                    switch ((Patch::SourceNode::TargetID)sourceNode.modtarget[i].value)
                    {
                    case Patch::SourceNode::DIRECT:
                        ratioMod += d * *sourcePointers[i] * 2;
                        break;
                    case Patch::SourceNode::STARTING_PHASE:
                        phaseMod += d * *sourcePointers[i];
                        break;
                    case Patch::SourceNode::ENV_DEPTH_ATTEN:
                        envRatioAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                        break;
                    case Patch::SourceNode::LFO_DEPTH_ATTEN:
                        lfoRatioAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }

    SinTable st;
    float fbVal[2]{0.f, 0.f};
};
} // namespace baconpaul::six_sines

#endif
