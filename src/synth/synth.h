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

#ifndef BACONPAUL_SIX_SINES_SYNTH_SYNTH_H
#define BACONPAUL_SIX_SINES_SYNTH_SYNTH_H

#include <memory>
#include <array>

#include "sst/basic-blocks/dsp/LanczosResampler.h"
#include "samplerate.h"

#include <clap/clap.h>
#include "sst/basic-blocks/dsp/Lag.h"
#include "sst/basic-blocks/dsp/VUPeak.h"
#include "sst/basic-blocks/tables/EqualTuningProvider.h"
#include "sst/voicemanager/voicemanager.h"
#include "sst/cpputils/ring_buffer.h"

#include "filesystem/import.h"

#include "configuration.h"

#include "synth/voice.h"
#include "synth/patch.h"
#include "mono_values.h"
#include "mod_matrix.h"
#include "sst/basic-blocks/dsp/LagCollection.h"

namespace baconpaul::six_sines
{
struct Synth
{
    float output alignas(16)[2 * (1 + numOps)][blockSize];

    bool isMultiOut{false};

    SampleRateStrategy sampleRateStrategy{SampleRateStrategy::SR_110120};
    ResamplerEngine resamplerEngine{ResamplerEngine::SRC_FAST};
    inline bool usesLanczos() const
    {
        return resamplerEngine == ResamplerEngine::LANCZOS || resamplerEngine == ZOH ||
               resamplerEngine == LINTERP;
    }

    using resampler_t = sst::basic_blocks::dsp::LanczosResampler<blockSize>;
    std::array<std::unique_ptr<resampler_t>, 1 + numOps> resampler;
    std::array<SRC_STATE *, 1 + numOps> lState{}, rState{};

    Patch patch;
    MonoValues monoValues;
    sst::basic_blocks::dsp::LagCollection<130> midiCCLagCollection; // 130 for 128 + pitch + chanat

    struct VMConfig
    {
        static constexpr size_t maxVoiceCount{maxVoices};
        using voice_t = Voice;
    };

    std::array<Voice, VMConfig::maxVoiceCount> voices;
    Voice *head{nullptr};
    void addToVoiceList(Voice *);
    Voice *removeFromVoiceList(Voice *); // returns next
    void dumpVoiceList();
    int voiceCount{0};

    struct PortaContinuation
    {
        bool active{false};
        bool updateEveryBlock{false};
        float sourceKey{0.f};
        float dKey{0.f};
        float portaFrac{0.f};
        float dPortaFrac{0.f};
    } portaContinuation;

    struct VMResponder
    {
        Synth &synth;
        VMResponder(Synth &s) : synth(s) {}

        std::function<void(Voice *)> doVoiceEndCallback = [](auto) {};
        void setVoiceEndCallback(std::function<void(Voice *)> f) { doVoiceEndCallback = f; }
        void retriggerVoiceWithNewNoteID(Voice *v, int32_t nid, float vel)
        {
            v->voiceValues.setGated(true);
            v->voiceValues.velocity = vel;
            v->retriggerAllEnvelopesForReGate();
        }
        void moveVoice(Voice *v, uint16_t p, uint16_t c, uint16_t k, float ve)
        {
            v->setupPortaTo(k, synth.patch.output.portaTime.value);
            v->voiceValues.setKey(k);
            v->voiceValues.velocity = ve;
            v->retriggerAllEnvelopesForKeyPress();
        }

        void moveAndRetriggerVoice(Voice *v, uint16_t p, uint16_t c, uint16_t k, float ve)
        {
            v->setupPortaTo(k, synth.patch.output.portaTime.value);
            v->voiceValues.setKey(k);
            v->voiceValues.velocity = ve;
            v->voiceValues.setGated(true);
            v->retriggerAllEnvelopesForReGate();
        }

        int32_t beginVoiceCreationTransaction(
            typename sst::voicemanager::VoiceBeginBufferEntry<VMConfig>::buffer_t &buffer, uint16_t,
            uint16_t, uint16_t, int32_t, float)
        {
            auto vc = (int)std::round(synth.patch.output.unisonCount.value);
            for (int i = 0; i < vc; ++i)
                buffer[i].polyphonyGroup = 0;
            return vc;
        };

        void endVoiceCreationTransaction(uint16_t, uint16_t, uint16_t, int32_t, float) {}

