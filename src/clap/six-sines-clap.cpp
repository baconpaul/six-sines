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

#include <clapwrapper/vst3.h>

namespace baconpaul::six_sines
{

extern const clap_plugin_descriptor *getDescriptor();

namespace clapimpl
{

static constexpr clap::helpers::MisbehaviourHandler misLevel =
    clap::helpers::MisbehaviourHandler::Ignore;
static constexpr clap::helpers::CheckingLevel checkLevel = clap::helpers::CheckingLevel::Maximal;

using plugHelper_t = clap::helpers::Plugin<misLevel, checkLevel>;

template <bool multiOut>
struct SixSinesClap : public plugHelper_t, sst::clap_juce_shim::EditorProvider
{
    SixSinesClap(const clap_host *h) : plugHelper_t(getDescriptor(), h)
    {
        engine = std::make_unique<Synth>(multiOut);

        engine->clapHost = h;

        clapJuceShim = std::make_unique<sst::clap_juce_shim::ClapJuceShim>(this);
        clapJuceShim->setResizable(true);
    }
    virtual ~SixSinesClap(){};

    std::unique_ptr<Synth> engine;
    size_t blockPos{0};

  protected:
    bool activate(double sampleRate, uint32_t minFrameCount,
                  uint32_t maxFrameCount) noexcept override
    {
        engine->setSampleRate(sampleRate);
        return true;
    }

    void onMainThread() noexcept override
    {
        engine->onMainThread();
    }

    bool implementsAudioPorts() const noexcept override { return true; }
    uint32_t audioPortsCount(bool isInput) const noexcept override
    {
        return isInput ? 0 : (multiOut ? 7 : 1);
    }
    bool audioPortsInfo(uint32_t index, bool isInput,
                        clap_audio_port_info *info) const noexcept override
    {
        assert(!isInput);
        if (isInput || index > (multiOut ? 6 : 0))
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
        }
        else
        {
            engine->monoValues.tempoSyncRatio = 1.f;
        }

        static constexpr int outBus{multiOut ? 1 + numOps : 1};
        static constexpr int outChan{multiOut ? (1 + numOps) * 2 : 2};
        float *out[outChan];
        for (auto i = 0; i < outBus; ++i)
        {
            auto lo = process->audio_outputs[i].data32;
            out[2 * i] = lo[0];
            out[2 * i + 1] = lo[1];
        }

        for (auto s = 0U; s < process->frames_count; ++s)
        {
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
        engine->mainToAudio.push({Synth::MainToAudioMsg::SEND_PREP_FOR_STREAM});
        if (_host.canUseParams())
            _host.paramsRequestFlush();

        // best efforts on that message for now
        static constexpr int maxIts{5};
        int i{0};
        for (i = 0; i < maxIts && !engine->readyForStream; ++i)
        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(4ms);
        }
        if (i == maxIts && !engine->readyForStream)
        {
            // sigh. something is wonky
            engine->prepForStream();
        }

        auto res = sst::plugininfra::patch_support::patchToOutStream(engine->patch, ostream);
        engine->readyForStream = false;
        return res;
    }
    bool stateLoad(const clap_istream *istream) noexcept override
    {
        Patch patchCopy;
        if (!sst::plugininfra::patch_support::inStreamToPatch(istream, patchCopy))
            return false;

        presets::PresetManager::sendEntirePatchToAudio(patchCopy, engine->mainToAudio,
                                                       patchCopy.name, _host.host());
        if (_host.canUseParams())
        {
            _host.paramsRescan(CLAP_PARAM_RESCAN_VALUES);
            _host.paramsRequestFlush();
        }
        return true;
    }

    bool implementsParams() const noexcept override { return true; }
    uint32_t paramsCount() const noexcept override { return engine->patch.params.size(); }
    bool paramsInfo(uint32_t paramIndex, clap_param_info *info) const noexcept override
    {
        return sst::plugininfra::patch_support::patchParamsInfo(paramIndex, info, engine->patch);
    }
    bool paramsValue(clap_id paramId, double *value) noexcept override
    {
        return sst::plugininfra::patch_support::patchParamsValue(paramId, value, engine->patch);
    }
    bool paramsValueToText(clap_id paramId, double value, char *display,
                           uint32_t size) noexcept override
    {
        return sst::plugininfra::patch_support::patchParamsValueToText(paramId, value, display,
                                                                       size, engine->patch);
    }
    bool paramsTextToValue(clap_id paramId, const char *display, double *value) noexcept override
    {
        return sst::plugininfra::patch_support::patchParamsTextToValue(paramId, display, value,
                                                                       engine->patch);
    }
    void paramsFlush(const clap_input_events *in, const clap_output_events *out) noexcept override
    {
        auto sz = in->size(in);

        for (int i = 0; i < sz; ++i)
        {
            const clap_event_header_t *nextEvent{nullptr};
            nextEvent = in->get(in, i);
            handleEvent(nextEvent);
        }

        engine->processUIQueue(out);
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
                auto p = fs::path(fs::u8path(location));
                if (p.extension() == "sxsnp")
                {
                    std::ifstream t(p);
                    if (!t.is_open())
                        return false;
                    std::stringstream buffer;
                    buffer << t.rdbuf();

                    Patch patchCopy;
                    patchCopy.fromState(buffer.str());

                    auto dn = p.filename().replace_extension("").u8string();
                    presets::PresetManager::sendEntirePatchToAudio(patchCopy, engine->mainToAudio,
                                                                   patchCopy.name, _host.host());
                    return true;
                }
            }
            catch (const fs::filesystem_error &e)
            {
                SXSNLOG("File System Error " << e.what() << " with " << location);
            }
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
            engine->audioToUi, engine->mainToAudio, _host.host());

        res->onZoomChanged = [this](auto f)
        {
            if (_host.canUseGui() && clapJuceShim->isEditorAttached())
            {
                auto s = f * clapJuceShim->getGuiScale();
                _host.guiRequestResize(baconpaul::six_sines::ui::SixSinesEditor::edWidth * s,
                                       baconpaul::six_sines::ui::SixSinesEditor::edHeight * s);
            }
        };

        onShow = [e = res.get()]()
        {
            e->setZoomFactor(e->zoomFactor);
            return true;
        };
        res->sneakyStartupGrabFrom(engine->patch);
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

    static uint32_t vst3_getNumMIDIChannels(const clap_plugin *plugin, uint32_t note_port)
    {
        return 16;
    }
    static uint32_t vst3_supportedNoteExpressions(const clap_plugin *plugin)
    {
        return clap_supported_note_expressions::AS_VST3_NOTE_EXPRESSION_TUNING |
               clap_supported_note_expressions::AS_VST3_NOTE_EXPRESSION_PAN;
    }

    const void *extension(const char *id) noexcept override
    {
        if (strcmp(id, CLAP_PLUGIN_AS_VST3) == 0)
        {
            static clap_plugin_as_vst3 v3p{vst3_getNumMIDIChannels, vst3_supportedNoteExpressions};
            return &v3p;
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
} // namespace baconpaul::six_sines

namespace chlp = clap::helpers;
namespace bpss = baconpaul::six_sines::clapimpl;

template class chlp::Plugin<bpss::misLevel, bpss::checkLevel>;
template class chlp::HostProxy<bpss::misLevel, bpss::checkLevel>;
