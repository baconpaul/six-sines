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

#include "catch2/catch2.hpp"

#include <cmath>
#include <functional>
#include <vector>

#include "clap/clap.h"
#include "configuration.h"
#include "dsp/sintable.h"
#include "synth/patch.h"

using namespace baconpaul::six_sines;

namespace baconpaul::six_sines
{
extern const clap_plugin *makePlugin(const clap_host *h, bool multiOut);
extern const Patch *getPatchFromPlugin(const clap_plugin_t *plugin);
} // namespace baconpaul::six_sines

namespace
{
static constexpr double hostRate{48000.0};
static constexpr int sineKey{69};
static constexpr double sineHz{440.0};

// drives the plugin purely through clap: param and note events in, audio out
struct ClapDriver
{
    clap_host_t host{};
    const clap_plugin *plugin{nullptr};
    const Patch *patch{nullptr};

    union Event
    {
        clap_event_header_t header;
        clap_event_param_value_t param;
        clap_event_note_t note;
    };
    std::vector<Event> pending;

    ClapDriver()
    {
        host.clap_version = CLAP_VERSION;
        host.name = "Six Sines Test Host";
        host.vendor = "Test";
        host.url = "";
        host.version = "0";
        host.get_extension = [](const clap_host_t *, const char *) -> const void *
        { return nullptr; };
        host.request_restart = [](const clap_host_t *) {};
        host.request_process = [](const clap_host_t *) {};
        host.request_callback = [](const clap_host_t *) {};

        plugin = makePlugin(&host, false);
        plugin->init(plugin);
        plugin->activate(plugin, hostRate, 1, 4096);
        plugin->start_processing(plugin);
        patch = getPatchFromPlugin(plugin);
    }
    ~ClapDriver()
    {
        plugin->stop_processing(plugin);
        plugin->deactivate(plugin);
        plugin->destroy(plugin);
    }

    void param(const Param &p, double v)
    {
        Event e{};
        e.param.header = {sizeof(clap_event_param_value_t), 0, CLAP_CORE_EVENT_SPACE_ID,
                          CLAP_EVENT_PARAM_VALUE, 0};
        e.param.param_id = p.meta.id;
        e.param.note_id = -1;
        e.param.port_index = -1;
        e.param.channel = -1;
        e.param.key = -1;
        e.param.value = v;
        pending.push_back(e);
    }

