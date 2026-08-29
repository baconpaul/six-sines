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

#include "configuration.h"
#include <clap/clap.h>
#include <chrono>

#include <clap/helpers/plugin.hh>
#include "synth/synth.h"
#include "presets/preset-manager.h"

#include <clap/helpers/plugin.hxx>
#include <clap/helpers/host-proxy.hxx>

#include <memory>
#include "sst/plugininfra/patch-support/patch_base_clap_adapter.h"
#include "sst/plugininfra/cpufeatures.h"

#include "sst/voicemanager/midi1_to_voicemanager.h"
#include "sst/clap_juce_shim/clap_juce_shim.h"

#include "ui/six-sines-editor.h"

#include <sst/cpputils/rtsan_support.h>

#include <clapwrapper/vst3.h>
#include <clapwrapper/auv2.h>
#include <numeric>
#include <algorithm>

namespace baconpaul::six_sines
{

extern const clap_plugin_descriptor *getDescriptor();
extern const clap_plugin_descriptor *getMultiOutDescriptor();

namespace clapimpl
{

static constexpr clap::helpers::MisbehaviourHandler misLevel =
    clap::helpers::MisbehaviourHandler::Ignore;
static constexpr clap::helpers::CheckingLevel checkLevel = clap::helpers::CheckingLevel::Maximal;

using plugHelper_t = clap::helpers::Plugin<misLevel, checkLevel>;

template <bool multiOut>
struct SixSinesClap : public plugHelper_t, sst::clap_juce_shim::EditorProvider
{
    SixSinesClap(const clap_host *h)
        : plugHelper_t(multiOut ? getMultiOutDescriptor() : getDescriptor(), h)
    {
        engine = std::make_unique<Synth>(multiOut);

        engine->clapHost = h;

        clapJuceShim = std::make_unique<sst::clap_juce_shim::ClapJuceShim>(this);
        clapJuceShim->setResizable(true);
    }
    virtual ~SixSinesClap() {};

    std::unique_ptr<Synth> engine;
    size_t blockPos{0};

  protected:
    bool activate(double sampleRate, uint32_t minFrameCount,
                  uint32_t maxFrameCount) noexcept override
    {
        // The audio thread is stopped here; seed it from the main-thread source of truth. The DAW
        // session state (MPE / smoothing) is seeded at construction and thereafter reaches the
        // audio thread only through the SET_AUDIO_DAW_STATE queue, so it is not touched here.
        engine->patch.copyValuesFrom(engine->patchMain);
        engine->setSampleRate(sampleRate);
        return true;
    }

    void onMainThread() noexcept override { engine->onMainThread(); }

    bool implementsAudioPorts() const noexcept override { return true; }
    uint32_t audioPortsCount(bool isInput) const noexcept override
    {
        return isInput ? 1 : (multiOut ? 7 : 1);
    }
    bool audioPortsInfo(uint32_t index, bool isInput,
                        clap_audio_port_info *info) const noexcept override
    {
        if (isInput)
        {
            if (index != 0)
                return false;
            info->id = 82649;
            info->in_place_pair = CLAP_INVALID_ID;
            strncpy(info->name, "Audio In", sizeof(info->name));
            info->flags = 0;
            info->channel_count = 2;
            info->port_type = CLAP_PORT_STEREO;
            return true;
        }
        if (index > (multiOut ? 6 : 0))
            return false;
        info->id = 75241 + index;
        info->in_place_pair = CLAP_INVALID_ID;
        if (index == 0)
            strncpy(info->name, "Main Out", sizeof(info->name));
        else
            snprintf(info->name, sizeof(info->name) - 1, "Operator %d", index);
        if (index == 0)
            info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        else
            info->flags = 0;
        info->channel_count = 2;
        info->port_type = CLAP_PORT_STEREO;
        return true;
    }
    bool implementsAudioPortsActivation() const noexcept override { return true; }
    bool audioPortsActivationCanActivateWhileProcessing() const noexcept override { return true; }
    bool audioPortsActivationSetActive(bool is_input, uint32_t port_index, bool is_active,
                                       uint32_t sample_size) noexcept override
    {
        return true;
    }

