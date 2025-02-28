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

#include "synth/synth.h"
#include "sst/cpputils/constructors.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/dsp/PanLaws.h"

#include "libMTSClient.h"

namespace baconpaul::six_sines
{

int debugLevel{0};

namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;

Synth::Synth(bool mo)
    : isMultiOut(mo), responder(*this), monoResponder(*this),
      voices(sst::cpputils::make_array<Voice, VMConfig::maxVoiceCount>(patch, monoValues))
{
    voiceManager = std::make_unique<voiceManager_t>(responder, monoResponder);
    monoValues.mtsClient = MTS_RegisterClient();

    for (int i = 0; i < numMacros; ++i)
    {
        monoValues.macroPtr[i] = &patch.macroNodes[i].level.value;
    }

    std::fill(lState.begin(), lState.end(), nullptr);
    std::fill(rState.begin(), rState.end(), nullptr);

    reapplyControlSettings();
    resetSoloState();
}

Synth::~Synth()
{
    if (monoValues.mtsClient)
    {
        MTS_DeregisterClient(monoValues.mtsClient);
    }

    for (auto ls : lState)
        if (ls)
            src_delete(ls);
    for (auto rs : rState)
        if (rs)
            src_delete(rs);
}

void Synth::setSampleRate(double sampleRate)
{
    auto ohsr = hostSampleRate;
    auto oesr = engineSampleRate;

    hostSampleRate = sampleRate;
    // Look for 44 variants
    bool is441{false};
    auto hsrBy441 = hostSampleRate / (44100 / 2);
    if (std::fabs(hsrBy441 - std::round(hsrBy441)) < 1e-3)
    {
        is441 = true;
    }

    double internalRate{0.f};
    double mul{0.f};
    switch (sampleRateStrategy)
    {
    case SR_110120:
    {
        mul = 2.5;
    }
    break;
    case SR_132144:
    {
        mul = 3.0;
    }
    break;
    case SR_176192:
    {
        mul = 4;
    }
    break;
    case SR_220240:
    {
        mul = 5;
    }
    break;
    }

    if (is441)
        internalRate = 44100 * mul;
    else
        internalRate = 48000 * mul;

    engineSampleRate = internalRate;

    monoValues.sr.setSampleRate(internalRate);

    lagHandler.setRate(60, blockSize, monoValues.sr.sampleRate);
    vuPeak.setSampleRate(monoValues.sr.sampleRate);
    sampleRateRatio = hostSampleRate / engineSampleRate;

    if (usesLanczos())
    {
        for (int i = 0; i < (isMultiOut ? (1 + numOps) : 1); ++i)
            resampler[i] = std::make_unique<resampler_t>((float)monoValues.sr.sampleRate,
                                                         (float)hostSampleRate);
    }
    else
    {
        auto mode = SRC_SINC_FASTEST;
        if (resamplerEngine == SRC_MEDIUM)
        {
            mode = SRC_SINC_MEDIUM_QUALITY;
        }
        else if (resamplerEngine == SRC_BEST)
        {
            mode = SRC_SINC_BEST_QUALITY;
        }

        for (int i = 0; i < (isMultiOut ? (1 + numOps) : 1); ++i)
        {
            if (lState[i])
            {
                src_delete(lState[i]);
            }
            if (rState[i])
            {
                src_delete(rState[i]);
            }
            int ec;

            lState[i] = src_new(mode, 1, &ec);
            rState[i] = src_new(mode, 1, &ec);
            src_set_ratio(lState[i], sampleRateRatio);
            src_set_ratio(rState[i], sampleRateRatio);
        }
    }

    for (auto &[i, p] : patch.paramMap)
    {
        p->lag.setRateInMilliseconds(1000.0 * 64.0 / 48000.0, engineSampleRate, 1.0 / blockSize);
        p->lag.snapTo(p->value);
    }
    paramLagSet.removeAll();

    // midi is a bit less frequent than param automation so a slightly slower smooth
    midiCCLagCollection.setRateInMilliseconds(1000.0 * 128.0 / 48000.0, engineSampleRate,
                                              1.0 / blockSize);
    midiCCLagCollection.snapAllActiveToTarget();

    audioToUi.push(
        {AudioToUIMsg::SEND_SAMPLE_RATE, 0, (float)hostSampleRate, (float)engineSampleRate});
}

template <bool multiOut> void Synth::processInternal(const clap_output_events_t *outq)
{
    if (!SinTable::staticsInitialized)
        SinTable::initializeStatics();

    processUIQueue(outq);

    if (!audioRunning)
    {
        memset(output, 0, sizeof(output));
        return;
    }

    for (auto it = paramLagSet.begin(); it != paramLagSet.end();)
    {
        it->lag.process();
        it->value = it->lag.v;
        if (!it->lag.isActive())
        {
            it = paramLagSet.erase(it);
        }
        else
        {
            ++it;
        }
    }

    midiCCLagCollection.processAll();

    monoValues.attackFloorOnRetrig = patch.output.attackFloorOnRetrig > 0.5;

    int loops{0};

    SRC_DATA d;

    int generated{0};

    if (usesLanczos())
        generated = (resampler[0]->inputsRequiredToGenerateOutputs(blockSize) > 0 ? 0 : blockSize);

    std::array<bool, numOps> mixerActive;
    if constexpr (multiOut)
    {
        std::fill(mixerActive.begin(), mixerActive.end(), false);
    }

    while (generated < blockSize)
    {
        loops++;
        lagHandler.process();

        if (portaContinuation.updateEveryBlock && portaContinuation.active)
        {
            portaContinuation.portaFrac += portaContinuation.dPortaFrac;
            portaContinuation.sourceKey += portaContinuation.dKey;
            if (portaContinuation.portaFrac >= 1.0)
            {
                portaContinuation.active = false;
                portaContinuation.updateEveryBlock = false;
            }
        }

        float lOutput alignas(16)[2 * (1 + (multiOut ? numOps : 0))][blockSize];
        memset(lOutput, 0, sizeof(lOutput));

        auto cvoice = head;
        Voice *removeVoice{nullptr};

        while (cvoice)
        {
            assert(cvoice->used);
            cvoice->renderBlock();

            mech::accumulate_from_to<blockSize>(cvoice->output[0], lOutput[0]);
            mech::accumulate_from_to<blockSize>(cvoice->output[1], lOutput[1]);

            if constexpr (multiOut)
            {
                // TODO if voice active check
                float stp[2][blockSize];
                for (int i = 0; i < numOps; ++i)
                {
                    if (!cvoice->mixerNode[i].active)
                    {
                        continue;
                    }
                    if (!cvoice->mixerNode[i].from.operatorOutputsToOp)
                    {
                        continue;
                    }
                    mixerActive[i] = true;
                    mech::mul_block<blockSize>(cvoice->out.finalEnvLevel,
                                               cvoice->mixerNode[i].output[0], stp[0]);
                    mech::mul_block<blockSize>(cvoice->out.finalEnvLevel,
                                               cvoice->mixerNode[i].output[1], stp[1]);
                    mech::accumulate_from_to<blockSize>(stp[0], lOutput[2 + 2 * i]);
                    mech::accumulate_from_to<blockSize>(stp[1], lOutput[2 + 2 * i + 1]);
                }
            }

            if (cvoice->out.env.stage > OutputNode::env_t::s_release || cvoice->fadeBlocks == 0)
            {
                auto rvoice = cvoice;
                cvoice = removeFromVoiceList(cvoice);
                rvoice->next = removeVoice;
                removeVoice = rvoice;
            }
            else
            {
                cvoice = cvoice->next;
            }
        }

        while (removeVoice)
        {
            responder.doVoiceEndCallback(removeVoice);
            auto v = removeVoice;
            removeVoice = removeVoice->next;
            v->next = nullptr;
            assert(!v->next && !v->prior);
        }

        if (usesLanczos())
        {
            if constexpr (multiOut)
            {
                for (int rsi = 0; rsi < numOps + 1; ++rsi)
                {
                    for (int i = 0; i < blockSize; ++i)
                    {
                        resampler[rsi]->push(lOutput[rsi * 2][i], lOutput[rsi * 2 + 1][i]);
                    }
                }
            }
            else
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    resampler[0]->push(lOutput[0][i], lOutput[1][i]);
                }
            }
            generated =
                (resampler[0]->inputsRequiredToGenerateOutputs(blockSize) > 0 ? 0 : blockSize);
        }
        else
        {
            int gen0{0};
            for (int rsi = 0; rsi < (multiOut ? (numOps + 1) : 1); ++rsi)
            {
                d.data_in = lOutput[2 * rsi];
                d.data_out = output[2 * rsi] + generated;
                d.input_frames = blockSize;
                d.output_frames = blockSize - generated;
                d.end_of_input = 0;
                d.src_ratio = sampleRateRatio;

                src_process(lState[rsi], &d);
                auto lgen = d.output_frames_gen;

                d.data_in = lOutput[2 * rsi + 1];
                d.data_out = output[2 * rsi + 1] + generated;
                d.input_frames = blockSize;
                d.output_frames = blockSize - generated;
                d.end_of_input = 0;
                d.src_ratio = sampleRateRatio;

                src_process(rState[rsi], &d);
                auto rgen = d.output_frames_gen;
                assert(lgen == rgen);
                if (rsi == 0)
                {
                    gen0 = lgen;
                }
            }
            generated += gen0;
        }

