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
#include "remap_functions.h"
#include "resonant_window.h"
#include "noise_helper.h"

namespace baconpaul::six_sines
{
struct alignas(16) OpSource : public EnvelopeSupport<Patch::SourceNode>,
                              LFOSupport<OpSource, Patch::SourceNode, false>,
                              ModulationSupport<Patch::SourceNode, OpSource>
{
    int32_t phaseInput alignas(16)[blockSize];
    int32_t feedbackLevel alignas(16)[blockSize];
    float rmLevel alignas(16)[blockSize];
    float fmAmount alignas(16)[blockSize]; // in hz
    bool rmAssigned{false};
    // Per-block signal from MatrixNodeSelf::applyBlock: true when self-feedback is
    // active for this block (so feedbackLevel[] may be non-zero). The inner-loop
    // dispatcher uses this to pick a no-FB template instantiation that skips the
    // feedback math AND the fbv[] shift entirely — removing the per-sample
    // backward dependency unlocks a lot more compiler reordering.
    bool hasActiveFeedback{false};
    int opIndex{0}; // set by Voice constructor; 0 = op1 which can use AUDIO_IN

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

    uint32_t phase{0};
    int32_t dPhase{0};

    static constexpr float centsScale{1.0 / (12 * 100)};

    // Latch-at-attack cache. The dispatch in innerLoop() and the AUDIO_IN check
    // in renderBlock() used to round+cast the patch .value every block. They
    // read these typed members instead, which are populated by cacheEnums() in
    // reset() and stay stable for the life of the note. Mid-note mode changes
    // don't take effect until retrigger — same as the heavy state (stWindow,
    // extendedLagM/N) already did.
    SinTable::WaveForm waveFormCachedAtAttack{SinTable::SIN};
    bool isAudioInCachedAtAttack{false};
    Patch::SourceNode::ExtendedMode extendedModeCachedAtAttack{
        Patch::SourceNode::ExtendedMode::NONE};
    Patch::SourceNode::PhaseMapShape phaseMapShapeCachedAtAttack{
        Patch::SourceNode::PhaseMapShape::SAW};
    Patch::SourceNode::ResonantSweepWindow resonantSweepWindowCachedAtAttack{
        Patch::SourceNode::ResonantSweepWindow::SAW};
    Patch::SourceNode::NoiseMode noiseModeCachedAtAttack{
        Patch::SourceNode::NoiseMode::ADD_TO_PHASE};
    Patch::SourceNode::NoiseType noiseTypeCachedAtAttack{Patch::SourceNode::NoiseType::PINK};
    Patch::SourceNode::LFSRMode lfsrModeCachedAtAttack{Patch::SourceNode::LFSRMode::LONG_KEYTRACK};
    float resonantSweepKScaleCachedAtAttack{1.0f};

    void cacheEnums()
    {
        using EM = Patch::SourceNode::ExtendedMode;
        using PM = Patch::SourceNode::PhaseMapShape;
        using RW = Patch::SourceNode::ResonantSweepWindow;
        using RFD = Patch::SourceNode::ResonantSweepFrequencyDepth;
        using NM = Patch::SourceNode::NoiseMode;
        using NT = Patch::SourceNode::NoiseType;
        using LM = Patch::SourceNode::LFSRMode;

        waveFormCachedAtAttack =
            static_cast<SinTable::WaveForm>(static_cast<uint32_t>(std::round(waveForm)));
        isAudioInCachedAtAttack = (waveFormCachedAtAttack == SinTable::AUDIO_IN);

        extendedModeCachedAtAttack =
            static_cast<EM>(static_cast<uint32_t>(std::round(sourceNode.extendedModeMode.value)));
        phaseMapShapeCachedAtAttack =
            static_cast<PM>(static_cast<uint32_t>(std::round(sourceNode.phaseMapModeShape.value)));
        resonantSweepWindowCachedAtAttack = static_cast<RW>(
            static_cast<uint32_t>(std::round(sourceNode.resonantSweepWindowShape.value)));

        auto rfd = static_cast<RFD>(
            static_cast<uint32_t>(std::round(sourceNode.resonantSweepFrequencyDepth.value)));
        switch (rfd)
        {
        case RFD::TWO:
            resonantSweepKScaleCachedAtAttack = 2.0f;
            break;
        case RFD::FOUR:
            resonantSweepKScaleCachedAtAttack = 4.0f;
            break;
        case RFD::TEN:
            resonantSweepKScaleCachedAtAttack = 10.0f;
            break;
        }

        noiseModeCachedAtAttack =
            static_cast<NM>(static_cast<uint32_t>(std::round(sourceNode.noiseMode.value)));
        noiseTypeCachedAtAttack =
            static_cast<NT>(static_cast<uint32_t>(std::round(sourceNode.noiseType.value)));
        lfsrModeCachedAtAttack =
            static_cast<LM>(static_cast<uint32_t>(std::round(sourceNode.lfsrMode.value)));
    }

    OpSource(const Patch::SourceNode &sn, MonoValues &mv, const VoiceValues &vv)
        : sourceNode(sn), monoValues(mv), voiceValues(vv), EnvelopeSupport(sn, mv, vv),
          LFOSupport(sn, mv), ModulationSupport(sn, this, mv, vv), ratio(sn.ratio),
          activeV(sn.active), envToRatio(sn.envToRatio), lfoToRatio(sn.lfoToRatio),
          waveForm(sn.waveForm), kt(sn.keyTrack), ktv(sn.keyTrackValue),
          ktlo(sn.keyTrackValueIsLow), ktlov(sn.keyTrackLowFrequencyValue),
          startPhase(sn.startingPhase), octTranspose(sn.octTranspose),
          lfoToRatioFine(sn.lfoToRatioFine), envToRatioFine(sn.envToRatioFine),
          noiseHelper(mv.rng, mv.dbToLinear)
    {
        reset();
    }

    float *lfoFacP{nullptr};
    float one{1.f}; // to point to

    void reset()
    {
        resetModulation();
        envResetMod();
        lfoResetMod();

        // Latch all mode/shape enums up front; reset() and renderBlock() / innerLoop()
        // both read the cached typed members from here on.
        cacheEnums();

        st.setSampleRate(monoValues.sr.sampleRate);
        stWindow.setSampleRate(monoValues.sr.sampleRate);
        // Pick the table for the resonant-sweep window. BLACKMAN_HARRIS and TUKEY pull
        // their own tables; everything else (closed-form windows + HANN) defaults to
        // HANN so a mid-note shape change doesn't read a stale unrelated table — the
        // closed-form arms in the inner loop don't read stWindow at all.
        {
            using RW = Patch::SourceNode::ResonantSweepWindow;
            switch (resonantSweepWindowCachedAtAttack)
            {
            case RW::BLACKMAN_HARRIS:
                stWindow.setWaveForm(SinTable::BLACKMAN_HARRIS_WINDOW);
                break;
            case RW::TUKEY:
                stWindow.setWaveForm(SinTable::TUKEY_WINDOW);
                break;
            default:
                stWindow.setWaveForm(SinTable::HANN_WINDOW);
                break;
            }
        }
        firstTime = true;
        extendedMPrior = sourceNode.extendedModeM.value;
        // Configure the M/N lags only if the operator is actually using an extended mode
        // that consumes them. In NONE the lag members exist but are never touched.
        {
            using EM = Patch::SourceNode::ExtendedMode;
            if (extendedModeCachedAtAttack == EM::PHASE_REMAP ||
                extendedModeCachedAtAttack == EM::RESONANT_SWEEP ||
                extendedModeCachedAtAttack == EM::NOISE)
            {
                extendedLagM.setRateInMilliseconds(10, monoValues.sr.samplerate, blockSizeInv);
                extendedLagM.snapTo(sourceNode.extendedModeM.value);
            }
            if (extendedModeCachedAtAttack == EM::NOISE)
            {
                extendedLagN.setRateInMilliseconds(10, monoValues.sr.samplerate, blockSizeInv);
                extendedLagN.snapTo(sourceNode.extendedModeN.value);
            }
        }
        noiseHelper.setSampleRate(monoValues.sr.sampleRate);
        noiseHelper.warmup();
        noisePos = 16;
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
            st.setWaveForm(waveFormCachedAtAttack);

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

    bool checkLfoUsed()
    {
        auto used = lfoUsedAsModulationSource;
        used = used || (lfoToRatio != 0);
        used = used || (lfoToRatioFine != 0);

        // Extended-mode targets — only consume the LFO when the mode that uses them is active.
        using EM = Patch::SourceNode::ExtendedMode;
        if (extendedModeCachedAtAttack == EM::PHASE_REMAP ||
            extendedModeCachedAtAttack == EM::RESONANT_SWEEP ||
            extendedModeCachedAtAttack == EM::NOISE)
            used = used || (sourceNode.lfoToExtendedModeM.value != 0);
        if (extendedModeCachedAtAttack == EM::NOISE)
            used = used || (sourceNode.lfoToExtendedModeN.value != 0);

        return used;
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
            fmAmount[i] = 0.f;
        }
        rmAssigned = false;
        hasActiveFeedback = false;
    }

    void clearOutputs() { memset(output, 0, sizeof(output)); }

    void snapActive() { active = activeV > 0.5 || monoValues.designModeRunAll; }

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
    float extendedMPrior{0.f};
    float extendedMMod{0.f}, extendedNMod{0.f};

    // Per-block one-pole smoothers for extended-mode M and N. Unlike the LFO smoother
    // these are *required* in extended mode — M/N have jumps would alias loudly.
    sst::basic_blocks::dsp::OnePoleLag<float, false> extendedLagM, extendedLagN;

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

        if (isAudioInCachedAtAttack)
        {
            if (opIndex == 0)
            {
                for (int i = 0; i < blockSize; ++i)
                    output[i] = monoValues.audioInBlock[i];
            }
            else
            {
                memset(output, 0, sizeof(output));
            }
            fbVal[0] = fbVal[1] = 0.f;
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
        auto dRF = (rf - priorRF) / blockSize;
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
    uint32_t softPhase{0};
    float softFb[2]{0.f, 0.f};
    void softResetPhase()
    {
        softPhase = 4 << 27;
        softFb[0] = 0.f;
        softFb[1] = 0.f;
        softResetPhaseCount = softPhaseCount;
    }

    void innerLoop(float *onto, float *fbv, float rf, const float dRF, uint32_t &phs)
    {
        // Split on per-block self-feedback so we pick the right template
        // instantiation of innerLoopImpl. The flag is set by
        // MatrixNodeSelf::applyBlock and stays false when self-FB is off for
        // this op (the common case for modulator stacks) — the UsesFB=false
        // path then skips the feedback math entirely.
        if (hasActiveFeedback)
            innerLoopDispatch<true>(onto, fbv, rf, dRF, phs);
        else
            innerLoopDispatch<false>(onto, fbv, rf, dRF, phs);
    }

    template <bool UsesFB>
    void innerLoopDispatch(float *onto, float *fbv, float rf, const float dRF, uint32_t &phs)
    {
        using EM = Patch::SourceNode::ExtendedMode;
        using PM = Patch::SourceNode::PhaseMapShape;
        switch (extendedModeCachedAtAttack)
        {
        case EM::NONE:
            innerLoopImpl<UsesFB, EM::NONE>(onto, fbv, rf, dRF, phs);
            break;
        case EM::PHASE_REMAP:
        {
            switch (phaseMapShapeCachedAtAttack)
            {
            case PM::SAW:
                innerLoopImpl<UsesFB, EM::PHASE_REMAP, PM::SAW>(onto, fbv, rf, dRF, phs);
                break;
            case PM::SQUARE:
                innerLoopImpl<UsesFB, EM::PHASE_REMAP, PM::SQUARE>(onto, fbv, rf, dRF, phs);
                break;
            case PM::PULSE:
                innerLoopImpl<UsesFB, EM::PHASE_REMAP, PM::PULSE>(onto, fbv, rf, dRF, phs);
                break;
            case PM::DOUBLE:
                innerLoopImpl<UsesFB, EM::PHASE_REMAP, PM::DOUBLE>(onto, fbv, rf, dRF, phs);
                break;
            case PM::SIN_TO_SQUARE:
                innerLoopImpl<UsesFB, EM::PHASE_REMAP, PM::SIN_TO_SQUARE>(onto, fbv, rf, dRF, phs);
                break;
            case PM::DOUBLE_SAW:
                innerLoopImpl<UsesFB, EM::PHASE_REMAP, PM::DOUBLE_SAW>(onto, fbv, rf, dRF, phs);
                break;
            }
            break;
        }
        case EM::RESONANT_SWEEP:
        {
            using RW = Patch::SourceNode::ResonantSweepWindow;
            // The PhaseMapShape template arg is unused on this branch; leave it at default.
            switch (resonantSweepWindowCachedAtAttack)
            {
            case RW::SAW:
                innerLoopImpl<UsesFB, EM::RESONANT_SWEEP, Patch::SourceNode::PhaseMapShape::SAW,
                              RW::SAW>(onto, fbv, rf, dRF, phs, resonantSweepKScaleCachedAtAttack);
                break;
            case RW::TRIANGLE:
                innerLoopImpl<UsesFB, EM::RESONANT_SWEEP, Patch::SourceNode::PhaseMapShape::SAW,
                              RW::TRIANGLE>(onto, fbv, rf, dRF, phs,
                                            resonantSweepKScaleCachedAtAttack);
                break;
            case RW::TRAPEZOID:
                innerLoopImpl<UsesFB, EM::RESONANT_SWEEP, Patch::SourceNode::PhaseMapShape::SAW,
                              RW::TRAPEZOID>(onto, fbv, rf, dRF, phs,
                                             resonantSweepKScaleCachedAtAttack);
                break;
            case RW::FULLTRAP:
                innerLoopImpl<UsesFB, EM::RESONANT_SWEEP, Patch::SourceNode::PhaseMapShape::SAW,
                              RW::FULLTRAP>(onto, fbv, rf, dRF, phs,
                                            resonantSweepKScaleCachedAtAttack);
                break;
            case RW::HANN:
                innerLoopImpl<UsesFB, EM::RESONANT_SWEEP, Patch::SourceNode::PhaseMapShape::SAW,
                              RW::HANN>(onto, fbv, rf, dRF, phs, resonantSweepKScaleCachedAtAttack);
                break;
            case RW::BLACKMAN_HARRIS:
                innerLoopImpl<UsesFB, EM::RESONANT_SWEEP, Patch::SourceNode::PhaseMapShape::SAW,
                              RW::BLACKMAN_HARRIS>(onto, fbv, rf, dRF, phs,
                                                   resonantSweepKScaleCachedAtAttack);
                break;
            case RW::TUKEY:
                innerLoopImpl<UsesFB, EM::RESONANT_SWEEP, Patch::SourceNode::PhaseMapShape::SAW,
                              RW::TUKEY>(onto, fbv, rf, dRF, phs,
                                         resonantSweepKScaleCachedAtAttack);
                break;
            }
            break;
        }
        case EM::NOISE:
        {
            using NM = Patch::SourceNode::NoiseMode;
            constexpr auto PMSAW = Patch::SourceNode::PhaseMapShape::SAW;
            constexpr auto RWSAW = Patch::SourceNode::ResonantSweepWindow::SAW;
            switch (noiseModeCachedAtAttack)
            {
            case NM::ADD_TO_PHASE:
                innerLoopImpl<UsesFB, EM::NOISE, PMSAW, RWSAW, NM::ADD_TO_PHASE>(onto, fbv, rf, dRF,
                                                                                 phs);
                break;
            case NM::ADD_TO_SIGNAL:
                innerLoopImpl<UsesFB, EM::NOISE, PMSAW, RWSAW, NM::ADD_TO_SIGNAL>(onto, fbv, rf,
                                                                                  dRF, phs);
                break;
            case NM::MIX_WITH_SIGNAL:
                innerLoopImpl<UsesFB, EM::NOISE, PMSAW, RWSAW, NM::MIX_WITH_SIGNAL>(onto, fbv, rf,
                                                                                    dRF, phs);
                break;
            case NM::MUL_BY_SIGNAL:
                innerLoopImpl<UsesFB, EM::NOISE, PMSAW, RWSAW, NM::MUL_BY_SIGNAL>(onto, fbv, rf,
                                                                                  dRF, phs);
                break;
            case NM::MUL_BY_UNI_SIGNAL:
                innerLoopImpl<UsesFB, EM::NOISE, PMSAW, RWSAW, NM::MUL_BY_UNI_SIGNAL>(onto, fbv, rf,
                                                                                      dRF, phs);
                break;
            }
            break;
        }
        }
    }

    template <
        bool UsesFB, Patch::SourceNode::ExtendedMode ET,
        Patch::SourceNode::PhaseMapShape S = Patch::SourceNode::PhaseMapShape::SAW,
        Patch::SourceNode::ResonantSweepWindow R = Patch::SourceNode::ResonantSweepWindow::SAW,
        Patch::SourceNode::NoiseMode NM = Patch::SourceNode::NoiseMode::ADD_TO_PHASE>
    void innerLoopImpl(float *onto, float *fbv, float rf, const float dRF, uint32_t &phs,
                       float kScale = 1.0f)
    {
        using EM = Patch::SourceNode::ExtendedMode;
        using NMode = Patch::SourceNode::NoiseMode;
        using NT = Patch::SourceNode::NoiseType;
        using LM = Patch::SourceNode::LFSRMode;
        float nextM{0.f}, dM{0.f};
        float nextN{0.f};
        NT noiseType{NT::PINK};
        LM lfsrMode{LM::LONG_KEYTRACK};
        if constexpr (ET == EM::PHASE_REMAP || ET == EM::RESONANT_SWEEP || ET == EM::NOISE)
        {
            // Raw target m for this block: patch value + external mod + env / lfo contributions.
            auto lfoFac = *lfoFacP;
            float target = sourceNode.extendedModeM.value + extendedMMod +
                           sourceNode.envToExtendedModeM.value * env.outputCache[blockSize - 1] +
                           lfoFac * sourceNode.lfoToExtendedModeM.value * lfo.outputBlock[0];
            // Push through the one-pole smoother before the block-walk. M cannot tolerate
            // jumps the way ratio can (the remap math hard-clamps the shape), so we always
            // smooth here. Then walk linearly from prior block's smoothed value to this one.
            extendedLagM.setTarget(target);
            extendedLagM.process();
            nextM = extendedLagM.v;
            dM = (nextM - extendedMPrior) / blockSize;
            std::swap(nextM, extendedMPrior);
        }
        if constexpr (ET == EM::NOISE)
        {
            auto lfoFac = *lfoFacP;
            float targetN = sourceNode.extendedModeN.value + extendedNMod +
                            sourceNode.envToExtendedModeN.value * env.outputCache[blockSize - 1] +
                            lfoFac * sourceNode.lfoToExtendedModeN.value * lfo.outputBlock[0];
            extendedLagN.setTarget(targetN);
            extendedLagN.process();
            nextN = extendedLagN.v;
            noiseType = noiseTypeCachedAtAttack;
            lfsrMode = lfsrModeCachedAtAttack;
        }

        for (int i = 0; i < blockSize; ++i)
        {
            dPhase = st.dPhase((baseFrequency * (1.0 + fmAmount[i])) * rf);
            rf += dRF;

            phs += dPhase;
            // When self-feedback is inactive for this block, skip the fb math
            // entirely (constexpr-out). When active, use an int compare for the
            // sign bit instead of std::signbit on int32_t — std::signbit's
            // integral overload promotes to double per C++11 [c.math.fpclass],
            // which is implementation-defined cost. `ph` then feeds every
            // extended-mode transform downstream, so this gating must be at the
            // top of the per-sample loop, not around EM::NONE only.
            uint32_t ph{0};
            if constexpr (UsesFB)
            {
                auto fb = 0.5 * (fbv[0] + fbv[1]);
                auto sb = (feedbackLevel[i] < 0);
                // fb = sb ? fb * fb : fb. Ugh a branch. but bool = 0/1, so
                // (1-sb) * fb + sb * fb * fb - 3 mul, 2 add
                // fb - sb * fb + sb * fb * fb - 3 nul 2 add
                // fb * ( 1 - sb * ( 1 - fb)) - 2 mul 2 add
                fb = fb * (1 - sb * (1 - fb));
                ph = phs + phaseInput[i] + (int32_t)(feedbackLevel[i] * fb);
            }
            else
            {
                ph = phs + phaseInput[i];
            }

            float out;
            if constexpr (ET == EM::PHASE_REMAP)
            {
                using PM = Patch::SourceNode::PhaseMapShape;
                if constexpr (S == PM::SAW)
                    ph = remap::remapSaw(ph & phase::phaseMask, nextM);
                else if constexpr (S == PM::SQUARE)
                    ph = remap::remapSquare(ph & phase::phaseMask, nextM);
                else if constexpr (S == PM::PULSE)
                    ph = remap::remapPulse(ph & phase::phaseMask, nextM);
                else if constexpr (S == PM::DOUBLE)
                    ph = remap::remapDoubleSine(ph & phase::phaseMask, nextM);
                else if constexpr (S == PM::SIN_TO_SQUARE)
                    ph = remap::remapSinToSquare(ph & phase::phaseMask, nextM);
                else if constexpr (S == PM::DOUBLE_SAW)
                    ph = remap::remapDoubleSaw(ph & phase::phaseMask, nextM);
                nextM += dM;
                out = st.at(ph);
            }
            else if constexpr (ET == EM::RESONANT_SWEEP)
            {
                using RW = Patch::SourceNode::ResonantSweepWindow;
                auto wph = ph & phase::phaseMask;
                float window;
                if constexpr (R == RW::TRIANGLE)
                    window = resonant_window::windowTriangle(wph);
                else if constexpr (R == RW::TRAPEZOID)
                    window = resonant_window::windowTrapezoid(wph);
                else if constexpr (R == RW::FULLTRAP)
                    window = resonant_window::windowFullTrapezoid(wph);
                else if constexpr (R == RW::HANN || R == RW::BLACKMAN_HARRIS || R == RW::TUKEY)
                    window = stWindow.at(wph);
                else // RW::SAW and any unhandled future value — keep `window` defined.
                    window = resonant_window::windowSaw(wph);
                // Inner phase runs at k(m)x the fundamental and natural-wraps at the
                // fundamental boundary via uint32 multiplication (mod phaseMax).
                // kScale is the patch-selected sweep depth (2 / 4 / 10).
                auto kFactor = kScale * nextM + 1.0f;
                uint32_t kmph = static_cast<uint32_t>(static_cast<float>(wph) * kFactor);
                nextM += dM;
                out = window * st.at(kmph);
            }
            else if constexpr (ET == EM::NOISE)
            {
                if (noisePos >= 16)
                {
                    noiseHelper.fill16(noiseBuf, noiseType, nextN, baseFrequency, lfsrMode);
                    noisePos = 0;
                }
                float noise = noiseBuf[noisePos++];
                float m = nextM;
                nextM += dM;
                if constexpr (NM == NMode::ADD_TO_PHASE)
                {
                    ph += static_cast<int32_t>(m * noise * phase::phaseMaxF *
                                               Patch::SourceNode::noisePhaseScale);
                    out = st.at(ph);
                }
                else if constexpr (NM == NMode::ADD_TO_SIGNAL)
                {
                    out = st.at(ph) + m * noise;
                }
                else if constexpr (NM == NMode::MUL_BY_SIGNAL)
                {
                    out = st.at(ph) * (1.f + m * noise);
                }
                else if constexpr (NM == NMode::MUL_BY_UNI_SIGNAL)
                {
                    auto u = (st.at(ph) + 1.f) * 0.5f;
                    out = u * (1.f + m * noise) * 2.f - 1.f;
                }
                else // MIX_WITH_SIGNAL
                {
                    auto base = st.at(ph);
                    out = (1.f - m) * base + m * noise;
                }
            }
            else
            {
                out = st.at(ph);
            }

            out = out * rmLevel[i];
            onto[i] = out;
            if constexpr (UsesFB)
            {
                fbv[1] = fbv[0];
                fbv[0] = out;
            }
        }
    }

    void resetModulation()
    {
        envRatioAtten = 1.f;
        lfoRatioAtten = 1.f;
        ratioMod = 0.f;
        phaseMod = 0.f;
        extendedMMod = 0.f;
        extendedNMod = 0.f;
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
                    case Patch::SourceNode::DIRECT_FINE:
                        ratioMod += d * *sourcePointers[i] * 2.0 / 12.0;
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
                    case Patch::SourceNode::EXTEND_M:
                        extendedMMod += d * *sourcePointers[i];
                        break;
                    case Patch::SourceNode::EXTEND_N:
                        extendedNMod += d * *sourcePointers[i];
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }

    SinTable st;
    // Used by RESONANT_SWEEP for table-based windows (Hann / Blackman-Harris / Tukey).
    // Independent of `st` so the operator's main waveform stays selectable freely.
    SinTable stWindow;
    float fbVal[2]{0.f, 0.f};

    // 16-sample noise buffer drained across two blocks (blockSize = 8). NoiseHelper
    // is seeded once at construction; not re-seeded on note retrigger.
    NoiseHelper noiseHelper;
    float noiseBuf alignas(16)[16]{};
    int noisePos{16};
};
} // namespace baconpaul::six_sines

#endif
