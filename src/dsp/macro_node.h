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

#ifndef BACONPAUL_SIX_SINES_DSP_MACRO_NODE_H
#define BACONPAUL_SIX_SINES_DSP_MACRO_NODE_H

#include <algorithm>
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "dsp/node_support.h"
#include "synth/patch.h"
#include "synth/mono_values.h"
#include "synth/voice_values.h"

namespace baconpaul::six_sines
{
namespace mech = sst::basic_blocks::mechanics;

/*
 * MacroVoiceNode — per-voice node that processes one macro's envelope + LFO + modulation
 * when the macro's power button (macroPower) is enabled. When power is off, the node
 * simply mirrors the static mono level.
 */
struct MacroVoiceNode : EnvelopeSupport<Patch::MacroNode>,
                        LFOSupport<MacroVoiceNode, Patch::MacroNode>,
                        ModulationSupport<Patch::MacroNode, MacroVoiceNode>
{
    const Patch::MacroNode &macroNode;
    const MonoValues &monoValues;
    const VoiceValues &voiceValues;
    const float &level;
    const float &macroPowerV; // mn.macroPower — distinct from EnvelopeSupport::powerV (envPower)
    const float &envDepth;
    const float &lfoDepth;

    float out{0.f};
    bool macroPowerOn{false};
    bool wasPowerOn{false};

    MacroVoiceNode(const Patch::MacroNode &mn, MonoValues &mv, const VoiceValues &vv)
        : macroNode(mn), monoValues(mv), voiceValues(vv), level(mn.level),
          macroPowerV(mn.macroPower), envDepth(mn.envDepth), lfoDepth(mn.lfoDepth),
          ModulationSupport(mn, this, mv, vv), EnvelopeSupport(mn, mv, vv), LFOSupport(mn, mv)
    {
    }

    void attack()
    {
        resetModulation();
        envResetMod();
        lfoResetMod();
        bindModulation();
        macroPowerOn = macroPowerV > 0.5f;
        wasPowerOn = macroPowerOn;
        if (macroPowerOn)
        {
            calculateModulation();
            envAttack();
            lfoAttack();
        }
    }

    void process()
    {
        if (!macroPowerOn)
        {
            out = level;
            return;
        }
        calculateModulation();
        envProcess();
        lfoProcess();
        // outputCache, not outBlock0: envAttack's constantEnv path (the macro
        // default) writes outputCache directly and never sets outBlock0.
        auto envOut = env.outputCache[blockSize - 1];
        auto lfoOut = lfo.outputBlock[blockSize - 1];
        if (lfoIsEnveloped)
            lfoOut *= envOut;
        auto amp = std::clamp(level + levMod, -1.f, 1.f);
        auto lfoContrib = lfoOut * lfoDepth * lfoAtten;
        float result;
        if (envIsMult)
            result = amp * envOut * depthAtten + lfoContrib;
        else
            result = amp + envDepth * envOut * depthAtten + lfoContrib;
        out = std::clamp(result, -1.f, 1.f);
    }

    float levMod{0.f};
    float depthAtten{1.f};
    float lfoAtten{1.f};

    void resetModulation()
    {
        levMod = 0.f;
        depthAtten = 1.f;
        lfoAtten = 1.f;
    }

    void calculateModulation()
    {
        resetModulation();
        envResetMod();
        lfoResetMod();

        if (!anySources)
            return;

        for (int i = 0; i < numModsPer; ++i)
        {
            if (sourcePointers[i] &&
                (int)macroNode.modtarget[i].value != Patch::MacroNode::TargetID::NONE)
            {
                auto dp = depthPointers[i];
                if (!dp)
                    continue;
                auto d = *dp;

                auto handled = envHandleModulationValue((int)macroNode.modtarget[i].value, d,
                                                        sourcePointers[i]) ||
                               lfoHandleModulationValue((int)macroNode.modtarget[i].value, d,
                                                        sourcePointers[i]);

                if (!handled)
                {
                    switch ((Patch::MacroNode::TargetID)macroNode.modtarget[i].value)
                    {
                    case Patch::MacroNode::LEVEL:
                        levMod += d * *sourcePointers[i];
                        break;
                    case Patch::MacroNode::DEPTH_ATTEN:
                        depthAtten *= 1.0 - d * (1.0 - std::clamp(*sourcePointers[i], 0.f, 1.f));
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }

    bool checkLfoUsed() { return lfoDepth != 0 || lfoUsedAsModulationSource; }
};

} // namespace baconpaul::six_sines
#endif // BACONPAUL_SIX_SINES_DSP_MACRO_NODE_H