    bool implementsNotePorts() const noexcept override { return true; }
    uint32_t notePortsCount(bool isInput) const noexcept override { return isInput ? 1 : 0; }
    bool notePortsInfo(uint32_t index, bool isInput,
                       clap_note_port_info *info) const noexcept override
    {
        assert(isInput);
        assert(index == 0);
        if (!isInput || index != 0)
            return false;

        info->id = 17252;
        info->supported_dialects =
            CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI_MPE | CLAP_NOTE_DIALECT_CLAP;
        info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
        strncpy(info->name, "Note Input", CLAP_NAME_SIZE - 1);
        return true;
    }

    clap_process_status process(const clap_process *process) noexcept override
    {
        return process_nonblocking(process);
    }

    clap_process_status process_nonblocking(const clap_process *process) SST_CPPUTILS_NONBLOCKING
    {
        auto fpuguard = sst::plugininfra::cpufeatures::FPUStateGuard();

        auto ev = process->in_events;
        auto outq = process->out_events;
        auto sz = ev->size(ev);

        const clap_event_header_t *nextEvent{nullptr};
        uint32_t nextEventIndex{0};
        if (sz != 0)
        {
            nextEvent = ev->get(ev, nextEventIndex);
        }

        if (process->transport)
        {
            engine->monoValues.tempoSyncRatio = process->transport->tempo / 120.0;
            auto tflags = process->transport->flags;
            // Only trust the song position when the host is both playing and exposes a
            // seconds timeline; otherwise free-run our own clock.
            engine->monoValues.isPlayingAndHasSecondsTimeline =
                (tflags & CLAP_TRANSPORT_IS_PLAYING) &&
                (tflags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE);
            if (engine->monoValues.isPlayingAndHasSecondsTimeline)
            {
                engine->monoValues.hostSongPosSeconds =
                    process->transport->song_pos_seconds / (double)CLAP_SECTIME_FACTOR;
                // Anchor here, before any events are dispatched. A note-on at frame 0 of
                // this buffer attacks (and snapshots its SONGPOS LFO phase) before the first
                // engine->process() runs, so the resync inside processInternal would land too
                // late and the LFO would read the stale free-run value.
                engine->monoValues.songPosSeconds = engine->monoValues.hostSongPosSeconds;
            }
        }
        else
        {
            engine->monoValues.tempoSyncRatio = 1.f;
            engine->monoValues.isPlayingAndHasSecondsTimeline = false;
        }
        // Re-anchor songPosSeconds to the host position on the first engine block of this
        // buffer; subsequent blocks advance from there. (Also done above so frame-0 note-ons
        // see the anchored value; this suppresses the per-block advance on the first block.)
        engine->monoValues.songPosNeedsResync = true;

        static constexpr int outBus{multiOut ? 1 + numOps : 1};
        static constexpr int outChan{multiOut ? (1 + numOps) * 2 : 2};
        float *out[outChan];
        for (auto i = 0; i < outBus; ++i)
        {
            auto lo = process->audio_outputs[i].data32;
            out[2 * i] = lo[0];
            out[2 * i + 1] = lo[1];
        }

        const float *audioInL{nullptr}, *audioInR{nullptr};
        if (process->audio_inputs_count > 0 && process->audio_inputs[0].data32)
        {
            audioInL = process->audio_inputs[0].data32[0];
            audioInR = process->audio_inputs[0].data32[1];
        }

        for (auto s = 0U; s < process->frames_count; ++s)
        {
            engine->pushAudioIn(audioInL ? audioInL[s] : 0.f, audioInR ? audioInR[s] : 0.f);

            if (blockPos == 0)
            {
                // Only realy need to run events when we do the block process
                while (nextEvent && nextEvent->time <= s)
                {
                    handleEvent(nextEvent);
                    nextEventIndex++;
                    if (nextEventIndex < sz)
                        nextEvent = ev->get(ev, nextEventIndex);
                    else
                        nextEvent = nullptr;
                }

                engine->process(outq);
            }

            for (auto i = 0; i < outChan; ++i)
                out[i][s] = engine->output[i][blockPos];

            blockPos++;
            if (blockPos == blockSize)
            {
                blockPos = 0;
            }
        }

        while (nextEvent)
        {
            handleEvent(nextEvent);
            nextEventIndex++;
            if (nextEventIndex < sz)
                nextEvent = ev->get(ev, nextEventIndex);
            else
                nextEvent = nullptr;
        }
        return CLAP_PROCESS_CONTINUE;
    }

