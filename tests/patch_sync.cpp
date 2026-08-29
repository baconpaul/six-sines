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

// Exercises the patch / patchMain sync seams introduced by the ownership rework:
//   - Patch::copyValuesFrom (value-only copy used by activate())
//   - Synth::drainAudioToMainInto (audio -> patchMain main-thread drain)
//   - processUIQueue (UI -> audio-thread patch)
//   - paramsFlushMainThread (inactive host param flush -> patchMain)
//   - the DAW session state (dawStateMain) streaming on patchMain
// No CLAP host is needed: Synth works standalone, and handleParamValue only calls
// request_callback when clapHost is set (it is null here). Patch is large, so every Patch /
// Synth is heap-allocated (the stack copy blows the Windows stack).

#include "catch2/catch2.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "synth/synth.h"
#include "sst/plugininfra/patch-support/patch_base_clap_adapter.h"

using namespace baconpaul::six_sines;

namespace
{
// A minimal output-events sink that accepts and discards everything.
bool outTryPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }
clap_output_events_t makeOut() { return clap_output_events_t{nullptr, outTryPush}; }

bool approxEq(float a, float b, float tol = 1e-5f) { return std::abs(a - b) < tol; }

// A minimal clap_input_events source backed by a vector of param-value events.
struct InputEvents
{
    std::vector<clap_event_param_value_t> events;

    void pushParam(uint32_t id, double value)
    {
        clap_event_param_value_t p{};
        p.header.size = sizeof(p);
        p.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        p.header.type = CLAP_EVENT_PARAM_VALUE;
        p.param_id = id;
        p.note_id = -1;
        p.port_index = -1;
        p.channel = -1;
        p.key = -1;
        p.value = value;
        events.push_back(p);
    }

    clap_input_events_t asClap()
    {
        clap_input_events_t in;
        in.ctx = this;
        in.size = [](const clap_input_events_t *l)
        { return (uint32_t)static_cast<InputEvents *>(l->ctx)->events.size(); };
        in.get = [](const clap_input_events_t *l, uint32_t i) -> const clap_event_header_t *
        { return &static_cast<InputEvents *>(l->ctx)->events[i].header; };
        return in;
    }
};
} // namespace

TEST_CASE("copyValuesFrom copies values, macro names, name, author and dirty", "[patch-sync]")
{
    // The mod-matrix index tables must be initialized before any Patch is constructed (the Synth
    // forces this via its isTableInitialized member; a standalone Patch must do it explicitly).
    MatrixIndex::initialize();

    auto aP = std::make_unique<Patch>();
    auto bP = std::make_unique<Patch>();
    auto &a = *aP;
    auto &b = *bP;

    // Distinct per-param values in the source.
    for (auto &[id, p] : a.paramMap)
        p->value = 0.01f * (float)(id % 97) + 0.123f;

    std::strncpy(a.macroNames[2].data(), "Cutoff Sweep", 63);
    std::strncpy(a.name, "CopyTest", 63);
    a.setAuthor("Ada");
    a.dirty = true;

    b.copyValuesFrom(a);

    SECTION("all param values match")
    {
        for (auto &[id, p] : a.paramMap)
            REQUIRE(b.paramMap.at(id)->value == p->value);
    }

    SECTION("macro names, name, author, dirty match")
    {
        REQUIRE(std::string(b.macroNames[2].data()) == "Cutoff Sweep");
        REQUIRE(std::string(b.name) == "CopyTest");
        REQUIRE(std::string(b.author) == "Ada");
        REQUIRE(b.dirty == true);
    }

    SECTION("pointer identity: b's maps point inside b, not a")
    {
        // Guards against an accidental memberwise operator= that would alias the Param*
        // containers across the two Patch objects.
        for (auto &[id, p] : b.paramMap)
            REQUIRE(p != a.paramMap.at(id));
    }
}

TEST_CASE("toState / fromState round-trips values and macro names", "[patch-sync]")
{
    MatrixIndex::initialize(); // see the copyValuesFrom case

    auto aP = std::make_unique<Patch>();
    auto &a = *aP;
    for (auto &[id, p] : a.paramMap)
    {
        auto &m = p->meta;
        p->value = m.minVal + 0.37f * (m.maxVal - m.minVal);
    }
    std::strncpy(a.macroNames[0].data(), "My Macro", 63);

    auto state = a.toState();

    auto bP = std::make_unique<Patch>();
    auto &b = *bP;
    REQUIRE(b.fromState(state));

    for (auto &[id, p] : a.paramMap)
        REQUIRE(approxEq(b.paramMap.at(id)->value, p->value));
    REQUIRE(std::string(b.macroNames[0].data()) == "My Macro");
}

