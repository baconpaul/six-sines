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
#include "synth/mono_values.h"
#include "synth/voice_values.h"

namespace baconpaul::six_sines
{
struct alignas(16) OpSource : public EnvelopeSupport<Patch::SourceNode>,
                              LFOSupport<Patch::SourceNode, false>,
                              ModulationSupport<Patch::SourceNode>
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
    const float &ratio, &activeV, &envToRatio, &lfoToRatio, &lfoByEnv; // in  frequency multiple
    const float &waveForm, &kt, &ktv;
    bool active{false};

    // todo waveshape

    uint32_t phase;
    uint32_t dPhase;

    OpSource(const Patch::SourceNode &sn, MonoValues &mv, const VoiceValues &vv)
        : sourceNode(sn), monoValues(mv), voiceValues(vv), EnvelopeSupport(sn, mv, vv),
          LFOSupport(sn, mv), ModulationSupport(sn, mv, vv), ratio(sn.ratio), activeV(sn.active),
          envToRatio(sn.envToRatio), lfoToRatio(sn.lfoToRatio), lfoByEnv(sn.envLfoSum),
          waveForm(sn.waveForm), kt(sn.keyTrack), ktv(sn.keyTrackValue)
    {
        reset();
    }

    void reset()
    {
        zeroInputs();
        snapActive();

        if (active)
        {
            bindModulation();
            calculateModulation();
            envAttack();
            lfoAttack();

            phase = 4 << 27;
            if (voiceValues.phaseRandom)
            {
                phase += monoValues.rng.unifU32() & ((1 << 27) - 1);
            }
            fbVal[0] = 0.f;
            fbVal[1] = 0.f;
            auto wf = (SinTable::WaveForm)std::round(waveForm);
            st.setWaveForm(wf);
        }
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
    void setBaseFrequency(float freq, float octFac)
    {
        if (kt > 0.5)
        {
            baseFrequency = freq * octFac;
        }
        else
        {
            // Consciously do *not* retune absolute mode oscillators
            baseFrequency = 440 * monoValues.twoToTheX.twoToThe(ktv / 12);
        }
    }

    float ratioMod{0.f};
    float envRatioAtten{1.0};
    float lfoRatioAtten{1.0};

    float priorRF{-10000000};
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
        auto lfoFac = lfoByEnv > 0.5 ? env.outputCache[blockSize - 1] : 1.f;

        auto rf = monoValues.twoToTheX.twoToThe(
            ratio + envRatioAtten * envToRatio * env.outputCache[blockSize - 1] +
            lfoFac * lfoRatioAtten * lfoToRatio * lfo.outputBlock[0] + ratioMod);
        rf *= voiceValues.uniRatioMul;
        if (priorRF < -10000)
            priorRF = rf;
        auto dRF = (priorRF - rf) / blockSize;
        std::swap(rf, priorRF);

        for (int i = 0; i < blockSize; ++i)
        {
            dPhase = st.dPhase(baseFrequency * rf);
            rf += dRF;

            phase += dPhase;
            auto fb = 0.5 * (fbVal[0] + fbVal[1]);
            auto out = st.at(phase + phaseInput[i] + (int32_t)(feedbackLevel[i] * fb)) * rmLevel[i];
            output[i] = out;
            fbVal[1] = fbVal[0];
            fbVal[0] = out;
        }
    }

    void calculateModulation()
    {
        envRatioAtten = 1.f;
        lfoRatioAtten = 1.f;
        ratioMod = 0.f;
        attackMod = 0.f;
        rateMod = 0.f;

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
                switch ((Patch::SourceNode::TargetID)sourceNode.modtarget[i].value)
                {
                case Patch::SourceNode::DIRECT:
                    ratioMod += d * *sourcePointers[i] * 2;
                    break;
                case Patch::SourceNode::ENV_DEPTH_ATTEN:
                    envRatioAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                    break;
                case Patch::SourceNode::LFO_DEPTH_ATTEN:
                    lfoRatioAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                    break;
                case Patch::SourceNode::ENV_ATTACK:
                    attackMod += d * *sourcePointers[i];
                    break;
                case Patch::SourceNode::LFO_RATE:
                    rateMod += d * *sourcePointers[i] * 4;
                    break;
                default:
                    break;
                }
            }
        }
    }

    SinTable st;
    float fbVal[2]{0.f, 0.f};
};
} // namespace baconpaul::six_sines

#endif
