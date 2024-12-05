/*
 * BaconPaul's FM Atrocity
 *
 * A mess, with FM.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */
#include "configuration.h"
#include <clap/clap.h>

#include <clap/helpers/plugin.hh>
#include "synth/synth.h"

#include <clap/helpers/plugin.hxx>
#include <clap/helpers/host-proxy.hxx>

#include <memory>

namespace baconpaul::fm
{

extern const clap_plugin_descriptor *getDescriptor();

namespace clapimpl
{

static constexpr clap::helpers::MisbehaviourHandler misLevel =
    clap::helpers::MisbehaviourHandler::Ignore;
static constexpr clap::helpers::CheckingLevel checkLevel = clap::helpers::CheckingLevel::Maximal;

using plugHelper_t = clap::helpers::Plugin<misLevel, checkLevel>;

struct FMClap : public plugHelper_t // , sst::clap_juce_shim::EditorProvider
{
    FMClap(const clap_host *h) : plugHelper_t(getDescriptor(), h) {}
    virtual ~FMClap(){};

    std::unique_ptr<Synth> engine;
    size_t blockPos{0};

  protected:
    bool activate(double sampleRate, uint32_t minFrameCount,
                  uint32_t maxFrameCount) noexcept override
    {
        return true;
    }

    void onMainThread() noexcept override {}

    bool implementsAudioPorts() const noexcept override { return false; }
    // uint32_t audioPortsCount(bool isInput) const noexcept override;
    // bool audioPortsInfo(uint32_t index, bool isInput,
    //                     clap_audio_port_info *info) const noexcept override;

    bool implementsNotePorts() const noexcept override { return false; }
    // uint32_t notePortsCount(bool isInput) const noexcept override;
    // bool notePortsInfo(uint32_t index, bool isInput,
    //                       clap_note_port_info *info) const noexcept override;

    clap_process_status process(const clap_process *process) noexcept override
    {
        return CLAP_PROCESS_CONTINUE;
    }
    // bool handleEvent(const clap_event_header_t *);

    bool implementsState() const noexcept override { return false; }
    // bool stateSave(const clap_ostream *stream) noexcept override;
    // bool stateLoad(const clap_istream *stream) noexcept override;

    bool implementsParams() const noexcept override { return false; }
    // uint32_t paramsCount() const noexcept override;
    // bool paramsInfo(uint32_t paramIndex, clap_param_info *info) const noexcept override;
    // bool paramsValue(clap_id paramId, double *value) noexcept override;
    // bool paramsValueToText(clap_id paramId, double value, char *display,
    //                           uint32_t size) noexcept override;
    // bool paramsTextToValue(clap_id paramId, const char *display, double *value) noexcept
    // override;
    // void paramsFlush(const clap_input_events *in, const clap_output_events *out) noexcept
    // override;

  public:
#if 0
    bool implementsGui() const noexcept override { return clapJuceShim != nullptr; }
    std::unique_ptr<sst::clap_juce_shim::ClapJuceShim> clapJuceShim;
    ADD_SHIM_IMPLEMENTATION(clapJuceShim)
    ADD_SHIM_LINUX_TIMER(clapJuceShim)
    std::unique_ptr<juce::Component> createEditor() override;

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
#endif
};

} // namespace clapimpl

const clap_plugin *makePlugin(const clap_host *h)
{
    FMLOG("makePlugin");
    auto res = new baconpaul::fm::clapimpl::FMClap(h);
    return res->clapPlugin();
}
} // namespace baconpaul::fm

namespace chlp = clap::helpers;
namespace bfmc = baconpaul::fm::clapimpl;

template class chlp::Plugin<bfmc::misLevel, bfmc::checkLevel>;
template class chlp::HostProxy<bfmc::misLevel, bfmc::checkLevel>;