TEST_CASE("UI edit reaches the audio patch through processUIQueue", "[patch-sync]")
{
    auto enginePtr = std::make_unique<Synth>(false);
    auto &engine = *enginePtr;
    auto out = makeOut();

    const uint32_t pid = engine.patch.output.outputGain.meta.id;
    const float target = 0.5f;

    // The UI writes patchMain directly, then queues the change for the audio thread.
    engine.patchMain.paramMap.at(pid)->value = target;
    engine.mainToAudio.push({Synth::MainToAudioMsg::BEGIN_EDIT, pid});
    engine.mainToAudio.push({Synth::MainToAudioMsg::SET_PARAM, pid, target});
    engine.mainToAudio.push({Synth::MainToAudioMsg::END_EDIT, pid});

    engine.processUIQueue(&out);
    engine.snapAllParams(); // settle the smoothed destination

    REQUIRE(approxEq(engine.patch.paramMap.at(pid)->value, target));
}

TEST_CASE("Audio-thread param change drains back into patchMain", "[patch-sync]")
{
    auto enginePtr = std::make_unique<Synth>(false);
    auto &engine = *enginePtr;

    const uint32_t pid = engine.patch.macroNodes[0].level.meta.id;
    const float target = 0.42f;

    // Simulate host automation landing on the audio thread. clapHost is null, so no callback.
    engine.handleParamValue(nullptr, pid, target);
    engine.snapAllParams();
    REQUIRE(approxEq(engine.patch.paramMap.at(pid)->value, target));

    // patchMain is stale until the main thread drains audioToMain.
    engine.drainAudioToMainInto(engine.patchMain);
    REQUIRE(approxEq(engine.patchMain.paramMap.at(pid)->value, target));
}

TEST_CASE("paramsFlushMainThread applies host values to patchMain and forces a UI rebuild",
          "[patch-sync]")
{
    auto enginePtr = std::make_unique<Synth>(false);
    auto &engine = *enginePtr;
    auto out = makeOut();

    const uint32_t pid = engine.patch.output.outputGain.meta.id;
    const float target = 0.5f;
    const auto rebuildBefore = engine.uiForceRebuild.load();

    InputEvents in;
    in.pushParam(pid, target);
    auto inC = in.asClap();

    // Inactive path: host param changes land here, not on the audio thread.
    engine.paramsFlushMainThread(&inC, &out);

    // The value reaches patchMain (the main-thread source of truth) ...
    REQUIRE(approxEq(engine.patchMain.paramMap.at(pid)->value, target));
    // ... an open editor is told to rebuild (no audio thread to push UPDATE_PARAM) ...
    REQUIRE(engine.uiForceRebuild.load() == rebuildBefore + 1);
    // ... and the audio-thread `patch` is left untouched.
    REQUIRE_FALSE(approxEq(engine.patch.paramMap.at(pid)->value, target));
}

TEST_CASE("paramsFlushMainThread with no incoming param values does not bump uiForceRebuild",
          "[patch-sync]")
{
    auto enginePtr = std::make_unique<Synth>(false);
    auto &engine = *enginePtr;
    auto out = makeOut();
    const auto rebuildBefore = engine.uiForceRebuild.load();

    InputEvents in; // empty
    auto inC = in.asClap();
    engine.paramsFlushMainThread(&inC, &out);

    REQUIRE(engine.uiForceRebuild.load() == rebuildBefore);
}