    void reset() noexcept override { engine->voiceManager->allSoundsOff(); }

    bool handleEvent(const clap_event_header_t *nextEvent)
    {
        auto &vm = engine->voiceManager;
        if (nextEvent->space_id == CLAP_CORE_EVENT_SPACE_ID)
        {
            switch (nextEvent->type)
            {
            case CLAP_EVENT_MIDI:
            {
                auto mevt = reinterpret_cast<const clap_event_midi *>(nextEvent);
                sst::voicemanager::applyMidi1Message(*vm, mevt->port_index, mevt->data);
            }
            break;

            case CLAP_EVENT_NOTE_ON:
            {
                auto nevt = reinterpret_cast<const clap_event_note *>(nextEvent);
                vm->processNoteOnEvent(nevt->port_index, nevt->channel, nevt->key, nevt->note_id,
                                       nevt->velocity, 0.f);
            }
            break;

            case CLAP_EVENT_NOTE_OFF:
            {
                auto nevt = reinterpret_cast<const clap_event_note *>(nextEvent);
                auto nid = nevt->note_id;
                // nid = -1;
                vm->processNoteOffEvent(nevt->port_index, nevt->channel, nevt->key, nid,
                                        nevt->velocity);
            }
            break;
            case CLAP_EVENT_PARAM_VALUE:
            {
                auto pevt = reinterpret_cast<const clap_event_param_value *>(nextEvent);
                auto par =
                    sst::plugininfra::patch_support::paramFromClapEvent<Param>(pevt, engine->patch);
                if (par)
                {
                    engine->handleParamValue(par, pevt->param_id, pevt->value);
                }
            }
            break;

            case CLAP_EVENT_NOTE_EXPRESSION:
            {
                auto nevt = reinterpret_cast<const clap_event_note_expression *>(nextEvent);
                vm->routeNoteExpression(nevt->port_index, nevt->channel, nevt->key, nevt->note_id,
                                        nevt->expression_id, nevt->value);
            }
            break;
            default:
            {
                SXSNLOG("Unknown inbound event of type " << nextEvent->type);
            }
            break;
            }
        }
        return true;
    }

