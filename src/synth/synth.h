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
#include <atomic>
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
#include "ui/ui-defaults.h"
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

    Patch patch;     // audio-thread working copy
    Patch patchMain; // main-thread source of truth
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
            // one draw for the whole note; unison-locked LFOs hash it per instance
            auto notePhaseSeed = synth.monoValues.rng.unifU32();
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
                            synth.voices[i].voiceValues.notePhaseSeed = notePhaseSeed;
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
        void setVoiceMIDIMPETimbre(Voice *v, int8_t t)
        {
            v->voiceValues.mpeTimbre = t / 127.0;
            // Bipolar: 64 -> 0, 0 -> -1, 127 -> +1. The lower half has one extra
            // tick (0..63 vs 65..127), so the bottom tick clamps to -1.
            v->voiceValues.mpeTimbreBipolar = std::clamp((t - 64) / 63.0, -1.0, 1.0);
        }
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

    // Optional steep anti-alias LP ahead of the ZOH (the "+ Pre-Filter" modes),
    // cutoff at the crusher Nyquist so the ZOH has nothing above it to fold.
    sst::filters::ButterworthLP<8> bitRatePreFilter;
    bool bitRatePreFilterActive{false};

    // Optional steep brickwall at host Nyquist, applied at the very head of the
    // output chain (before saturation) so nothing ultrasonic feeds the stages.
    sst::filters::ButterworthLP<16> ultrasonicFilter;
    bool ultrasonicActive{false};

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

    // Every clap cookie we hand the host points into the audio-thread `patch`: the host hands
    // it back on param events, which we only ever resolve on the audio thread.
    void *clapCookieFor(uint32_t paramId)
    {
        auto it = patch.paramMap.find(paramId);
        return it == patch.paramMap.end() ? nullptr : (void *)it->second;
    }

    static_assert(sst::voicemanager::constraints::ConstraintsChecker<VMConfig, VMResponder,
                                                                     VMMonoResponder>::satisfies());

    // Daw-session state: streamed into host (clap) state but NOT into patches. Split by which
    // thread needs it so the audio/main separation is explicit in the type (see surge-xt2).
    //
    // AudioDawState is the part the audio engine consumes (MPE dialect + engine-wide lag rates).
    // Trivially-copyable POD so it can be transported to the audio thread by value; the main
    // thread owns the authoritative copy and the audio thread only ever reads it (into monoValues)
    // and never writes it back.
    struct AudioDawState
    {
        bool mpeActive{false};
        int mpeBendRange{24};
        // Engine-wide smoothing times, in milliseconds. midiCCSmoothingTimeMs: MIDI CC + MPE +
        // note-expression lags. paramAutomationSmoothingTimeMs: per-param host-automation lag.
        float midiCCSmoothingTimeMs{25.f};
        float paramAutomationSmoothingTimeMs{2.f};
    };

    // MainDawState is the part only the UI needs; it never crosses to the audio thread, so it may
    // hold heap members (the colour-map XML). mpeFromExtraState marks that an incoming save carried
    // an <mpe> element — a false value drives the 1.1-era fallback to the legacy in-patch MPE slots
    // (resolved on the main thread in stateLoad).
    struct MainDawState
    {
        std::string colorMapXml;
        bool mpeFromExtraState{false};

        // Which preset the session is showing. patch.name carries only the bare display name,
        // which is not a unique key - a user preset can share it with a factory one - so the
        // slot itself is recorded here. Empty kind means unknown: fall back to matching by name.
        std::string presetKind{};     // "init" | "factory" | "user"
        std::string presetCategory{}; // factory category
        std::string presetPath{};     // factory: the file name. user: relative to userPatchesPath

        // no arguments clears the record, so a load we cannot place falls back to the name
        void recordPreset(const std::string &kind = "", const std::string &cat = "",
                          const std::string &path = "")
        {
            presetKind = kind;
            presetCategory = cat;
            presetPath = path;
        }
    };

    // The definitive DAW session state is all main-thread. `dawStateMain` is the authoritative
    // copy: the editor binds it by reference and edits it single-threaded, and stateSave streams
    // it. Its audio-relevant part is pushed to the audio thread by value via SET_AUDIO_DAW_STATE;
    // the colour map never crosses. Grouping audio + main here lets the editor bind one reference.
    struct DawStateMain
    {
        AudioDawState audio;
        MainDawState main;
    };
    DawStateMain dawStateMain;

    // The engine's audio-thread copy of the audio-relevant DAW state. QUEUE-INBOUND-ONLY: written
    // solely by the SET_AUDIO_DAW_STATE handler (and seeded once at construction, pre-audio), which
    // then applies it into monoValues. No main thread ever touches it at runtime — the only
    // main/audio channel is the queue.
    AudioDawState audioDawState;

    // User-defaults reader, owned by the engine so non-UI startup can seed the session state
    // from saved preferences. Shared with the editor (by pointer) for read/write.
    std::unique_ptr<ui::defaultsProvder_t> defaultsProvider;

    // Resolve the Six Sines user documents folder. Prefers the legacy ~/Documents/SixSines when
    // it already exists; otherwise uses (and creates) the vendored BaconPaul/SixSines path. The
    // single source of truth for presets, themes, and user defaults. Empty on filesystem error.
    static fs::path userDocumentsPath();

    // Stream / parse the <dawExtraState> block. Static and operate on a caller-held DawStateMain,
    // so they never implicitly touch engine state; the patchMain hooks pass `dawStateMain`. Both
    // are [main-thread] (stateSave / stateLoad).
    static void toDawExtraState(TiXmlElement &e, const DawStateMain &s);
    static void fromDawExtraState(TiXmlElement &e, DawStateMain &s);

    // Audio thread: consume the queue-inbound audioDawState into monoValues (MPE dialect + lag
    // rates), from the SET_AUDIO_DAW_STATE handler (and once at construction, pre-audio).
    void applyAudioDawState(const AudioDawState &s)
    {
        monoValues.mpeActive = s.mpeActive;
        monoValues.mpeBendRange = s.mpeBendRange;
        monoValues.midiCCSmoothingTimeMs = s.midiCCSmoothingTimeMs;
        monoValues.paramAutomationSmoothingTimeMs = s.paramAutomationSmoothingTimeMs;
        applyMpeState();
        applySmoothingTimes();
    }

    // UI Communication
    struct AudioToMainMsg
    {
        enum Action : uint32_t
        {
            UPDATE_PARAM, // host-automation echo; keeps patchMain current + moves the knob
            UPDATE_VU,
            UPDATE_VOICE_COUNT,
            UPDATE_CPU_USAGE,
            SEND_SAMPLE_RATE,
            MTS_POINTER // dawExtraStatePointer = MTSClient* (or nullptr)
        } action;
        uint32_t paramId{0};
        float value{0}, value2{0};
        const void *dawExtraStatePointer{nullptr};
    };
    struct MainToAudioMsg
    {
        enum Action : uint32_t
        {
            // Ask the engine to echo back its non-patch UI state (sample rate + MTS pointer). The
            // editor reads all patch state directly from patchMain.
            REQUEST_NON_PATCH_STATE,
            SET_PARAM,
            SET_PARAM_WITHOUT_NOTIFYING,
            BEGIN_EDIT,
            END_EDIT,
            STOP_AUDIO,
            START_AUDIO,
            SEND_POST_LOAD,
            PANIC_STOP_VOICES,
            SET_DESIGN_MODE_RUN_ALL,
            // Transport the main-owned AudioDawState (MPE + smoothing) to the audio thread, by
            // value in the `audioDawState` field. Engine-instance session state, not a patch param.
            SET_AUDIO_DAW_STATE
        } action;
        uint32_t paramId{0};
        float value{0};

        // SET_AUDIO_DAW_STATE payload: the session state to consume, carried by value (POD).
        AudioDawState audioDawState{};
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
    using audioToMainQueue_t = sst::cpputils::SimpleRingBuffer<AudioToMainMsg, 1024 * 16>;
    using mainToAudioQueue_T = sst::cpputils::SimpleRingBuffer<MainToAudioMsg, 1024 * 64>;
    audioToMainQueue_t audioToMain;
    mainToAudioQueue_T mainToAudio;

    // Stereo audio tap for visualizers; ~1.4s @ 96kHz / 2.7s @ 48kHz.
    using audioOutputQueue_t = sst::cpputils::StereoRingBuffer<float, 1024 * 128>;
    audioOutputQueue_t audioOutputRing;

    // Set true while an editor idles (it owns draining audioToMain). Gates the audio thread's
    // telemetry pushes and its request for onMainThread to drain.
    std::atomic<bool> editorActive{false};
    // Coalesces request_callback: the audio thread flips it so at most one onMainThread drain is
    // pending while no editor is open.
    std::atomic<bool> mainThreadDrainRequested{false};
    // Bumped on an out-of-band write to patchMain (host stateLoad / preset load / an inactive
    // paramsFlush) so an open editor rebuilds every widget from patchMain on its next idle.
    std::atomic<uint32_t> uiForceRebuild{0};
    sst::basic_blocks::dsp::UIComponentLagHandler lagHandler;

    // Snap every lagged param to its settled value and clear the active lag set. Audio-thread-safe
    // (no allocation). Used by tests and before streaming a snapshot.
    void snapAllParams()
    {
        if (lagHandler.active)
            lagHandler.instantlySnap();
        for (auto &p : paramLagSet)
        {
            p.lag.snapToTarget();
            p.value = p.lag.v;
        }
        paramLagSet.removeAll();
    }

    // Applies a patch-model audioToMain message (a host-automation param value) to `dest`. Returns
    // true if handled, false for UI-only telemetry (VU / voice count / CPU / sample rate / MTS)
    // which the editor idle handles itself. Static: it only touches `dest`, so the editor (which
    // has no Synth handle) can call it too, and drainAudioToMainInto shares it.
    static bool handleAudioToMainMessage(Patch &dest, const AudioToMainMsg &m);

    // Main-thread drain of audioToMain into a target patch (patchMain), discarding the UI-only
    // messages. Used when no editor is open (onMainThread, stateSave, tests).
    void drainAudioToMainInto(Patch &dest);

    // Main-thread paramsFlush (plugin INACTIVE): apply incoming host events in place to patchMain
    // and echo queued UI edits to the host. Never touches `patch`.
    void paramsFlushMainThread(const clap_input_events_t *in, const clap_output_events_t *out);

    // Push an entire patch's params into the audio-thread `patch` via the mainToAudio queue, then
    // tell the host to re-read. Main thread only. Patch name / author / dirty / macroNames are
    // main-thread-only state, set on patchMain by the caller — they do not travel here. Static
    // because callers (preset manager, clap adapter) hold the queue + host but not a Synth handle.
    static void sendEntirePatchToAudio(Patch &src, mainToAudioQueue_T &mainToAudio,
                                       const clap_host_t *host,
                                       const clap_host_params_t *hostParams = nullptr);

    void postLoad()
    {
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

    // Push the current monoValues mpe state into the voice manager (dialect setup). Called from
    // applyAudioDawState (the SET_AUDIO_DAW_STATE handler / construction seed) after the
    // AudioDawState has been consumed into monoValues, and from reapplyControlSettings.
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
