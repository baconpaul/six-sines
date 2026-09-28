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

/*
 * Before patch version 14 the output node never cleared its stereo bus, so every block
 * accumulated onto the previous one: a feedback comb with an 8 sample delay at the engine
 * rate and gain 0.15 * level^3 * env. Version 14 clears the bus and keeps the old behaviour
 * behind a per patch flag that migration turns on for older streams.
 */

#include "catch2/catch2.hpp"

#include <cmath>
#include <memory>
#include <regex>
#include <string>

#include "configuration.h"
#include "dsp/sintable.h"
#include "synth/patch.h"
#include "synth/synth.h"
#include "synth/voice.h"

using namespace baconpaul::six_sines;

namespace
{
static constexpr double hostRate{48000.0};

// a bare Patch needs the engine tables a Synth initialises
std::unique_ptr<Patch> makePatch()
{
    static auto engine = std::make_unique<Synth>(false);
    return std::make_unique<Patch>();
}

// the stream an older build would have written: version 13 and no legacy flag param
std::string asVersion13(std::string s)
{
    s = std::regex_replace(s, std::regex("version=\"14\""), "version=\"13\"");
    s = std::regex_replace(s, std::regex("<p id=\"559\" v=\"[^\"]*\" />"), "");
    return s;
}

// one steady sine at full level with flat envelopes, key 45 so the comb phase is negligible
std::unique_ptr<Synth> makeSineSynth(bool legacyFeedback)
{
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(hostRate);

    auto &p = s->patch;
    p.output.playMode.value = 0.f;
    p.output.polyLimit.value = 1.f;
    p.output.unisonCount.value = 1.f;

    auto flatEnv = [](auto &n)
    {
        n.delay.value = 0.f;
        n.attack.value = 0.f;
        n.hold.value = 0.f;
        n.decay.value = 0.f;
        n.sustain.value = 1.f;
        n.release.value = 0.f;
    };

    auto &sn = p.sourceNodes[0];
    sn.active.value = 1.f;
    sn.waveForm.value = (float)SinTable::SIN;
    sn.ratio.value = 0.f;
    sn.startingPhase.value = 0.f;
    sn.keyTrack.value = 1.f;
    flatEnv(sn);

    flatEnv(p.mixerNodes[0]);
    p.mixerNodes[0].level.value = 1.f;
    p.mixerNodes[0].pan.value = 0.f;

    flatEnv(p.output);
    p.output.level.value = 1.f;
    p.output.velSensitivity.value = 0.f;
    p.output.legacyDsp.value = legacyFeedback ? 1.f : 0.f;

    s->reapplyControlSettings();
    s->voiceManager->processNoteOnEvent(0, 0, 45, -1, 1.0f, 0.f);

    // let the envelopes and the resampler settle before anything is captured
    for (int i = 0; i < 4096 / blockSize; ++i)
        s->process(nullptr);
    return s;
}

double rmsOverSeconds(Synth &s, double seconds)
{
    double acc{0};
    size_t n{0};
    auto blocks = (int)(seconds * hostRate / blockSize);
    for (int b = 0; b < blocks; ++b)
    {
        s.process(nullptr);
        for (int i = 0; i < (int)blockSize; ++i)
        {
            acc += (double)s.output[0][i] * s.output[0][i];
            ++n;
        }
    }
    return std::sqrt(acc / n);
}
} // namespace

TEST_CASE("Output bus clears each block by default", "[output-bus]")
{
    auto p = makePatch();
    REQUIRE(p->output.legacyDsp.value == 0.f);
}

TEST_CASE("Streams from version 13 and earlier keep the legacy output feedback", "[output-bus]")
{
    auto p = makePatch();
    auto v13 = asVersion13(p->toState());
    REQUIRE(v13.find("version=\"13\"") != std::string::npos);
    REQUIRE(v13.find("id=\"559\"") == std::string::npos);

    auto q = makePatch();
    REQUIRE(q->fromState(v13));
    REQUIRE(q->output.legacyDsp.value == 1.f);
}

TEST_CASE("A version 14 stream round trips the legacy output flag", "[output-bus]")
{
    for (float v : {0.f, 1.f})
    {
        auto p = makePatch();
        p->output.legacyDsp.value = v;
        auto q = makePatch();
        REQUIRE(q->fromState(p->toState()));
        REQUIRE(q->output.legacyDsp.value == v);
    }
}

TEST_CASE("Legacy output feedback raises a low sine by the comb gain", "[output-bus]")
{
    auto clean = rmsOverSeconds(*makeSineSynth(false), 1.0);
    auto legacy = rmsOverSeconds(*makeSineSynth(true), 1.0);
    REQUIRE(clean > 1e-3);

    // 1 / (1 - g) with g = 0.15 * level^3 * env = 0.15
    REQUIRE(legacy / clean == Approx(1.0 / 0.85).margin(0.01));
}

namespace
{
std::unique_ptr<Synth> makeSquarishSynth(bool legacy)
{
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(hostRate);
    auto &p = s->patch;
    p.output.playMode.value = 0.f;
    p.output.polyLimit.value = 4.f;
    p.output.unisonCount.value = 1.f;
    p.output.legacyDsp.value = legacy ? 1.f : 0.f;
    p.sourceNodes[0].active.value = 1.f;
    p.sourceNodes[0].waveForm.value = (float)SinTable::SQUARISH;
    p.mixerNodes[0].active.value = 1.f;
    s->reapplyControlSettings();
    return s;
}

const SIMD_M128 *tableForKey(Synth &s, int key)
{
    for (auto *v = s.head; v; v = v->next)
        if (v->voiceValues.key == key)
            return v->src[0].st.simdQuad;
    return nullptr;
}
} // namespace

TEST_CASE("The 1.2 dsp flag reads squarish from the 1.2 table", "[output-bus]")
{
    for (bool legacy : {false, true})
    {
        auto s = makeSquarishSynth(legacy);
        s->voiceManager->processNoteOnEvent(0, 0, 60, -1, 1.0f, 0.f);
        s->process(nullptr);
        INFO("legacy " << legacy);
        REQUIRE(tableForKey(*s, 60) == SinTable::quadTable(SinTable::SQUARISH, legacy));
    }
}

TEST_CASE("A held note keeps its tables when the 1.2 dsp flag changes", "[output-bus]")
{
    auto s = makeSquarishSynth(true);
    s->voiceManager->processNoteOnEvent(0, 0, 60, -1, 1.0f, 0.f);
    s->process(nullptr);

    s->patch.output.legacyDsp.value = 0.f;
    s->reapplyControlSettings();
    s->voiceManager->processNoteOnEvent(0, 0, 64, -1, 1.0f, 0.f);
    s->process(nullptr);

    REQUIRE(tableForKey(*s, 60) == SinTable::quadTable(SinTable::SQUARISH, true));
    REQUIRE(tableForKey(*s, 64) == SinTable::quadTable(SinTable::SQUARISH, false));
}