        void terminateVoice(Voice *voice)
        {
            voice->voiceValues.setGated(false);
            voice->fadeBlocks = Voice::fadeOverBlocks;
        }
        int32_t initializeMultipleVoices(
            int32_t ct,
            const typename sst::voicemanager::VoiceInitInstructionsEntry<VMConfig>::buffer_t &ibuf,
            typename sst::voicemanager::VoiceInitBufferEntry<VMConfig>::buffer_t &obuf, uint16_t pt,
            uint16_t ch, uint16_t key, int32_t nid, float vel, float rt)
        {
            int made{0};

            int lastStart{0};
            auto usp = synth.patch.output.unisonSpread.value;
            usp = synth.monoValues.twoToTheX.twoToThe(usp);
            float uniVal[5]{1.0};
            float uniPan[5]{0.0};
            float uniScale[5]{0.0};
            assert(ct <= 5);
            if (ct > 1)
            {
                auto dUni = 2 * (usp - 1) / (ct - 1);
                for (int i = 0; i < ct; ++i)
                {
                    auto val = (1.0 - (usp - 1)) + dUni * i;
                    if (val < 1)
                    {
                        val = 1.0 / ((1.0 + (usp - 1)) - dUni * i);
                    }
                    uniVal[i] = val;
                    uniScale[i] = 2 * (i - 1.0 * (ct - 1) / 2) / (ct - 1);
                    uniPan[i] =
                        std::clamp(synth.patch.output.unisonPan.value * uniScale[i], -1.f, 1.f);
                }
            }

            auto upr = synth.patch.output.uniPhaseRand.value > 0.5;
            auto prt = synth.patch.output.rephaseOnRetrigger > 0.5;
            for (int vc = 0; vc < ct; ++vc)
            {
                if (ibuf[vc].instruction !=
                    sst::voicemanager::VoiceInitInstructionsEntry<
                        baconpaul::six_sines::Synth::VMConfig>::Instruction::SKIP)
                {
                    for (int i = lastStart; i < VMConfig::maxVoiceCount; ++i)
                    {
                        if (synth.voices[i].used == false)
                        {
                            obuf[vc].voice = &synth.voices[i];
                            synth.voices[i].used = true;
                            synth.voices[i].voiceValues.setGated(true);
                            synth.voices[i].voiceValues.setKey(key);
                            synth.voices[i].voiceValues.channel = ch;
                            synth.voices[i].voiceValues.velocity = vel;
                            synth.voices[i].voiceValues.releaseVelocity = 0;
                            synth.voices[i].voiceValues.uniCount = ct;
                            synth.voices[i].voiceValues.uniIndex = vc;
                            synth.voices[i].voiceValues.hasCenterVoice = (ct > 1 && (ct % 2 == 1));
                            synth.voices[i].voiceValues.isCenterVoice =
                                (ct > 1 && (ct % 2 == 1)) && (std::fabs(uniScale[vc]) < 1e-4);
                            synth.voices[i].voiceValues.uniRatioMul = uniVal[vc];
                            synth.voices[i].voiceValues.uniPanShift = uniPan[vc];
                            synth.voices[i].voiceValues.uniPMScale = uniScale[vc];
                            synth.voices[i].voiceValues.phaseRandom = (vc > 0 && upr);
                            synth.voices[i].voiceValues.rephaseOnRetrigger = (!upr && prt);
                            synth.voices[i].voiceValues.noteExpressionTuningInSemis = 0;
                            synth.voices[i].voiceValues.noteExpressionPanBipolar = 0;

                            if (synth.portaContinuation.active)
                            {
                                synth.voices[i].restartPortaTo(synth.portaContinuation.sourceKey,
                                                               key, synth.patch.output.portaTime,
                                                               synth.portaContinuation.portaFrac);
                            }
                            synth.voices[i].attack();

                            synth.addToVoiceList(&synth.voices[i]);

                            made++;
                            lastStart = i + 1;
                            break;
                        }
                    }
                }
            }
            // If there is a porta continuation we dealt with it
            if (ct > 0)
                synth.portaContinuation.active = false;

            return made;
        }
        void releaseVoice(Voice *v, float rv)
        {
            v->voiceValues.setGated(false);
            v->voiceValues.releaseVelocity = rv;
        }
        void setNoteExpression(Voice *v, int32_t e, double val)
        {
            switch (e)
            {
            case CLAP_NOTE_EXPRESSION_TUNING:
                v->voiceValues.noteExpressionTuningInSemis = val;
                break;
            case CLAP_NOTE_EXPRESSION_PAN:
                v->voiceValues.noteExpressionPanBipolar = 2 * val - 1;
                break;
            default:
                break;
            }
        }
        void setVoicePolyphonicParameterModulation(Voice *, uint32_t, double) {}
        void setPolyphonicAftertouch(Voice *v, int8_t a) { v->voiceValues.polyAt = a / 127.0; }