TEST_CASE("drainAudioToMainInto applies only patch-affecting messages", "[patch-sync]")
{
    auto enginePtr = std::make_unique<Synth>(false);
    auto &engine = *enginePtr;

    const uint32_t pid = engine.patch.output.outputGain.meta.id;
    const float target = 0.75f;

    // Interleave UI-only telemetry with a real param update.
    engine.audioToMain.push({Synth::AudioToMainMsg::UPDATE_VU, 0, 0.9f, 0.8f});
    engine.audioToMain.push({Synth::AudioToMainMsg::UPDATE_VOICE_COUNT, 3});
    engine.audioToMain.push({Synth::AudioToMainMsg::UPDATE_PARAM, pid, target});
    engine.audioToMain.push({Synth::AudioToMainMsg::SEND_SAMPLE_RATE, 0, 48000.f, 120000.f});
    engine.audioToMain.push({Synth::AudioToMainMsg::MTS_POINTER, 0, 0, 0, nullptr});

    engine.drainAudioToMainInto(engine.patchMain);

    REQUIRE(approxEq(engine.patchMain.paramMap.at(pid)->value, target));
    // The queue is fully consumed (VU / voice count / sample rate / MTS were discarded).
    REQUIRE_FALSE(engine.audioToMain.pop().has_value());
}

TEST_CASE("DAW session state round-trips through patchMain streaming", "[patch-sync]")
{
    // stateSave/stateLoad stream patchMain, so the <dawExtraState> hooks must be wired on patchMain
    // (not the audio patch). This pins that the colour map + MPE + smoothing survive a round-trip.
    auto a = std::make_unique<Synth>(false);
    a->dawStateMain.main.colorMapXml = "THEME_XML_BLOB";
    a->dawStateMain.audio.mpeActive = true;
    a->dawStateMain.audio.mpeBendRange = 48;
    a->dawStateMain.audio.midiCCSmoothingTimeMs = 12.5f;
    a->dawStateMain.audio.paramAutomationSmoothingTimeMs = 7.5f;

    const auto state = a->patchMain.toState(/*withDawExtraState*/ true); // as stateSave does

    auto b = std::make_unique<Synth>(false);
    REQUIRE(b->patchMain.fromState(state));

    REQUIRE(b->dawStateMain.main.colorMapXml == "THEME_XML_BLOB");
    REQUIRE(b->dawStateMain.audio.mpeActive == true);
    REQUIRE(b->dawStateMain.audio.mpeBendRange == 48);
    REQUIRE(approxEq(b->dawStateMain.audio.midiCCSmoothingTimeMs, 12.5f));
    REQUIRE(approxEq(b->dawStateMain.audio.paramAutomationSmoothingTimeMs, 7.5f));
}

TEST_CASE("Host automation carrying a clap cookie reaches the audio patch", "[patch-sync]")
{
    auto enginePtr = std::make_unique<Synth>(false);
    auto &engine = *enginePtr;

    const uint32_t pid = engine.patch.macroNodes[0].level.meta.id;
    const float target = 0.42f;

    // paramsInfo reads patchMain for the info fields but must cookie the audio patch.
    uint32_t idx{0};
    bool found{false};
    for (uint32_t i = 0; i < engine.patchMain.params.size(); ++i)
    {
        if (engine.patchMain.params[i]->meta.id == pid)
        {
            idx = i;
            found = true;
            break;
        }
    }
    REQUIRE(found);

    clap_param_info info{};
    REQUIRE(sst::plugininfra::patch_support::patchParamsInfo(idx, &info, engine.patchMain));
    info.cookie = engine.clapCookieFor(info.id);

    REQUIRE(info.cookie == (void *)engine.patch.paramMap.at(pid));
    REQUIRE(info.cookie != (void *)engine.patchMain.paramMap.at(pid));

    clap_event_param_value_t pevt{};
    pevt.header.size = sizeof(pevt);
    pevt.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    pevt.header.type = CLAP_EVENT_PARAM_VALUE;
    pevt.param_id = pid;
    pevt.cookie = info.cookie;
    pevt.note_id = -1;
    pevt.port_index = -1;
    pevt.channel = -1;
    pevt.key = -1;
    pevt.value = target;

    // Exactly what the clap process() event loop does with the host's cookie.
    auto par = sst::plugininfra::patch_support::paramFromClapEvent<Param>(&pevt, engine.patch);
    engine.handleParamValue(par, pevt.param_id, pevt.value);
    engine.snapAllParams();

    // The engine must actually hear the automation ...
    REQUIRE(approxEq(engine.patch.paramMap.at(pid)->value, target));
    // ... and the audio thread must not have written patchMain behind the main thread's back.
    REQUIRE_FALSE(approxEq(engine.patchMain.paramMap.at(pid)->value, target));

    // patchMain catches up only through the queue.
    engine.drainAudioToMainInto(engine.patchMain);
    REQUIRE(approxEq(engine.patchMain.paramMap.at(pid)->value, target));
}