    void noteOn(int key)
    {
        Event e{};
        e.note.header = {sizeof(clap_event_note_t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_ON,
                         0};
        e.note.note_id = -1;
        e.note.port_index = 0;
        e.note.channel = 0;
        e.note.key = key;
        e.note.velocity = 1.0;
        pending.push_back(e);
    }

    // pending events land at the start of the first host block
    std::vector<float> run(int frames, int hostBlock)
    {
        std::vector<float> res;
        res.reserve(frames);
        std::vector<float> inL(hostBlock), inR(hostBlock), outL(hostBlock), outR(hostBlock);

        while ((int)res.size() < frames)
        {
            auto n = std::min(hostBlock, frames - (int)res.size());

            clap_input_events_t in{};
            in.ctx = &pending;
            in.size = [](const clap_input_events_t *l) -> uint32_t
            { return (uint32_t)static_cast<std::vector<Event> *>(l->ctx)->size(); };
            in.get = [](const clap_input_events_t *l, uint32_t i) -> const clap_event_header_t *
            { return &(*static_cast<std::vector<Event> *>(l->ctx))[i].header; };

            clap_output_events_t out{};
            out.try_push = [](const clap_output_events_t *, const clap_event_header_t *)
            { return true; };

            float *ins[2]{inL.data(), inR.data()};
            float *outs[2]{outL.data(), outR.data()};
            clap_audio_buffer_t inBuf{}, outBuf{};
            inBuf.data32 = ins;
            inBuf.channel_count = 2;
            outBuf.data32 = outs;
            outBuf.channel_count = 2;

            clap_process_t proc{};
            proc.steady_time = -1;
            proc.frames_count = (uint32_t)n;
            proc.audio_inputs = &inBuf;
            proc.audio_inputs_count = 1;
            proc.audio_outputs = &outBuf;
            proc.audio_outputs_count = 1;
            proc.in_events = &in;
            proc.out_events = &out;

            plugin->process(plugin, &proc);
            pending.clear();

            res.insert(res.end(), outL.begin(), outL.begin() + n);
        }
        return res;
    }

    // one sine on op 1 with flat envelopes into the main bus
    void setUpSine()
    {
        auto &p = *patch;
        auto flatEnv = [this](auto &n)
        {
            param(n.delay, 0.f);
            param(n.attack, 0.f);
            param(n.hold, 0.f);
            param(n.decay, 0.f);
            param(n.sustain, 1.f);
            param(n.release, 0.f);
        };
        param(p.output.playMode, 0.f);
        param(p.output.unisonCount, 1.f);
        param(p.output.ultrasonicFilter, 0.f);

        auto &sn = p.sourceNodes[0];
        param(sn.active, 1.f);
        param(sn.waveForm, (float)SinTable::SIN);
        param(sn.ratio, 0.f);
        param(sn.keyTrack, 1.f);
        flatEnv(sn);

        flatEnv(p.mixerNodes[0]);
        param(p.mixerNodes[0].level, 1.f);
        param(p.mixerNodes[0].pan, 0.f);

        flatEnv(p.output);
        param(p.output.level, 1.f);
        param(p.output.velSensitivity, 0.f);
    }
};

// a sine obeys x[n+1] + x[n-1] = 2 cos(w) x[n]; a dropped, repeated or stale sample breaks it
struct SineCheck
{
    double rms{0};
    double worstResidual{0};
    size_t worstAt{0};
};

SineCheck checkSine(const std::vector<float> &x)
{
    SineCheck c;
    auto k = 2 * std::cos(2 * M_PI * sineHz / hostRate);
    double acc{0};
    for (size_t i = 1; i + 1 < x.size(); ++i)
    {
        acc += (double)x[i] * x[i];
        auto r = std::fabs(x[i + 1] + x[i - 1] - k * x[i]);
        if (r > c.worstResidual)
        {
            c.worstResidual = r;
            c.worstAt = i;
        }
    }
    c.rms = std::sqrt(acc / (x.size() - 2));
    return c;
}
} // namespace

TEST_CASE("Odd host blocks give a continuous sine at every rate and block size",
          "[engine-block-size][clap]")
{
    for (auto hostBlock : {7, 43})
    {
        for (auto fsr : {FSR_1X, FSR_2X, FSR_HI})
        {
            for (auto ebs : {EBS_8, EBS_16, EBS_32})
            {
                INFO("host block " << hostBlock << " force rate " << fsr << " engine block "
                                   << ebs);
                ClapDriver d;
                d.setUpSine();
                d.param(d.patch->output.forceSampleRate, (float)fsr);
                d.param(d.patch->output.engineBlockSize, (float)ebs);
                // let the float lags and the resampler settle before the note
                d.run(4800, hostBlock);

                d.noteOn(sineKey);
                d.run(4800, hostBlock);

                auto c = checkSine(d.run(48000, hostBlock));
                INFO("rms " << c.rms << " worst residual " << c.worstResidual << " at "
                            << c.worstAt);
                REQUIRE(c.rms > 0.09);
                REQUIRE(c.worstResidual < 1e-4);
            }
        }
    }
}

TEST_CASE("A rate change keeps the values its parameters were gliding to", "[clap]")
{
    double rms[4]{};
    for (auto srs : {SR_110120, SR_132144, SR_176192, SR_220240})
    {
        INFO("strategy " << srs);
        ClapDriver d;
        // the levels arrive as lagged floats in the same block that changes the rate
        d.setUpSine();
        d.param(d.patch->output.sampleRateStrategy, (float)srs);
        d.run(4800, 43);
        d.noteOn(sineKey);
        d.run(4800, 43);
        rms[srs] = checkSine(d.run(48000, 43)).rms;
        REQUIRE(20 * std::log10(rms[srs] / rms[SR_110120]) == Approx(0).margin(0.05));
    }
}

namespace
{
// hold a sine, make an engine change under it, then check it faded out and the next note is clean
void checkChangeFades(int hostBlock, const std::function<void(ClapDriver &)> &before,
                      const std::function<void(ClapDriver &)> &change, double residualLimit = 1e-4)
{
    auto maxStep = [](const std::vector<float> &x, size_t from)
    {
        double r{0};
        for (auto i = std::max(from, (size_t)1); i < x.size(); ++i)
            r = std::max(r, (double)std::fabs(x[i] - x[i - 1]));
        return r;
    };

    ClapDriver d;
    d.setUpSine();
    before(d);
    d.run(4800, hostBlock);
    d.noteOn(sineKey);
    // land the change away from a zero crossing
    auto x = d.run(4855, hostBlock);
    auto steady = maxStep(x, x.size() - 480);

    change(d);
    auto post = d.run(2400, hostBlock);
    auto preLen = x.size();
    x.insert(x.end(), post.begin(), post.end());
    auto across = maxStep(x, preLen - 1);
    INFO("steady step " << steady << " step across the change " << across);
    REQUIRE(across < 1.5 * steady);

    double tail{0};
    for (auto i = post.size() / 2; i < post.size(); ++i)
        tail = std::max(tail, (double)std::fabs(post[i]));
    REQUIRE(tail < 1e-4);

    d.noteOn(sineKey);
    d.run(4800, hostBlock);
    auto c = checkSine(d.run(9600, hostBlock));
    REQUIRE(c.rms > 0.09);
    REQUIRE(c.worstResidual < residualLimit);
}
} // namespace

TEST_CASE("Changing block size under a held note fades rather than clicks",
          "[engine-block-size][clap]")
{
    for (auto hostBlock : {7, 43})
    {
        for (auto fsr : {FSR_1X, FSR_2X, FSR_HI})
        {
            for (auto [from, to] :
                 {std::pair{EBS_8, EBS_16}, std::pair{EBS_8, EBS_32}, std::pair{EBS_16, EBS_8},
                  std::pair{EBS_16, EBS_32}, std::pair{EBS_32, EBS_8}, std::pair{EBS_32, EBS_16}})
            {
                INFO("host block " << hostBlock << " force rate " << fsr << " from " << from
                                   << " to " << to);
                checkChangeFades(
                    hostBlock,
                    [fsr = fsr, from = from](auto &d)
                    {
                        d.param(d.patch->output.forceSampleRate, (float)fsr);
                        d.param(d.patch->output.engineBlockSize, (float)from);
                    },
                    [to = to](auto &d) { d.param(d.patch->output.engineBlockSize, (float)to); });
            }
        }
    }
}

TEST_CASE("Changing the engine rate under a held note fades rather than clicks",
          "[engine-block-size][clap]")
{
    for (auto hostBlock : {7, 43})
    {
        for (auto ebs : {EBS_8, EBS_32})
        {
            for (auto [from, to] :
                 {std::pair{FSR_1X, FSR_2X}, std::pair{FSR_1X, FSR_HI}, std::pair{FSR_2X, FSR_1X},
                  std::pair{FSR_2X, FSR_HI}, std::pair{FSR_HI, FSR_1X}, std::pair{FSR_HI, FSR_2X}})
            {
                INFO("host block " << hostBlock << " engine block " << ebs << " force rate from "
                                   << from << " to " << to);
                checkChangeFades(
                    hostBlock,
                    [ebs = ebs, from = from](auto &d)
                    {
                        d.param(d.patch->output.engineBlockSize, (float)ebs);
                        d.param(d.patch->output.forceSampleRate, (float)from);
                    },
                    [to = to](auto &d) { d.param(d.patch->output.forceSampleRate, (float)to); });
            }

            INFO("host block " << hostBlock << " engine block " << ebs);
            checkChangeFades(
                hostBlock,
                [ebs = ebs](auto &d) { d.param(d.patch->output.engineBlockSize, (float)ebs); },
                [](auto &d) { d.param(d.patch->output.sampleRateStrategy, (float)SR_220240); });
            checkChangeFades(
                hostBlock,
                [ebs = ebs](auto &d) { d.param(d.patch->output.engineBlockSize, (float)ebs); },
                [](auto &d) { d.param(d.patch->output.resampleEngine, (float)LANCZOS); },
                // lanczos leaves 7e-4 on a steady sine; a repeated sample is ten times that
                2e-3);
        }
    }
}
