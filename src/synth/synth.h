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

#ifndef BACONPAUL_SIX_SINES_SYNTH_SYNTH_H
#define BACONPAUL_SIX_SINES_SYNTH_SYNTH_H

#include <memory>
#include <array>

#include <clap/clap.h>
#include "sst/basic-blocks/dsp/LanczosResampler.h"
#include "sst/basic-blocks/dsp/Lag.h"
#include "sst/basic-blocks/dsp/VUPeak.h"
#include "sst/basic-blocks/tables/EqualTuningProvider.h"
#include "sst/voicemanager/voicemanager.h"
#include "sst/cpputils/ring_buffer.h"

#include "filesystem/import.h"

#include "configuration.h"

#include "synth/voice.h"
#include "synth/patch.h"

struct MTSClient;

namespace baconpaul::six_sines
{
struct PresetManager;

struct Synth
{
    float output alignas(16)[2][blockSize];

    using resampler_t = sst::basic_blocks::dsp::LanczosResampler<blockSize>;
    std::unique_ptr<resampler_t> resampler;

    Patch patch;
    MTSClient *mtsClient{nullptr};

    struct VMConfig
    {
        static constexpr size_t maxVoiceCount{128};
        using voice_t = Voice;
    };

    std::array<Voice, VMConfig::maxVoiceCount> voices;
    Voice *head{nullptr};
    void addToVoiceList(Voice *);
    Voice *removeFromVoiceList(Voice *); // returns next
    void dumpVoiceList();

    struct VMResponder
    {
        Synth &synth;
        VMResponder(Synth &s) : synth(s) {}

        std::function<void(Voice *)> doVoiceEndCallback = [](auto) {};
        void setVoiceEndCallback(std::function<void(Voice *)> f) { doVoiceEndCallback = f; }
        void retriggerVoiceWithNewNoteID(Voice *, int32_t, float) { assert(false); }
        void moveVoice(Voice *v, uint16_t p, uint16_t c, uint16_t k, float ve)
        {
            v->key = k;
            v->velocity = ve;
        }
        void moveAndRetriggerVoice(Voice *v, uint16_t p, uint16_t c, uint16_t k, float ve)
        {
            v->key = k;
            v->velocity = ve;
            v->gated = true;
            v->retriggerAllEnvelopes();
        }

        int32_t beginVoiceCreationTransaction(
            typename sst::voicemanager::VoiceBeginBufferEntry<VMConfig>::buffer_t &buffer, uint16_t,
            uint16_t, uint16_t, int32_t, float)
        {
            buffer[0].polyphonyGroup = 0;
            return 1;
        };
        void endVoiceCreationTransaction(uint16_t, uint16_t, uint16_t, int32_t, float) {}
        void terminateVoice(Voice *voice)
        {
            voice->used = false;
            voice->gated = false;
            synth.removeFromVoiceList(voice);
        }
        int32_t initializeMultipleVoices(
            int32_t ct,
            const typename sst::voicemanager::VoiceInitInstructionsEntry<VMConfig>::buffer_t &ibuf,
            typename sst::voicemanager::VoiceInitBufferEntry<VMConfig>::buffer_t &obuf, uint16_t pt,
            uint16_t ch, uint16_t key, int32_t nid, float vel, float rt)
        {
            assert(ct == 1);

            if (ibuf[0].instruction !=
                sst::voicemanager::VoiceInitInstructionsEntry<
                    baconpaul::six_sines::Synth::VMConfig>::Instruction::SKIP)
            {
                for (int i = 0; i < VMConfig::maxVoiceCount; ++i)
                {
                    if (synth.voices[i].used == false)
                    {
                        obuf[0].voice = &synth.voices[i];
                        synth.voices[i].used = true;
                        synth.voices[i].gated = true;
                        synth.voices[i].key = key;
                        synth.voices[i].channel = ch;
                        synth.voices[i].mtsClient = synth.mtsClient;
                        synth.voices[i].velocity = vel;
                        synth.voices[i].attack();

                        synth.addToVoiceList(&synth.voices[i]);

                        return 1;
                    }
                }
                return 0;
            }
            return 0;
        }
        void releaseVoice(Voice *v, float) { v->gated = false; }
        void setNoteExpression(Voice *, int32_t, double) {}
        void setVoicePolyphonicParameterModulation(Voice *, uint32_t, double) {}
        void setPolyphonicAftertouch(Voice *, int8_t) {}

        void setVoiceMIDIMPEChannelPitchBend(Voice *, uint16_t) {}
        void setVoiceMIDIMPEChannelPressure(Voice *, int8_t) {}
        void setVoiceMIDIMPETimbre(Voice *, int8_t) {}
    };
    struct VMMonoResponder
    {
        void setMIDIPitchBend(int16_t, int16_t) {}
        void setMIDI1CC(int16_t, int16_t, int8_t) {}
        void setMIDIChannelPressure(int16_t, int16_t) {}
    };
    using voiceManager_t = sst::voicemanager::VoiceManager<VMConfig, VMResponder, VMMonoResponder>;

    VMResponder responder;
    VMMonoResponder monoResponder;
    std::unique_ptr<voiceManager_t> voiceManager;

    Synth();
    ~Synth();

    bool audioRunning{true};

    double realSampleRate{0};
    void setSampleRate(double sampleRate)
    {
        SXSNLOG("Creating resampler at " << sampleRate << " from " << gSampleRate);
        realSampleRate = sampleRate;
        resampler = std::make_unique<resampler_t>(gSampleRate, realSampleRate);
    }

    void process(const clap_output_events_t *);
    void processUIQueue(const clap_output_events_t *);

    void handleParamValue(Param *p, uint32_t pid, float value);

    static_assert(sst::voicemanager::constraints::ConstraintsChecker<VMConfig, VMResponder,
                                                                     VMMonoResponder>::satisfies());

    // UI Communication
    struct AudioToUIMsg
    {
        enum Action : uint32_t
        {
            UPDATE_PARAM,
            UPDATE_VU,
        } action;
        uint32_t paramId{0};
        float value{0}, value2{0};
    };
    struct UIToAudioMsg
    {
        enum Action : uint32_t
        {
            REQUEST_REFRESH,
            SET_PARAM,
            BEGIN_EDIT,
            END_EDIT,
            STOP_AUDIO,
            START_AUDIO
        } action;
        uint32_t paramId{0};
        float value{0};
    };
    using audioToUIQueue_t = sst::cpputils::SimpleRingBuffer<AudioToUIMsg, 1024 * 16>;
    using uiToAudioQueue_T = sst::cpputils::SimpleRingBuffer<UIToAudioMsg, 1024 * 64>;
    audioToUIQueue_t audioToUi;
    uiToAudioQueue_T uiToAudio;
    std::atomic<bool> doFullRefresh{false};
    sst::basic_blocks::dsp::UIComponentLagHandler lagHandler;
    sst::basic_blocks::tables::EqualTuningProvider tuningProvider;

    void pushFullUIRefresh();
    void postLoad()
    {
        doFullRefresh = true;
        resetPlaymode();
    }

    void resetPlaymode();

    sst::basic_blocks::dsp::VUPeak vuPeak;
    int32_t updateVuEvery{(int32_t)(gSampleRate / 60 / blockSize)};
    int32_t lastVuUpdate{updateVuEvery};
};
} // namespace baconpaul::six_sines
#endif // SYNTH_H