        void setVoiceMIDIMPEChannelPitchBend(Voice *v, uint16_t b)
        {
            auto stb = (b - 8192) * 1.0 / 8192;
            v->voiceValues.mpeBendNormalized = stb;
            v->voiceValues.mpeBendInSemis = stb * synth.patch.output.mpeBendRange.value;
        }
        void setVoiceMIDIMPEChannelPressure(Voice *v, int8_t p)
        {
            v->voiceValues.mpePressure = p / 127.0;
        }
        void setVoiceMIDIMPETimbre(Voice *v, int8_t t) { v->voiceValues.mpeTimbre = t / 127.0; }
    };
    struct VMMonoResponder
    {
        Synth &synth;
        VMMonoResponder(Synth &s) : synth(s) {}

        void setMIDIPitchBend(int16_t c, int16_t v)
        {
            auto val = (v - 8192) * 1.0 / 8192;
            synth.midiCCLagCollection.setTarget(129, val, &synth.monoValues.pitchBend);
        }
        void setMIDI1CC(int16_t ch, int16_t cc, int8_t v)
        {
            synth.monoValues.midiCC[cc] = v;
            // synth.monoValues.midiCCFloat[cc] = v / 127.0;
            synth.midiCCLagCollection.setTarget(cc, v / 127.0, &synth.monoValues.midiCCFloat[cc]);
        }
        void setMIDIChannelPressure(int16_t ch, int16_t v)
        {
            synth.midiCCLagCollection.setTarget(128, v / 127.0, &synth.monoValues.channelAT);
            synth.monoValues.channelAT = v / 127.0;
        }
    };
    using voiceManager_t = sst::voicemanager::VoiceManager<VMConfig, VMResponder, VMMonoResponder>;

    VMResponder responder;
    VMMonoResponder monoResponder;
    std::unique_ptr<voiceManager_t> voiceManager;

    Synth(bool isMultiOut);
    ~Synth();

    bool audioRunning{true};
    int beginEndParamGestureCount{0};

    double hostSampleRate{0}, engineSampleRate{0}, sampleRateRatio{0};
    void setSampleRate(double sampleRate);

    template <bool multiOut> void processInternal(const clap_output_events_t *);

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
            UPDATE_VOICE_COUNT,
            SET_PATCH_NAME,
            SET_PATCH_DIRTY_STATE,
            DO_PARAM_RESCAN,
            SEND_SAMPLE_RATE
        } action;
        uint32_t paramId{0};
        float value{0}, value2{0};
        const char *patchNamePointer{0};
    };
    struct MainToAudioMsg
    {
        enum Action : uint32_t
        {
            REQUEST_REFRESH,
            SET_PARAM,
            SET_PARAM_WITHOUT_NOTIFYING,
            BEGIN_EDIT,
            END_EDIT,
            STOP_AUDIO,
            START_AUDIO,
            SEND_PATCH_NAME,
            SEND_PATCH_IS_CLEAN,
            SEND_POST_LOAD,
            SEND_REQUEST_RESCAN,
            EDITOR_ATTACH_DETATCH, // paramid is true for attach and false for detach
            SEND_PREP_FOR_STREAM,
            PANIC_STOP_VOICES
        } action;
        uint32_t paramId{0};
        float value{0};
        const char *uiManagedPointer{nullptr};
    };
    using audioToUIQueue_t = sst::cpputils::SimpleRingBuffer<AudioToUIMsg, 1024 * 16>;
    using mainToAudioQueue_T = sst::cpputils::SimpleRingBuffer<MainToAudioMsg, 1024 * 64>;
    audioToUIQueue_t audioToUi;
    mainToAudioQueue_T mainToAudio;
    std::atomic<bool> doFullRefresh{false};
    bool isEditorAttached{false};
    sst::basic_blocks::dsp::UIComponentLagHandler lagHandler;

    std::atomic<bool> readyForStream{false};
    void prepForStream()
    {
        if (lagHandler.active)
            lagHandler.instantlySnap();

        auto curr = lagHead;
        lagHead = nullptr;
        while (curr)
        {
            curr->lag.snapToTarget();
            curr->value = curr->lag.v;
            auto cc = curr;
            curr = curr->nextLag;
            if (curr)
            {
                curr->prevLag = nullptr;
            }
            cc->nextLag = nullptr;
            cc->prevLag = nullptr;
        }
        patch.dirty = false;
        doFullRefresh = true;
        readyForStream = true;
    }

    void pushFullUIRefresh();
    void postLoad()
    {
        doFullRefresh = true;
        reapplyControlSettings();

        for (auto &[i, p] : patch.paramMap)
        {
            p->lag.snapTo(p->value);
        }
    }

    void reapplyControlSettings();

    Param *lagHead{nullptr};

    sst::basic_blocks::dsp::VUPeak vuPeak;
    int32_t updateVuEvery{(int32_t)(48000 * 2.5 / 60 / blockSize)}; // approx
    int32_t lastVuUpdate{updateVuEvery};

    const clap_host_t *clapHost{nullptr};
};
} // namespace baconpaul::six_sines
#endif // SYNTH_H