        if (isEditorAttached)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                vuPeak.process(lOutput[0][i], lOutput[1][i]);
            }

            if (lastVuUpdate >= updateVuEvery)
            {
                AudioToUIMsg msg{AudioToUIMsg::UPDATE_VU, 0, vuPeak.vu_peak[0], vuPeak.vu_peak[1]};
                audioToUi.push(msg);

                AudioToUIMsg msg2{AudioToUIMsg::UPDATE_VOICE_COUNT, (uint32_t)voiceCount};
                audioToUi.push(msg2);
                lastVuUpdate = 0;
            }
            else
            {
                lastVuUpdate++;
            }
        }
    }

    if (resamplerEngine == LANCZOS)
    {
        if constexpr (multiOut)
        {
            for (int rsi = 0; rsi < numOps + 1; ++rsi)
            {
                resampler[rsi]->populateNextBlockSize(output[rsi * 2], output[rsi * 2 + 1]);
                resampler[rsi]->renormalizePhases();
            }
        }
        else
        {
            resampler[0]->populateNextBlockSize(output[0], output[1]);
            resampler[0]->renormalizePhases();
        }
    }
    if (resamplerEngine == ZOH)
    {
        if constexpr (multiOut)
        {
            for (int rsi = 0; rsi < numOps + 1; ++rsi)
            {
                resampler[rsi]->populateNextBlockSizeZOH(output[rsi * 2], output[rsi * 2 + 1]);
                resampler[rsi]->renormalizePhases();
            }
        }
        else
        {
            resampler[0]->populateNextBlockSizeZOH(output[0], output[1]);
            resampler[0]->renormalizePhases();
        }
    }
    if (resamplerEngine == LINTERP)
    {
        if constexpr (multiOut)
        {
            for (int rsi = 0; rsi < numOps + 1; ++rsi)
            {
                resampler[rsi]->populateNextBlockSizeLin(output[rsi * 2], output[rsi * 2 + 1]);
                resampler[rsi]->renormalizePhases();
            }
        }
        else
        {
            resampler[0]->populateNextBlockSizeLin(output[0], output[1]);
            resampler[0]->renormalizePhases();
        }
    }
}