    bool implementsState() const noexcept override { return true; }
    bool stateSave(const clap_ostream *ostream) noexcept override
    {
        // patchMain is authoritative. If no editor is open to keep it current, drain any pending
        // audio-thread updates into it first (we are the only consumer then). With the editor open,
        // the idle owns the queue, so patchMain can lag the audio thread by up to one idle tick
        // during an automation burst — a window of inconsistency, not a race.
        if (!engine->editorActive.load())
            engine->drainAudioToMainInto(engine->patchMain);

        return sst::plugininfra::patch_support::patchToOutStream(engine->patchMain, ostream, true);
    }
    bool stateLoad(const clap_istream *istream) noexcept override
    {
        // Load into temps so a parse failure never leaves the engine half-written.
        auto tmp = std::make_unique<Patch>();
        Synth::DawStateMain loadedState{};
        tmp->dawExtraStateFrom = [&](TiXmlElement &e) { Synth::fromDawExtraState(e, loadedState); };

        if (!sst::plugininfra::patch_support::inStreamToPatch(istream, *tmp))
            return false;

        engine->patchMain.copyValuesFrom(*tmp);

        // 1.1-era sessions carry no <mpe> element; derive MPE from the legacy in-patch slots (now
        // in patchMain). Resolved on the main thread where patchMain is authoritative.
        if (!loadedState.main.mpeFromExtraState)
        {
            loadedState.audio.mpeActive = engine->patchMain.output.legacyMpeActive.value > 0.5f;
            loadedState.audio.mpeBendRange =
                (int)std::round(engine->patchMain.output.legacyMpeBendRange.value);
        }
        engine->dawStateMain = loadedState;
        engine->uiForceRebuild++; // an open editor rebuilds from patchMain

        if (isActive())
        {
            // Push the loaded patch into the audio-thread `patch`; this also rescans the host.
            Synth::sendEntirePatchToAudio(engine->patchMain, engine->mainToAudio, _host.host());
        }
        else if (_host.canUseParams())
        {
            // Not running: the next activate() copies patchMain into patch. Just tell the host to
            // re-read the loaded values / names (it reads them from patchMain). INFO carries the
            // macro-name changes; issue it separately from VALUES|TEXT.
            _host.paramsRescan(CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT);
            _host.paramsRescan(CLAP_PARAM_RESCAN_INFO);
        }

        // Transport the loaded AudioDawState (mpe / smoothing) to the audio thread by value; the
        // handler stores the engine's queue-inbound copy and applies it into monoValues. Drains on
        // the next process (or the first process after activate, when inactive).
        Synth::MainToAudioMsg des{Synth::MainToAudioMsg::SET_AUDIO_DAW_STATE};
        des.audioDawState = engine->dawStateMain.audio;
        engine->mainToAudio.push(des);
        return true;
    }

    bool implementsParams() const noexcept override { return true; }
    uint32_t paramsCount() const noexcept override { return engine->patchMain.params.size(); }
    bool paramsInfo(uint32_t paramIndex, clap_param_info *info) const noexcept override
    {
        // All param reads come from patchMain (the main-thread source of truth); never
        // engine->patch.
        auto ok =
            sst::plugininfra::patch_support::patchParamsInfo(paramIndex, info, engine->patchMain);
        if (!ok)
            return ok;

        // patchParamsInfo cookies the patch it read, but the host hands the cookie back on
        // param events we resolve on the audio thread, so it must point into `patch`
        info->cookie = engine->clapCookieFor(info->id);

        // The macro amplitude param is tagged with isPrimaryMacroFeature; for that
        // single param swap the host-displayed name to "Foo (Macro N)" when the
        // user has renamed the macro. Every other macro param is left alone.
        auto *param = engine->patchMain.params[paramIndex];
        if (param->meta.hasFeature(isPrimaryMacroFeature))
        {
            int idx = (info->id - Patch::MacroNode::idBase) / Patch::MacroNode::idStride;
            if (idx >= 0 && idx < (int)numMacros)
            {
                const auto &nameBuf = engine->patchMain.macroNames[idx];
                std::string userName(nameBuf.data());
                auto def = Patch::MacroNode::defaultGroupName(idx);
                if (!userName.empty() && userName != def)
                {
                    auto fullName = userName + " (" + def + ")";
                    strncpy(info->name, fullName.c_str(), CLAP_NAME_SIZE - 1);
                    info->name[CLAP_NAME_SIZE - 1] = 0;
                }
            }
        }
        return ok;
    }
    bool paramsValue(clap_id paramId, double *value) noexcept override
    {
        return sst::plugininfra::patch_support::patchParamsValue(paramId, value, engine->patchMain);
    }
    bool paramsValueToText(clap_id paramId, double value, char *display,
                           uint32_t size) noexcept override
    {
        return sst::plugininfra::patch_support::patchParamsValueToText(paramId, value, display,
                                                                       size, engine->patchMain);
    }
    bool paramsTextToValue(clap_id paramId, const char *display, double *value) noexcept override
    {
        return sst::plugininfra::patch_support::patchParamsTextToValue(paramId, display, value,
                                                                       engine->patchMain);
    }
    void paramsFlush(const clap_input_events *in, const clap_output_events *out) noexcept override
    {
        // paramsFlush is audio-thread when the plugin is ACTIVE, main-thread when INACTIVE.
        if (isActive())
        {
            // Audio thread: route param changes through the queue into `patch`.
            auto sz = in->size(in);
            for (uint32_t i = 0; i < sz; ++i)
                handleEvent(in->get(in, i));
            engine->processUIQueue(out);
        }
        else
        {
            // Main thread: patchMain is the truth; update it in place and echo out.
            engine->paramsFlushMainThread(in, out);
        }
    }

