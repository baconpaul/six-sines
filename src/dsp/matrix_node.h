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

#ifndef BACONPAUL_SIX_SINES_DSP_MATRIX_NODE_H
#define BACONPAUL_SIX_SINES_DSP_MATRIX_NODE_H

#include "sst/basic-blocks/modulators/DAHDSREnvelope.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/dsp/PanLaws.h"
#include "dsp/op_source.h"
#include "dsp/sr_provider.h"
#include "dsp/node_support.h"
#include "synth/patch.h"
#include "synth/mono_values.h"

namespace baconpaul::six_sines
{
namespace mech = sst::basic_blocks::mechanics;

struct MatrixNodeFrom : public EnvelopeSupport<Patch::MatrixNode>,
                        public LFOSupport<Patch::MatrixNode>,
                        public ModulationSupport<Patch::MatrixNode>
{
    OpSource &onto, &from;

    const Patch::MatrixNode &matrixNode;
    const MonoValues &monoValues;
    const VoiceValues &voiceValues;
    const float &level, &activeV, &pmrmV, &lfoToDepth, &mulLfoV;
    MatrixNodeFrom(const Patch::MatrixNode &mn, OpSource &on, OpSource &fr, MonoValues &mv,
                   const VoiceValues &vv)
        : matrixNode(mn), monoValues(mv), voiceValues(vv), onto(on), from(fr), level(mn.level),
          pmrmV(mn.pmOrRM), activeV(mn.active), EnvelopeSupport(mn, mv, vv), LFOSupport(mn, mv),
          lfoToDepth(mn.lfoToDepth), mulLfoV(mn.envLfoSum), ModulationSupport(mn, mv, vv)
    {
    }

    bool active{false}, isrm{false}, mulLfo{false};

    void attack()
    {
        active = activeV > 0.5;
        isrm = pmrmV > 0.5;
        mulLfo = mulLfoV > 0.5;
        if (active)
        {
            bindModulation();
            calculateModulation();
            envAttack();
            lfoAttack();
        }
    }

    void applyBlock()
    {
        if (!active)
            return;

        calculateModulation();
        envProcess();
        lfoProcess();
        float mod alignas(16)[blockSize], modB alignas(16)[blockSize];
        mech::mul_block<blockSize>(env.outputCache, from.output, mod);
        mech::scale_by<blockSize>(level * depthAtten, mod);

        if (!mulLfo)
        {
            mech::scale_accumulate_from_to<blockSize>(lfo.outputBlock, lfoToDepth * lfoAtten, mod);
        }
        else
        {
            mech::copy_from_to<blockSize>(lfo.outputBlock, modB);
            mech::scale_by<blockSize>(lfoToDepth * lfoAtten, modB);
            mech::mul_block<blockSize>(mod, modB, mod);
        }

        for (int i = 0; i < blockSize; ++i)
        {
            mod[i] += applyMod * from.output[i];
        }
        if (isrm)
        {
            if (onto.rmAssigned)
            {
                mech::accumulate_from_to<blockSize>(mod, onto.rmLevel);
            }
            else
            {
                onto.rmAssigned = true;

                mech::copy_from_to<blockSize>(mod, onto.rmLevel);
            }
        }
        else
        {
            for (int j = 0; j < blockSize; ++j)
            {
                onto.phaseInput[j] += (int32_t)((1 << 27) * (mod[j]));
            }
        }
    }
    float applyMod{0.f};
    float depthAtten{1.0};
    float lfoAtten{1.0};

    void calculateModulation()
    {
        depthAtten = 1.f;
        lfoAtten = 1.f;
        applyMod = 0.f;
        attackMod = 0.f;
        rateMod = 0.f;

        if (!anySources)
            return;

        for (int i = 0; i < numModsPer; ++i)
        {
            if (sourcePointers[i] &&
                (int)matrixNode.modtarget[i].value != Patch::SelfNode::TargetID::NONE)
            {
                // targets: env depth atten, lfo dept atten, direct adjust, env attack, lfo rate
                auto dp = depthPointers[i];
                if (!dp)
                    continue;
                auto d = *dp;
                switch ((Patch::MatrixNode::TargetID)matrixNode.modtarget[i].value)
                {
                case Patch::MatrixNode::DIRECT:
                    applyMod += d * *sourcePointers[i];
                    break;
                case Patch::MatrixNode::DEPTH_ATTEN:
                    depthAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                    break;
                case Patch::MatrixNode::LFO_DEPTH_ATTEN:
                    lfoAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                    break;
                case Patch::MatrixNode::ENV_ATTACK:
                    attackMod += d * *sourcePointers[i];
                    break;
                case Patch::MatrixNode::LFO_RATE:
                    rateMod += d * *sourcePointers[i] * 4;
                    break;
                default:
                    break;
                }
            }
        }
    }
};

struct MatrixNodeSelf : EnvelopeSupport<Patch::SelfNode>,
                        LFOSupport<Patch::SelfNode>,
                        ModulationSupport<Patch::SelfNode>
{
    OpSource &onto;
    SRProvider sr;

    const Patch::SelfNode &selfNode;
    const MonoValues &monoValues;
    const VoiceValues &voiceValues;

    const float &fbBase, &lfoToFB, &activeV, &lfoMulV;
    MatrixNodeSelf(const Patch::SelfNode &sn, OpSource &on, MonoValues &mv, const VoiceValues &vv)
        : selfNode(sn), monoValues(mv), voiceValues(vv), sr(mv), onto(on), fbBase(sn.fbLevel),
          lfoToFB(sn.lfoToFB), activeV(sn.active), lfoMulV(sn.envLfoSum),
          EnvelopeSupport(sn, mv, vv), LFOSupport(sn, mv), ModulationSupport(sn, mv, vv){};
    bool active{true}, lfoMul{false};

    void attack()
    {
        active = activeV > 0.5;
        lfoMul = lfoMulV > 0.5;
        if (active)
        {
            bindModulation();
            calculateModulation();
            envAttack();
            lfoAttack();
        }
    }
    void applyBlock()
    {
        if (!active)
            return;

        calculateModulation();
        envProcess();
        lfoProcess();
        if (lfoMul)
        {
            for (int j = 0; j < blockSize; ++j)
            {
                onto.feedbackLevel[j] =
                    (int32_t)((1 << 24) *
                              (((env.outputCache[j] * lfoAtten * lfoToFB * lfo.outputBlock[j]) *
                                fbBase * depthAtten) +
                               fbMod));
            }
        }
        else
        {
            for (int j = 0; j < blockSize; ++j)
            {
                onto.feedbackLevel[j] =
                    (int32_t)((1 << 24) *
                              (((env.outputCache[j] + lfoAtten * lfoToFB * lfo.outputBlock[j]) *
                                fbBase * depthAtten) +
                               fbMod));
            }
        }
    }

    float fbMod{0.f};
    float depthAtten{1.0};
    float lfoAtten{1.0};

    void calculateModulation()
    {
        depthAtten = 1.f;
        lfoAtten = 1.f;
        fbMod = 0.f;
        attackMod = 0.f;
        rateMod = 0.f;

        if (!anySources)
            return;

        for (int i = 0; i < numModsPer; ++i)
        {
            if (sourcePointers[i] &&
                (int)selfNode.modtarget[i].value != Patch::SelfNode::TargetID::NONE)
            {
                // targets: env depth atten, lfo dept atten, direct adjust, env attack, lfo rate
                auto dp = depthPointers[i];
                if (!dp)
                    continue;
                auto d = *dp;
                switch ((Patch::SelfNode::TargetID)selfNode.modtarget[i].value)
                {
                case Patch::SelfNode::DIRECT:
                    fbMod += d * *sourcePointers[i];
                    break;
                case Patch::SelfNode::DEPTH_ATTEN:
                    depthAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                    break;
                case Patch::SelfNode::LFO_DEPTH_ATTEN:
                    lfoAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                    break;
                case Patch::SelfNode::ENV_ATTACK:
                    attackMod += d * *sourcePointers[i];
                    break;
                case Patch::SelfNode::LFO_RATE:
                    rateMod += d * *sourcePointers[i] * 4;
                    break;
                default:
                    break;
                }
            }
        }
    }
};

struct MixerNode : EnvelopeSupport<Patch::MixerNode>,
                   LFOSupport<Patch::MixerNode>,
                   ModulationSupport<Patch::MixerNode>
{
    float output alignas(16)[2][blockSize];
    OpSource &from;
    SRProvider sr;

    const Patch::MixerNode &mixerNode;
    const MonoValues &monoValues;
    const VoiceValues &voiceValues;

    const float &level, &activeF, &pan, &lfoToLevel, &lfoToPan, &lfoMulV;
    bool active{false}, lfoMul{false};

    MixerNode(const Patch::MixerNode &mn, OpSource &f, MonoValues &mv, const VoiceValues &vv)
        : mixerNode(mn), monoValues(mv), voiceValues(vv), sr(mv), from(f), pan(mn.pan),
          level(mn.level), activeF(mn.active), lfoToLevel(mn.lfoToLevel), lfoToPan(mn.lfoToPan),
          EnvelopeSupport(mn, mv, vv), LFOSupport(mn, mv), ModulationSupport(mn, mv, vv),
          lfoMulV(mn.envLfoSum)
    {
        memset(output, 0, sizeof(output));
    }

    void attack()
    {
        lfoMul = lfoMulV > 0.5;
        active = activeF > 0.5;
        memset(output, 0, sizeof(output));
        if (active)
        {
            bindModulation();
            calculateModulation();
            envAttack();
            lfoAttack();
        }
    }

    void renderBlock()
    {
        if (!active)
        {
            return;
        }
        float vSum alignas(16)[blockSize];

        calculateModulation();
        envProcess();
        lfoProcess();

        if (lfoMul)
        {
            mech::scale_by<blockSize>(env.outputCache, lfo.outputBlock);
        }

        auto lv = std::clamp(level + levMod, 0.f, 1.f) * depthAtten;
        for (int j = 0; j < blockSize; ++j)
        {
            // use mech blah
            auto amp = level * env.outputCache[j];
            // LFO is bipolar so
            auto uniLFO = (lfo.outputBlock[j] + 1) * 0.5;
            // attenuate amplitude by the LFO
            amp *= (lfoToLevel * uniLFO) + (1 - lfoToLevel);
            vSum[j] = lv * (env.outputCache[j] + lfoAtten * lfoToLevel * lfo.outputBlock[j]) *
                      from.output[j];
        }

        auto pn = std::clamp(pan + lfoPanAtten * lfoToPan * lfo.outputBlock[blockSize - 1] +
                                 voiceValues.uniPanShift + panMod,
                             -1.f, 1.f);
        if (pn != 0.f)
        {
            pn = (pn + 1) * 0.5;
            sdsp::pan_laws::panmatrix_t pmat;
            sdsp::pan_laws::monoEqualPower(pn, pmat);

            mech::mul_block<blockSize>(vSum, pmat[0], output[0]);
            mech::mul_block<blockSize>(vSum, pmat[3], output[1]);
        }
        else
        {
            mech::copy_from_to<blockSize>(vSum, output[0]);
            mech::copy_from_to<blockSize>(vSum, output[1]);
        }
    }

    float levMod{0.f};
    float depthAtten{1.0};
    float lfoAtten{1.0};
    float lfoPanAtten{1.0};
    float panMod{0.0};

    void calculateModulation()
    {
        depthAtten = 1.f;
        lfoAtten = 1.f;
        levMod = 0.f;
        attackMod = 0.f;
        rateMod = 0.f;
        panMod = 0.f;
        lfoPanAtten = 1.f;

        if (!anySources)
            return;

        for (int i = 0; i < numModsPer; ++i)
        {
            if (sourcePointers[i] &&
                (int)mixerNode.modtarget[i].value != Patch::MixerNode::TargetID::NONE)
            {
                // targets: env depth atten, lfo dept atten, direct adjust, env attack, lfo rate
                auto dp = depthPointers[i];
                if (!dp)
                    continue;
                auto d = *dp;
                switch ((Patch::MixerNode::TargetID)mixerNode.modtarget[i].value)
                {
                case Patch::MixerNode::DIRECT:
                    levMod += d * *sourcePointers[i];
                    break;
                case Patch::MixerNode::PAN:
                    panMod += d * *sourcePointers[i];
                    break;
                case Patch::MixerNode::DEPTH_ATTEN:
                    depthAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                    break;
                case Patch::MixerNode::LFO_DEPTH_ATTEN:
                    lfoAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                    break;
                case Patch::MixerNode::LFO_DEPTH_PAN_ATTEN:
                    lfoPanAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                    break;
                case Patch::MixerNode::ENV_ATTACK:
                    attackMod += d * *sourcePointers[i];
                    break;
                case Patch::MixerNode::LFO_RATE:
                    rateMod += d * *sourcePointers[i] * 4;
                    break;
                default:
                    break;
                }
            }
        }
    }
};

struct OutputNode : EnvelopeSupport<Patch::OutputNode>, ModulationSupport<Patch::OutputNode>
{
    float output alignas(16)[2][blockSize];
    std::array<MixerNode, numOps> &fromArr;
    SRProvider sr;