void Synth::process(const clap_output_events_t *o)
{
    if (isMultiOut)
        processInternal<true>(o);
    else
        processInternal<false>(o);
}

void Synth::addToVoiceList(Voice *v)
{
    v->prior = nullptr;
    v->next = head;
    if (v->next)
    {
        v->next->prior = v;
    }
    head = v;
    voiceCount++;
}

Voice *Synth::removeFromVoiceList(Voice *cvoice)
{
    if (patch.output.portaContinuation.value > 0.5 && voiceCount == 1)
    {
        portaContinuation.sourceKey =
            cvoice->voiceValues.key + cvoice->voiceValues.portaDiff * cvoice->voiceValues.portaSign;
        portaContinuation.portaFrac = cvoice->voiceValues.portaFrac;
        portaContinuation.dPortaFrac = cvoice->voiceValues.dPortaFrac;
        portaContinuation.dKey = -cvoice->voiceValues.dPorta * cvoice->voiceValues.portaSign;

        portaContinuation.active = true;
        portaContinuation.updateEveryBlock =
            (int)std::round(patch.output.portaContinuation.value) == 1;
    }
    else
    {
        portaContinuation.active = false;
    }
    if (cvoice->prior)
    {
        cvoice->prior->next = cvoice->next;
    }
    if (cvoice->next)
    {
        cvoice->next->prior = cvoice->prior;
    }
    if (cvoice == head)
    {
        head = cvoice->next;
    }
    auto nv = cvoice->next;
    cvoice->cleanup();
    cvoice->next = nullptr;
    cvoice->prior = nullptr;
    cvoice->fadeBlocks = -1;
    voiceCount--;

    return nv;
}

void Synth::dumpVoiceList()
{
    SXSNLOG("DUMP VOICE LIST : head=" << std::hex << head << std::dec);
    auto c = head;
    while (c)
    {
        SXSNLOG("   c=" << std::hex << c << std::dec << " key=" << c->voiceValues.key
                        << " u=" << c->used);
        c = c->next;
    }
}