    bool implementsPresetLoad() const noexcept override { return true; }
    bool presetLoadFromLocation(uint32_t location_kind, const char *location,
                                const char *load_key) noexcept override
    {
        if (location_kind ==
            clap_preset_discovery_location_kind::CLAP_PRESET_DISCOVERY_LOCATION_FILE)
        {
            try
            {
                SXSNLOG("Loading preset from file '" << location << "'");
                auto p = fs::path(fs::u8path(location));
                if (p.extension() == ".sxsnp")
                {
                    std::ifstream t(p);
                    if (!t.is_open())
                        return false;
                    std::stringstream buffer;
                    buffer << t.rdbuf();

                    auto tmp = std::make_unique<Patch>();
                    tmp->fromState(buffer.str());

                    engine->patchMain.copyValuesFrom(*tmp);
                    // Name is main-thread-only patch state; a preset takes the filename, set on
                    // patchMain directly (after copyValuesFrom, which copied the streamed name).
                    auto dn = p.filename().replace_extension("").u8string();
                    memset(engine->patchMain.name, 0, sizeof(engine->patchMain.name));
                    strncpy(engine->patchMain.name, dn.c_str(), sizeof(engine->patchMain.name) - 1);
                    engine->patchMain.dirty = false;
                    engine->uiForceRebuild++;

                    if (isActive())
                        Synth::sendEntirePatchToAudio(engine->patchMain, engine->mainToAudio,
                                                      _host.host());
                    else if (_host.canUseParams())
                    {
                        _host.paramsRescan(CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT);
                        _host.paramsRescan(CLAP_PARAM_RESCAN_INFO);
                    }
                    return true;
                }
                else
                {
                    SXSNLOG("File extension is not .sxsnp '" << p.extension().u8string() << "'");
                }
            }
            catch (const fs::filesystem_error &e)
            {
                SXSNLOG("File System Error " << e.what() << " with " << location);
            }
        }
        else if (location_kind ==
                 clap_preset_discovery_location_kind::CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN)
        {
            auto idx = std::atoi(load_key);
            // This is a bit of a pattern problem. The preset manager should be on the engine.
            auto pm = presets::PresetManager(_host.host());
            if (idx < 0 || idx >= pm.factoryPatchVector.size())
                return false;

            auto &p = pm.factoryPatchVector[idx];

            // TODO : Assert the keys match here. loadFactoryPreset writes patchMain (name / dirty /
            // macro names via fromState) and funnels params into the audio patch; force an open
            // editor to rebuild from patchMain.
            pm.loadFactoryPreset(engine->patchMain, engine->mainToAudio, p.first, p.second);
            engine->uiForceRebuild++;
            return true;
        }
        return false;
    }

  public:
    bool implementsGui() const noexcept override { return clapJuceShim != nullptr; }
    std::unique_ptr<sst::clap_juce_shim::ClapJuceShim> clapJuceShim;
    ADD_SHIM_IMPLEMENTATION(clapJuceShim)
    ADD_SHIM_LINUX_TIMER(clapJuceShim)
    std::unique_ptr<juce::Component> createEditor() override
    {
        auto res = std::make_unique<baconpaul::six_sines::ui::SixSinesEditor>(
            engine->patchMain, engine->audioToMain, engine->mainToAudio, engine->audioOutputRing,
            engine->editorActive, engine->uiForceRebuild, engine->dawStateMain,
            *engine->defaultsProvider, _host.host());

        res->onZoomChanged = [this](auto f)
        {
            if (_host.canUseGui() && clapJuceShim->isEditorAttached())
            {
                // SXSNLOG("onZoomChanged " << f);
                auto s = f * clapJuceShim->getGuiScale();
                guiSetSize(baconpaul::six_sines::ui::SixSinesEditor::edWidth * s,
                           baconpaul::six_sines::ui::SixSinesEditor::edHeight * s);
                _host.guiRequestResize(baconpaul::six_sines::ui::SixSinesEditor::edWidth * s,
                                       baconpaul::six_sines::ui::SixSinesEditor::edHeight * s);
            }
        };

        onShow = [e = res.get()]()
        {
            // SXSNLOG("onShow with zoom factor " << e->zoomFactor);
            e->setZoomFactor(e->zoomFactor);
            return true;
        };
        res->repaint();

        return res;
    }

