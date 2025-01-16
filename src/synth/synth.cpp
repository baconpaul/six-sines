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

namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;

Synth::Synth()
    : responder(*this), monoResponder(*this),
      voices(sst::cpputils::make_array<Voice, VMConfig::maxVoiceCount>(patch, monoValues))
{
    voiceManager = std::make_unique<voiceManager_t>(responder, monoResponder);
    monoValues.mtsClient = MTS_RegisterClient();

    for (int i = 0; i < numMacros; ++i)
    {
        monoValues.macroPtr[i] = &patch.macroNodes[i].level.value;
    }

    reapplyControlSettings();
}

Synth::~Synth()
{
    if (monoValues.mtsClient)
    {
        MTS_DeregisterClient(monoValues.mtsClient);
    }
}

void Synth::setSampleRate(double sampleRate)
{
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
    SXSNLOG("Setting SampleRates: " << SXSNV(hostSampleRate) << SXSNV(engineSampleRate));
    monoValues.sr.setSampleRate(internalRate);

    lagHandler.setRate(60, blockSize, monoValues.sr.sampleRate);
    vuPeak.setSampleRate(monoValues.sr.sampleRate);
    sampleRateRatio = hostSampleRate / engineSampleRate;

#if RESAMPLER_IS_LANCZOS
    resampler =
        std::make_unique<resampler_t>((float)monoValues.sr.sampleRate, (float)hostSampleRate);
#endif

#if RESAMPLER_IS_SRC
    if (lState)
    {
        src_delete(lState);
    }
    if (rState)
    {
        src_delete(rState);
    }
    int ec;
    lState = src_new(SRC_SINC_FASTEST, 1, &ec);
    rState = src_new(SRC_SINC_FASTEST, 1, &ec);
    src_set_ratio(lState, sampleRateRatio);
    src_set_ratio(rState, sampleRateRatio);
#endif
}

void Synth::process(const clap_output_events_t *outq)
{
    if (!SinTable::staticsInitialized)
        SinTable::initializeStatics();

    assert(resampler);

    processUIQueue(outq);

    if (!audioRunning)
    {
        memset(output, 0, sizeof(output));
        return;
    }

    monoValues.attackFloorOnRetrig = patch.output.attackFloorOnRetrig > 0.5;


    int loops{0};

#if RESAMPLER_IS_LANCZOS
    while (resampler->inputsRequiredToGenerateOutputs(blockSize) > 0)
#endif

#if RESAMPLER_IS_SRC
    SRC_DATA d;

    int generated{0};
    while (generated < blockSize)
#endif
    {
        loops ++;
        lagHandler.process();

        float lOutput alignas(16)[2][blockSize];
        memset(lOutput, 0, sizeof(output));
        // this can be way more efficient
        auto cvoice = head;
        Voice *removeVoice{nullptr};

        while (cvoice)
        {
            assert(cvoice->used);
            cvoice->renderBlock();

            mech::accumulate_from_to<blockSize>(cvoice->output[0], lOutput[0]);
            mech::accumulate_from_to<blockSize>(cvoice->output[1], lOutput[1]);

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

#if RESAMPLER_IS_LANCZOS
        for (int i = 0; i < blockSize; ++i)
        {
            resampler->push(lOutput[0][i], lOutput[1][i]);
        }
#endif

#if RESAMPLER_IS_SRC
        d.data_in = lOutput[0];
        d.data_out = output[0] + generated;
        d.input_frames = blockSize;
        d.output_frames = blockSize - generated;
        d.end_of_input = 0;
        d.src_ratio = sampleRateRatio;

        assert(d.input_frames_used == blockSize);
        src_process(lState, &d);
        auto lgen = d.output_frames_gen;

        d.data_in = lOutput[1];
        d.data_out = output[1] + generated;
        d.input_frames = blockSize;
        d.output_frames = blockSize - generated;
        d.end_of_input = 0;
        d.src_ratio = sampleRateRatio;

        assert(d.input_frames_used == blockSize);
        src_process(rState, &d);
        auto rgen = d.output_frames_gen;

        assert(lgen == rgen);
        generated += lgen;

#endif

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
#if RESAMPLER_IS_LANCZOS
    resampler->populateNextBlockSize(output[0], output[1]);
    resampler->renormalizePhases();
#endif
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
    auto uiM = uiToAudio.pop();
    while (uiM.has_value())
    {
        switch (uiM->action)
        {
        case UIToAudioMsg::REQUEST_REFRESH:
        {
            if (!didRefresh)
            {
                // don't do it twice in one process obvs
                pushFullUIRefresh();
            }
        }
        break;
        case UIToAudioMsg::SET_PARAM:
        {
            auto dest = patch.paramMap.at(uiM->paramId);
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

            /*
             * Some params have other side effects
             */
            if (dest->meta.id == patch.output.playMode.meta.id ||
                dest->meta.id == patch.output.polyLimit.meta.id ||
                dest->meta.id == patch.output.pianoModeActive.meta.id ||
                dest->meta.id == patch.output.mpeActive.meta.id ||
                dest->meta.id == patch.output.sampleRateStrategy.meta.id)
            {
                reapplyControlSettings();
            }

            auto d = patch.dirty;
            if (!d)
            {
                patch.dirty = true;
                audioToUi.push({AudioToUIMsg::SET_PATCH_DIRTY_STATE, patch.dirty});
            }
        }
        break;
        case UIToAudioMsg::BEGIN_EDIT:
        case UIToAudioMsg::END_EDIT:
        {
            clap_event_param_gesture_t p;
            p.header.size = sizeof(clap_event_param_gesture_t);
            p.header.time = 0;
            p.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            p.header.type = uiM->action == UIToAudioMsg::BEGIN_EDIT ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                                                    : CLAP_EVENT_PARAM_GESTURE_END;
            p.header.flags = 0;
            p.param_id = uiM->paramId;

            outq->try_push(outq, &p.header);
        }
        break;
        case UIToAudioMsg::STOP_AUDIO:
        {
            voiceManager->allSoundsOff();
            audioRunning = false;
        }
        break;
        case UIToAudioMsg::START_AUDIO:
        {
            audioRunning = true;
        }
        break;
        case UIToAudioMsg::SEND_PATCH_NAME:
        {
            memset(patch.name, 0, sizeof(patch.name));
            strncpy(patch.name, uiM->hackPointer, 255);
            audioToUi.push({AudioToUIMsg::SET_PATCH_NAME, 0, 0, 0, patch.name});
        }
        break;
        case UIToAudioMsg::SEND_PATCH_IS_CLEAN:
        {
            patch.dirty = false;
            audioToUi.push({AudioToUIMsg::SET_PATCH_DIRTY_STATE, patch.dirty});
        }
        break;
        case UIToAudioMsg::EDITOR_ATTACH_DETATCH:
        {
            isEditorAttached = uiM->paramId;
        }
        break;
        case UIToAudioMsg::PANIC_STOP_VOICES:
        {
            voiceManager->allSoundsOff();
        }
        break;
        }
        uiM = uiToAudio.pop();
    }
}

void Synth::reapplyControlSettings()
{
    if (sampleRateStrategy != (SampleRateStrategy)patch.output.sampleRateStrategy.value)
    {
        sampleRateStrategy = (SampleRateStrategy)patch.output.sampleRateStrategy.value;

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

    p->value = value;
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
}

} // namespace baconpaul::six_sines