void Synth::processUIQueue(const clap_output_events_t *outq)
{
    bool didRefresh{false};
    if (doFullRefresh)
    {
        pushFullUIRefresh();
        doFullRefresh = false;
        didRefresh = true;
    }
    auto uiM = mainToAudio.pop();
    while (uiM.has_value())
    {
        switch (uiM->action)
        {
        case MainToAudioMsg::REQUEST_REFRESH:
        {
            if (!didRefresh)
            {
                // don't do it twice in one process obvs
                pushFullUIRefresh();
            }
        }
        break;
        case MainToAudioMsg::SET_PARAM_WITHOUT_NOTIFYING:
        case MainToAudioMsg::SET_PARAM:
        {
            bool notify = uiM->action == MainToAudioMsg::SET_PARAM;

            auto dest = patch.paramMap.at(uiM->paramId);
            if (notify)
            {
                if (beginEndParamGestureCount == 0)
                {
                    SXSNLOG("Non-begin/end bound param edit for '" << dest->meta.name << "'");
                }
                if (dest->meta.type == md_t::FLOAT)
                    lagHandler.setNewDestination(&(dest->value), uiM->value);
                else
                    dest->value = uiM->value;

                clap_event_param_value_t p;
                p.header.size = sizeof(clap_event_param_value_t);
                p.header.time = 0;
                p.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                p.header.type = CLAP_EVENT_PARAM_VALUE;
                p.header.flags = 0;
                p.param_id = uiM->paramId;
                p.cookie = dest;

                p.note_id = -1;
                p.port_index = -1;
                p.channel = -1;
                p.key = -1;

                p.value = uiM->value;

                outq->try_push(outq, &p.header);
            }
            else
            {
                dest->value = uiM->value;
            }

            /*
             * Some params have other side effects
             */
            if (dest->meta.id == patch.output.playMode.meta.id ||
                dest->meta.id == patch.output.polyLimit.meta.id ||
                dest->meta.id == patch.output.pianoModeActive.meta.id ||
                dest->meta.id == patch.output.mpeActive.meta.id ||
                dest->meta.id == patch.output.sampleRateStrategy.meta.id ||
                dest->meta.id == patch.output.resampleEngine.meta.id)
            {
                reapplyControlSettings();
            }

            if (dest->adhocFeatures & Param::AdHocFeatureValues::SOLO)
            {
                resetSoloState();
            }

            auto d = patch.dirty;
            if (!d)
            {
                patch.dirty = true;
                audioToUi.push({AudioToUIMsg::SET_PATCH_DIRTY_STATE, patch.dirty});
            }
        }
        break;
        case MainToAudioMsg::BEGIN_EDIT:
        case MainToAudioMsg::END_EDIT:
        {
            if (uiM->action == MainToAudioMsg::BEGIN_EDIT)
            {
                beginEndParamGestureCount++;
            }
            else
            {
                beginEndParamGestureCount--;
            }
            clap_event_param_gesture_t p;
            p.header.size = sizeof(clap_event_param_gesture_t);
            p.header.time = 0;
            p.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            p.header.type = uiM->action == MainToAudioMsg::BEGIN_EDIT
                                ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                : CLAP_EVENT_PARAM_GESTURE_END;
            p.header.flags = 0;
            p.param_id = uiM->paramId;

            outq->try_push(outq, &p.header);
        }
        break;
        case MainToAudioMsg::STOP_AUDIO:
        {
            if (lagHandler.active)
                lagHandler.instantlySnap();
            voiceManager->allSoundsOff();
            audioRunning = false;
        }
        break;
        case MainToAudioMsg::START_AUDIO:
        {
            audioRunning = true;
        }
        break;
        case MainToAudioMsg::SEND_PATCH_NAME:
        {
            memset(patch.name, 0, sizeof(patch.name));
            strncpy(patch.name, uiM->uiManagedPointer, 255);
            audioToUi.push({AudioToUIMsg::SET_PATCH_NAME, 0, 0, 0, patch.name});
        }
        break;
        case MainToAudioMsg::SEND_PATCH_IS_CLEAN:
        {
            patch.dirty = false;
            audioToUi.push({AudioToUIMsg::SET_PATCH_DIRTY_STATE, patch.dirty});
        }
        break;
        case MainToAudioMsg::SEND_POST_LOAD:
        {
            postLoad();
        }
        break;
        case MainToAudioMsg::SEND_PREP_FOR_STREAM:
        {
            prepForStream();
        }
        break;
        case MainToAudioMsg::SEND_REQUEST_RESCAN:
        {
            onMainRescanParams = true;
            audioToUi.push({AudioToUIMsg::DO_PARAM_RESCAN});
            clapHost->request_callback(clapHost);
        }
        break;
        case MainToAudioMsg::EDITOR_ATTACH_DETATCH:
        {
            isEditorAttached = uiM->paramId;
        }
        break;
        case MainToAudioMsg::PANIC_STOP_VOICES:
        {
            voiceManager->allSoundsOff();
        }
        break;
        }
        uiM = mainToAudio.pop();
    }
}