    bool registerOrUnregisterTimer(clap_id &id, int ms, bool reg) override
    {
        if (!_host.canUseTimerSupport())
            return false;
        if (reg)
        {
            _host.timerSupportRegister(ms, &id);
        }
        else
        {
            _host.timerSupportUnregister(id);
        }
        return true;
    }

    bool registerOrUnregisterPosixFd(int fd, clap_posix_fd_flags_t flags, bool reg) override
    {
        if (!_host.canUsePosixFdSupport())
            return false;
        if (reg)
        {
            _host.posixFdSupportRegister(fd, flags);
        }
        else
        {
            _host.posixFdSupportUnregister(fd);
        }
        return true;
    }

    static uint32_t vst3_getNumMIDIChannels(const clap_plugin *plugin, uint32_t note_port)
    {
        return 16;
    }
    static uint32_t vst3_supportedNoteExpressions(const clap_plugin *plugin)
    {
        return clap_supported_note_expressions::AS_VST3_NOTE_EXPRESSION_TUNING |
               clap_supported_note_expressions::AS_VST3_NOTE_EXPRESSION_PAN;
    }

    static bool CLAP_ABI auv2_get_param_order(const clap_plugin_t *plugin, size_t *order,
                                              size_t param_count) noexcept
    {
        auto *self = static_cast<SixSinesClap *>(plugin->plugin_data);
        auto &params = self->engine->patchMain.params;
        if (param_count != params.size())
            return false;

        std::iota(order, order + param_count, (size_t)0);
        std::sort(order, order + param_count,
                  [&params](size_t a, size_t b)
                  {
                      auto aBack = params[a]->meta.version;
                      auto bBack = params[b]->meta.version;
                      if (aBack != bBack)
                          return aBack < bBack;
                      return params[a]->meta.id < params[b]->meta.id;
                  });
        return true;
    }

    const void *extension(const char *id) noexcept override
    {
        if (strcmp(id, CLAP_PLUGIN_AS_VST3) == 0)
        {
            static clap_plugin_as_vst3 v3p{vst3_getNumMIDIChannels, vst3_supportedNoteExpressions};
            return &v3p;
        }
        if (strcmp(id, CLAP_PLUGIN_AUV2_PARAM_ORDERING) == 0)
        {
            static clap_plugin_auv2_param_ordering_t auv2po{auv2_get_param_order};
            return &auv2po;
        }

        return nullptr;
    }
};

} // namespace clapimpl

const clap_plugin *makePlugin(const clap_host *h, bool multiOut)
{
    if (multiOut)
    {
        auto res = new baconpaul::six_sines::clapimpl::SixSinesClap<true>(h);
        return res->clapPlugin();
    }
    else
    {
        auto res = new baconpaul::six_sines::clapimpl::SixSinesClap<false>(h);
        return res->clapPlugin();
    }
}

// Used for testing only
const Patch *getPatchFromPlugin(const clap_plugin_t *plugin)
{
    auto *self = static_cast<clapimpl::SixSinesClap<false> *>(plugin->plugin_data);
    return &self->engine->patch;
}
} // namespace baconpaul::six_sines

namespace chlp = clap::helpers;
namespace bpss = baconpaul::six_sines::clapimpl;

template class chlp::Plugin<bpss::misLevel, bpss::checkLevel>;
template class chlp::HostProxy<bpss::misLevel, bpss::checkLevel>;