    const Patch::OutputNode &outputNode;

    const MonoValues &monoValues;
    const VoiceValues &voiceValues;

    const float &level, &velSen, &bendUp, &bendDown, &octTranspose;
    const float &defTrigV;
    TriggerMode defaultTrigger;

    OutputNode(const Patch::OutputNode &on, std::array<MixerNode, numOps> &f, MonoValues &mv,
               const VoiceValues &vv)
        : outputNode(on), ModulationSupport(on, mv, vv), monoValues(mv), voiceValues(vv), sr(mv),
          fromArr(f), level(on.level), bendUp(on.bendUp), bendDown(on.bendDown),
          octTranspose(on.octTranspose), velSen(on.velSensitivity), EnvelopeSupport(on, mv, vv),
          defTrigV(on.defaultTrigger)
    {
        memset(output, 0, sizeof(output));
        allowVoiceTrigger = false;
    }

    void attack()
    {
        defaultTrigger = (TriggerMode)std::round(defTrigV);
        bindModulation();
        calculateModulation();
        envAttack();
    }

    float finalEnvLevel alignas(16)[blockSize];
    void renderBlock()
    {
        calculateModulation();
        for (const auto &from : fromArr)
        {
            mech::accumulate_from_to<blockSize>(from.output[0], output[0]);
            mech::accumulate_from_to<blockSize>(from.output[1], output[1]);
        }

        envProcess(false);
        mech::copy_from_to<blockSize>(env.outputCache, finalEnvLevel);
        mech::scale_by<blockSize>(depthAtten, finalEnvLevel);
        mech::scale_by<blockSize>(finalEnvLevel, output[0], output[1]);

        // Apply main output
        auto lv = level;
        auto v = 1.f - velSen * (1.f - voiceValues.velocity);
        lv = 0.15 * std::clamp(v * lv * lv * lv, 0.f, 1.f);
        mech::scale_by<blockSize>(lv, output[0], output[1]);

        auto pn = panMod;
        if (pn != 0.f)
        {
            pn = (pn + 1) * 0.5;
            sdsp::pan_laws::panmatrix_t pmat;
            sdsp::pan_laws::stereoEqualPower(pn, pmat);

            for (int i = 0; i < blockSize; ++i)
            {
                auto oL = output[0][i];
                auto oR = output[1][i];

                output[0][i] = pmat[0] * oL + pmat[2] * oR;
                output[1][i] = pmat[3] * oL + pmat[1] * oR;
            }
        }

#if DEBUG_LEVELS
        for (int i = 0; i < blockSize; ++i)
        {
            if (std::fabs(output[0][i]) > 1 || std::fabs(output[1][i]) > 1)
            {
                SXSNLOG(i << " " << output[0][i] << " " << output[1][i]);
            }
        }
#endif
    }

    float panMod{0.f};
    float depthAtten{1.0};

    void calculateModulation()
    {
        depthAtten = 1.f;
        attackMod = 0.f;
        panMod = 0.f;

        if (!anySources)
            return;

        for (int i = 0; i < numModsPer; ++i)
        {
            if (sourcePointers[i] &&
                (int)outputNode.modtarget[i].value != Patch::MixerNode::TargetID::NONE)
            {
                // targets: env depth atten, lfo dept atten, direct adjust, env attack, lfo rate
                auto dp = depthPointers[i];
                if (!dp)
                    continue;
                auto d = *dp;
                switch ((Patch::OutputNode::TargetID)outputNode.modtarget[i].value)
                {
                case Patch::OutputNode::PAN:
                    panMod += d * *sourcePointers[i];
                    break;
                case Patch::OutputNode::DEPTH_ATTEN:
                    depthAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                    break;
                case Patch::OutputNode::ENV_ATTACK:
                    attackMod += d * *sourcePointers[i];
                    break;
                default:
                    break;
                }
            }
        }
    }
};
} // namespace baconpaul::six_sines

#endif // MATRIX_NODE_H
