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
#include <cassert>
#include <string>

#include "sst/basic-blocks/dsp/LanczosResampler.h"
#include "sst/filters/ButterworthLPHP.h"
#include "samplerate.h"

class TiXmlElement;

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
    bool isTableInitialized{MatrixIndex::initialize()}; // this forces this init before other ctors

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

    // Audio input upsampling: host rate -> engine rate
    using audioInResampler_t = sst::basic_blocks::dsp::LanczosResampler<blockSize>;
    std::unique_ptr<audioInResampler_t> audioInResampler;
    void pushAudioIn(float L, float R)
    {
        if (audioInResampler)
            audioInResampler->push(L, R);
    }

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

        void discardHostVoice(int32_t vid) {}
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
            assert(ct <= 5);
            const bool hasCenter = (ct > 1 && (ct % 2 == 1));

            auto upr = synth.patch.output.uniPhaseRand.value > 0.5;
            auto prt = synth.patch.output.rephaseOnRetrigger > 0.5;
            for (int vc = 0; vc < ct; ++vc)
            {
                // Bipolar position −1..1 across the unison field; 0 when ct==1.
                // uniRatioMul and uniPanShift are derived from this in Voice::renderBlock
                // each block, so they track host smoothing on unisonSpread / unisonPan.
                const float uniScale = (ct > 1) ? (2.f * (vc - 0.5f * (ct - 1)) / (ct - 1)) : 0.f;

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
                            synth.voices[i].voiceValues.hasCenterVoice = hasCenter;
                            synth.voices[i].voiceValues.isCenterVoice =
                                hasCenter && (std::fabs(uniScale) < 1e-4f);
                            synth.voices[i].voiceValues.uniRatioMul = 1.f;
                            synth.voices[i].voiceValues.uniPanShift = 0.f;
                            synth.voices[i].voiceValues.uniPMScale = uniScale;
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
        void setVoiceMonophonicParameterModulation(Voice *, uint32_t, double) {}
        void setPolyphonicAftertouch(Voice *v, int8_t a) { v->voiceValues.polyAt = a / 127.0; }

        void setVoiceMIDIMPEChannelPitchBend(Voice *v, uint16_t b)
        {
            auto stb = (b - 8192) * 1.0 / 8192;
            v->voiceValues.mpeBendNormalized = stb;
            v->voiceValues.mpeBendInSemis = stb * synth.monoValues.mpeBendRange;
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

    // End-of-chain processing on the engine-rate stereo bus, in place.
    // Runs the saturator / lowpass / decimator / bitcrush / highpass stages.
    void processEndOfBlock(float *L, float *R);

    // End-of-chain stage state. Coefficients are recomputed in
    // reapplyControlSettings; the active flags gate the per-block work.
    sst::filters::ButterworthLP<6> lpFilter;
    sst::filters::ButterworthHP<6> hpFilter;
    bool lpActive{false}, hpActive{false};

    // ZOH-style bit-rate decimator. Runs at the engine (oversample) rate
    // but only samples the input every engine_rate / target_rate ticks,
    // emitting a stair-step (v1 v1 v1 v1 v5 v5 v5 v5 ...) which the SRC
    // then resamples back up.
    struct ZOHRateDownsampler
    {
        float phase{1.f};
        float rate{0.f};
        float lastL{0.f}, lastR{0.f};

        void setRate(float targetRate, float engineRate)
        {
            rate = engineRate > 0 ? targetRate / engineRate : 0.f;
            // step() consumes one phase>=1 per call; rate>1 would lose updates.
            assert(rate <= 1.f);
        }
        void reset()
        {
            phase = 1.f;
            lastL = 0.f;
            lastR = 0.f;
        }
        inline void step(float &L, float &R)
        {
            if (phase >= 1.f)
            {
                phase -= 1.f;
                lastL = L;
                lastR = R;
            }
            else
            {
                L = lastL;
                R = lastR;
            }
            phase += rate;
        }
    };
    ZOHRateDownsampler bitRateZOH;
    bool bitRateActive{false};

    // Saturator scalar shapers. Cheap, stateless, per-session — kept
    // out of sst-waveshapers since this isn't per-voice.
    static inline float softSaturator(float x)
    {
        x = std::clamp(x, -4.f, 4.f);
        return x * (27.f + x * x) / (27.f + 9.f * x * x);
    }

    static inline float ojdSaturator(float x)
    {
        constexpr float pm17 = -1.7f, p11 = 1.1f;
        constexpr float pm03 = -0.3f, p09 = 0.9f;
        constexpr float denLow = 1.f / (4.f * (1.f - 0.3f));
        constexpr float denHigh = 1.f / (4.f * (1.f - 0.9f));

        if (x <= pm17)
            return -1.f;
        if (x >= p11)
            return 1.f;
        if (x >= pm03 && x <= p09)
            return x;
        if (x < pm03)
        {
            auto xl = x - pm03;
            return (xl + denLow * xl * xl) + pm03;
        }
        auto xh = x - p09;
        return (xh - denHigh * xh * xh) + p09;
    }

    void handleParamValue(Param *p, uint32_t pid, float value);

    static_assert(sst::voicemanager::constraints::ConstraintsChecker<VMConfig, VMResponder,
                                                                     VMMonoResponder>::satisfies());

    // Daw-session-only state. Streamed into host state (clap state) but NOT into patches.
    // Holds non-parameter state that is specific to a session and editable only via the editor.
    //
    // mpeFromExtraState is a transient marker (not serialized): set true by fromDawExtraState
    // when the incoming XML actually contained an <mpe> element. The SET_DAW_EXTRA_STATE
    // handler uses it to decide whether to fall back to the legacy patch slots — only 1.1-era
    // sessions that stored MPE inside the patch will have it false.
    struct DawExtraState
    {
        std::string colorMapXml;
        bool mpeActive{false};
        int mpeBendRange{24};
        bool mpeFromExtraState{false};

        // Engine-wide smoothing times, in milliseconds.
        // midiCCSmoothingTimeMs: MIDI CC + MPE + note-expression lags.
        // paramAutomationSmoothingTimeMs: per-param host-automation lag.
        float midiCCSmoothingTimeMs{25.f};
        float paramAutomationSmoothingTimeMs{2.f};
    };
    DawExtraState dawExtraState;

    void toDawExtraState(TiXmlElement &e) const;
    static void fromDawExtraState(TiXmlElement &e, DawExtraState &out);
    void fromDawExtraState(TiXmlElement &e) { fromDawExtraState(e, dawExtraState); }

    // UI Communication
    struct AudioToUIMsg
    {
        enum Action : uint32_t
        {
            UPDATE_PARAM,
            UPDATE_VU,
            UPDATE_VOICE_COUNT,
            UPDATE_CPU_USAGE,
            SET_PATCH_NAME,
            SET_PATCH_DIRTY_STATE,

            SEND_SAMPLE_RATE,
            SET_DAW_EXTRA_STATE,
            SET_MACRO_NAME, // paramId = macro index, patchNamePointer = name buffer
            MTS_POINTER     // dawExtraStatePointer = MTSClient* (or nullptr)
        } action;
        uint32_t paramId{0};
        float value{0}, value2{0};
        const char *patchNamePointer{0};
        const void *dawExtraStatePointer{nullptr};
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
            SEND_PATCH_AUTHOR,
            SEND_PATCH_IS_CLEAN,
            SEND_POST_LOAD,
            EDITOR_ATTACH_DETATCH, // paramid is true for attach and false for detach
            SEND_PREP_FOR_STREAM,
            PANIC_STOP_VOICES,
            SET_DESIGN_MODE_RUN_ALL,
            SET_DAW_EXTRA_STATE,
            SET_MPE_ACTIVE,                // value = 0/1; engine-instance, not a patch param
            SET_MPE_BEND_RANGE,            // value = 1..96; engine-instance, not a patch param
            SET_MIDI_CC_SMOOTHING_TIME_MS, // value = ms; engine-instance
            SET_PARAM_AUTOMATION_SMOOTHING_TIME_MS, // value = ms; engine-instance
            SEND_MACRO_NAME // paramId = macro index, uiManagedPointer = name buffer
        } action;
        uint32_t paramId{0};
        float value{0};
        const char *uiManagedPointer{nullptr};
        const void *dawExtraStatePointer{nullptr};
    };

    // Rescan request flags accumulated in onMainRescanFlags. Bit positions match
    // CLAP's CLAP_PARAM_RESCAN_* so onMainThread can map them with no translation.
    // The indirection lets non-CLAP layers stay CLAP-header-clean.
    enum RescanRequest : uint32_t
    {
        VALUES = 1 << 0,
        INFO = 1 << 2,
        ALL = VALUES | INFO
    };

    // Thread-safe; accumulates flags and asks the host to call us back on the main
    // thread, where onMainThread() will issue the actual clap rescans.
    void requestParamRescan(uint32_t flags);
    using audioToUIQueue_t = sst::cpputils::SimpleRingBuffer<AudioToUIMsg, 1024 * 16>;
    using mainToAudioQueue_T = sst::cpputils::SimpleRingBuffer<MainToAudioMsg, 1024 * 64>;
    audioToUIQueue_t audioToUi;
    mainToAudioQueue_T mainToAudio;

    // Stereo audio tap for visualizers; ~1.4s @ 96kHz / 2.7s @ 48kHz.
    using audioOutputQueue_t = sst::cpputils::StereoRingBuffer<float, 1024 * 128>;
    audioOutputQueue_t audioOutputRing;
    std::atomic<bool> doFullRefresh{false};
    bool isEditorAttached{false};
    sst::basic_blocks::dsp::UIComponentLagHandler lagHandler;

    std::atomic<bool> readyForStream{false};
    void prepForStream()
    {
        if (lagHandler.active)
            lagHandler.instantlySnap();

        for (auto &p : paramLagSet)
        {
            p.lag.snapToTarget();
            p.value = p.lag.v;
        }
        paramLagSet.removeAll();

        patch.dirty = false;
        doFullRefresh = true;
        readyForStream = true;
    }

    void pushFullUIRefresh();
    void postLoad()
    {
        doFullRefresh = true;
        reapplyControlSettings();
        resetSoloState();

        for (auto &[i, p] : patch.paramMap)
        {
            p->lag.snapTo(p->value);
        }
    }

    std::atomic<uint32_t> onMainRescanFlags{0};
    void onMainThread();

    void reapplyControlSettings();
    void resetSoloState();
    void handleAudioThreadParamSideEffects(Param *dest);

    // Push the current monoValues mpe state into the voice manager (dialect setup).
    // Called from message handlers when mpeActive / mpeBendRange change, and from the
    // SET_DAW_EXTRA_STATE handler after dawExtraState is applied.
    void applyMpeState();

    // Re-rates the engine-wide MIDI CC lag and per-param-map lags from monoValues smoothing
    // times. Active voices keep their attack-time MPE/NE rates — those refresh on next attack.
    void applySmoothingTimes();

    sst::cpputils::active_set_overlay<Param> paramLagSet;

    sst::basic_blocks::dsp::VUPeak vuPeak;
    std::array<sst::basic_blocks::dsp::VUPeak, numOps> opVuPeak;
    double cpuUsage{0};
    int32_t updateVuEvery{(int32_t)(48000 * 2.5 / 60 / blockSize)}; // approx
    int32_t lastVuUpdate{updateVuEvery};

    const clap_host_t *clapHost{nullptr};
};
} // namespace baconpaul::six_sines
#endif // SYNTH_H
