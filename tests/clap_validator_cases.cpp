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
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "clap/clap.h"
#include "clap/ext/params.h"
#include "clap/ext/state.h"
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

struct Driver
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

    Driver()
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
        patch = getPatchFromPlugin(plugin);
    }
    ~Driver()
    {
        if (active)
        {
            plugin->stop_processing(plugin);
            plugin->deactivate(plugin);
        }
        plugin->destroy(plugin);
    }

    bool active{false};
    void activate()
    {
        plugin->activate(plugin, hostRate, 1, 4096);
        plugin->start_processing(plugin);
        active = true;
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

    void note(uint16_t type, int16_t port, int16_t channel, int16_t key, int32_t noteId)
    {
        Event e{};
        e.note.header = {sizeof(clap_event_note_t), 0, CLAP_CORE_EVENT_SPACE_ID, type, 0};
        e.note.note_id = noteId;
        e.note.port_index = port;
        e.note.channel = channel;
        e.note.key = key;
        e.note.velocity = 1.0;
        pending.push_back(e);
    }

    // pending events land at the start of the first block
    std::vector<float> run(int frames)
    {
        static constexpr int block{256};
        std::vector<float> res;
        std::vector<float> inL(block), inR(block), outL(block), outR(block);

        while ((int)res.size() < frames)
        {
            auto n = std::min(block, frames - (int)res.size());

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

    // one sine on op 1 with flat envelopes and no release
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

        auto &sn = p.sourceNodes[0];
        param(sn.active, 1.f);
        param(sn.waveForm, (float)SinTable::SIN);
        param(sn.ratio, 0.f);
        param(sn.keyTrack, 1.f);
        flatEnv(sn);

        flatEnv(p.mixerNodes[0]);
        param(p.mixerNodes[0].level, 1.f);

        flatEnv(p.output);
        param(p.output.level, 1.f);
    }
};

float peak(const std::vector<float> &x, size_t from)
{
    float r{0};
    for (auto i = from; i < x.size(); ++i)
        r = std::max(r, std::fabs(x[i]));
    return r;
}

struct ByteStream
{
    std::vector<uint8_t> data;
    size_t pos{0};
    clap_istream_t stream{};

    explicit ByteStream(std::vector<uint8_t> d) : data(std::move(d))
    {
        stream.ctx = this;
        stream.read = [](const clap_istream_t *s, void *buffer, uint64_t size) -> int64_t
        {
            auto *self = static_cast<ByteStream *>(s->ctx);
            auto n = std::min<uint64_t>(size, self->data.size() - self->pos);
            memcpy(buffer, self->data.data() + self->pos, n);
            self->pos += n;
            return (int64_t)n;
        };
    }
};
} // namespace

TEST_CASE("Every parameter's text converts back to the same text", "[clap][validator]")
{
    Driver d;
    auto *params = static_cast<const clap_plugin_params_t *>(
        d.plugin->get_extension(d.plugin, CLAP_EXT_PARAMS));
    REQUIRE(params);

    static constexpr int steps{5};
    auto n = params->count(d.plugin);
    for (uint32_t pi = 0; pi < n; ++pi)
    {
        clap_param_info_t info{};
        REQUIRE(params->get_info(d.plugin, pi, &info));
        for (int s = 0; s < steps; ++s)
        {
            auto v = info.min_value + (info.max_value - info.min_value) * s / (steps - 1);
            INFO("param '" << info.name << "' id " << info.id << " value " << v);

            char text[CLAP_NAME_SIZE]{};
            REQUIRE(params->value_to_text(d.plugin, info.id, v, text, sizeof(text)));

            double back{0};
            INFO("text '" << text << "'");
            auto parsed = params->text_to_value(d.plugin, info.id, text, &back);
            CHECK(parsed);
            if (!parsed)
                continue;

            char again[CLAP_NAME_SIZE]{};
            REQUIRE(params->value_to_text(d.plugin, info.id, back, again, sizeof(again)));
            CHECK(std::string(text) == std::string(again));
        }
    }
}

TEST_CASE("Loading random bytes as state is refused", "[clap][validator]")
{
    Driver d;
    auto *state =
        static_cast<const clap_plugin_state_t *>(d.plugin->get_extension(d.plugin, CLAP_EXT_STATE));
    REQUIRE(state);

    std::mt19937 gen(8675309);
    std::uniform_int_distribution<int> dist(0, 255);
    std::vector<uint8_t> junk(1 << 16);
    for (auto &b : junk)
        b = (uint8_t)dist(gen);

    ByteStream bs(junk);
    REQUIRE_FALSE(state->load(d.plugin, &bs.stream));
}

TEST_CASE("A wildcard note off releases the notes it matches", "[clap][validator]")
{
    struct Case
    {
        const char *name;
        int16_t port, channel, key;
        int32_t noteId;
    };
    auto c = GENERATE(Case{"any port", -1, 0, 60, -1}, Case{"any channel", 0, -1, 60, -1},
                      Case{"any key", 0, 0, -1, -1}, Case{"only a note id", -1, -1, -1, 17},
                      Case{"everything", -1, -1, -1, -1});
    auto mono = GENERATE(false, true);
    INFO(c.name << (mono ? " mono" : " poly"));

    Driver d;
    d.activate();
    d.setUpSine();
    d.param(d.patch->output.playMode, mono ? 1.f : 0.f);
    d.note(CLAP_EVENT_NOTE_ON, 0, 0, 60, 17);
    auto held = d.run(4096);
    REQUIRE(peak(held, 2048) > 0.1f);

    d.note(CLAP_EVENT_NOTE_OFF, c.port, c.channel, c.key, c.noteId);
    auto released = d.run(8192);
    REQUIRE(peak(released, 4096) < 1e-4f);
}