void Synth::reapplyControlSettings()
{
    if (sampleRateStrategy != (SampleRateStrategy)patch.output.sampleRateStrategy.value ||
        resamplerEngine != (ResamplerEngine)patch.output.resampleEngine.value)
    {
        sampleRateStrategy = (SampleRateStrategy)patch.output.sampleRateStrategy.value;
        resamplerEngine = (ResamplerEngine)patch.output.resampleEngine.value;

        if (hostSampleRate > 0)
        {
            voiceManager->allSoundsOff();
            setSampleRate(hostSampleRate);
        }
    }

    auto val = (int)std::round(patch.output.playMode.value);
    if (val != 0)
    {
        voiceManager->setPlaymode(0, voiceManager_t::PlayMode::MONO_NOTES,
                                  (int)voiceManager_t::MonoPlayModeFeatures::NATURAL_LEGATO);
        voiceManager->repeatedKeyMode = voiceManager_t::RepeatedKeyMode::MULTI_VOICE;
    }
    else
    {
        voiceManager->setPlaymode(0, voiceManager_t::PlayMode::POLY_VOICES);
        if (patch.output.pianoModeActive.value > 0.5)
        {
            voiceManager->repeatedKeyMode = voiceManager_t::RepeatedKeyMode::PIANO;
        }
        else
        {
            voiceManager->repeatedKeyMode = voiceManager_t::RepeatedKeyMode::MULTI_VOICE;
        }
    }

    auto lim = (int)std::round(patch.output.polyLimit.value);
    lim = std::clamp(lim, 1, (int)maxVoices);
    voiceManager->setPolyphonyGroupVoiceLimit(0, lim);

    auto mpe = (bool)std::round(patch.output.mpeActive.value);
    if (mpe)
    {
        voiceManager->dialect = voiceManager_t::MIDI1Dialect::MIDI1_MPE;
    }
    else
    {
        voiceManager->dialect = voiceManager_t::MIDI1Dialect::MIDI1;
    }
}

void Synth::handleParamValue(Param *p, uint32_t pid, float value)
{
    if (!p)
    {
        p = patch.paramMap.at(pid);
    }

    // p->value = value;
    p->lag.setTarget(value);
    paramLagSet.addToActive(p);

    AudioToUIMsg au = {AudioToUIMsg::UPDATE_PARAM, pid, value};
    audioToUi.push(au);
}

void Synth::pushFullUIRefresh()
{
    for (const auto *p : patch.params)
    {
        AudioToUIMsg au = {AudioToUIMsg::UPDATE_PARAM, p->meta.id, p->value};
        audioToUi.push(au);
    }
    audioToUi.push({AudioToUIMsg::SET_PATCH_NAME, 0, 0, 0, patch.name});
    audioToUi.push({AudioToUIMsg::SET_PATCH_DIRTY_STATE, patch.dirty});
    audioToUi.push(
        {AudioToUIMsg::SEND_SAMPLE_RATE, 0, (float)hostSampleRate, (float)engineSampleRate});
}

void Synth::resetSoloState()
{
    bool anySolo = false;
    for (auto &mn : patch.mixerNodes)
    {
        anySolo = anySolo || (mn.solo.value > 0.5);
    }

    for (auto &mn : patch.mixerNodes)
    {
        mn.isMutedDueToSoloAway = anySolo && !(mn.solo.value > 0.5);
    }
}

void Synth::onMainThread()
{
    bool ex{true}, re{false};
    if (onMainRescanParams.compare_exchange_strong(ex, re))
    {
        auto pe = static_cast<const clap_host_params_t *>(clapHost->get_extension(clapHost, CLAP_EXT_PARAMS));
        if (pe)
        {
            pe->rescan(clapHost, CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT);
        }
    }
}

} // namespace baconpaul::six_sines