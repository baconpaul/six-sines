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
 * The three tuning sources - MTS, 12-TET and a session SCL/KBM - and the one path a scale
 * takes from the main thread to the voices. Offsets are semitones away from 12-TET.
 */

#include "catch2/catch2.hpp"

#include <cmath>
#include <memory>
#include <vector>

#include "configuration.h"
#include "dsp/sintable.h"
#include "synth/patch.h"
#include "synth/synth.h"
#include "synth/tuning.h"
#include "synth/voice.h"

#include "Tunings.h"
#include "tinyxml/tinyxml.h"

using namespace baconpaul::six_sines;

namespace
{
static constexpr double hostRate{48000.0};

std::string edo(int n) { return Tunings::evenDivisionOfSpanByM(2, n).rawText; }

// one steady sine with flat envelopes, so two renders at the same pitch match sample for sample
std::unique_ptr<Synth> makeSineSynth()
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

    s->reapplyControlSettings();
    return s;
}

std::vector<float> render(Synth &s, int key, int blocks)
{
    s.voiceManager->processNoteOnEvent(0, 0, key, -1, 1.0f, 0.f);
    std::vector<float> res;
    for (int b = 0; b < blocks; ++b)
    {
        s.process(nullptr);
        for (int i = 0; i < (int)s.blockSize; ++i)
            res.push_back(s.output[0][i]);
    }
    return res;
}

// the way the editor sets it: by value through the queue, consumed on the next block
void setTuningMode(Synth &s, int mode)
{
    s.dawStateMain.audio.tuningMode = mode;
    Synth::MainToAudioMsg m{Synth::MainToAudioMsg::SET_AUDIO_DAW_STATE};
    m.audioDawState = s.dawStateMain.audio;
    s.mainToAudio.push(m);
}

float maxDiff(const std::vector<float> &a, const std::vector<float> &b)
{
    REQUIRE(a.size() == b.size());
    float r{0};
    for (size_t i = 0; i < a.size(); ++i)
        r = std::max(r, std::fabs(a[i] - b[i]));
    return r;
}
} // namespace

TEST_CASE("Tuning - a fresh session uses MTS and nothing retunes without a master", "[tuning]")
{
    auto s = makeSineSynth();
    REQUIRE(s->dawStateMain.audio.tuningMode == TM_MTS);

    s->process(nullptr);
    REQUIRE(s->monoValues.tuning.mode == TM_MTS);
    REQUIRE_FALSE(s->monoValues.tuning.retunes());
}

TEST_CASE("Tuning - twelve tone equal temperament is a no-op table", "[tuning]")
{
    RetuneTable t;
    std::string err;
    REQUIRE(buildRetuneTable(twelveTETSclText(), "", t, err));
    REQUIRE(t.valid);
    for (int k = 0; k < RetuneTable::numKeys; ++k)
        REQUIRE(t.semitones[k] == Approx(0.f).margin(1e-4));
}

TEST_CASE("Tuning - 19 EDO steps are 12/19 of a semitone from middle C", "[tuning]")
{
    RetuneTable t;
    std::string err;
    REQUIRE(buildRetuneTable(edo(19), "", t, err));
    REQUIRE(t.semitones[60] == Approx(0.f).margin(1e-4));
    for (int i = 1; i < 12; ++i)
        REQUIRE(t.semitones[60 + i] == Approx(i * 12.0 / 19.0 - i).margin(1e-4));
}

TEST_CASE("Tuning - a KBM reference frequency shifts the whole keyboard", "[tuning]")
{
    RetuneTable t;
    std::string err;
    REQUIRE(buildRetuneTable(twelveTETSclText(), Tunings::tuneA69To(432.0).rawText, t, err));
    auto expected = 12.0 * std::log2(432.0 / 440.0);
    for (auto k : {0, 24, 60, 69, 127})
        REQUIRE(t.semitones[k] == Approx(expected).margin(1e-4));
}

TEST_CASE("Tuning - the editor's seed mapping never moves the pitch", "[tuning]")
{
    for (const auto &scl :
         {twelveTETSclText(), edo(19), Tunings::evenDivisionOfSpanByM(3, 13).rawText})
    {
        RetuneTable seeded, bare;
        std::string err;
        REQUIRE(buildRetuneTable(scl, defaultKbmText(), seeded, err));
        REQUIRE(buildRetuneTable(scl, "", bare, err));
        for (int k = 0; k < RetuneTable::numKeys; ++k)
            REQUIRE(seeded.semitones[k] == Approx(bare.semitones[k]).margin(1e-4));
    }
}

TEST_CASE("Tuning - a bad scale reports, publishes nothing and changes nothing", "[tuning]")
{
    RetuneTable t;
    std::string err;
    REQUIRE(buildRetuneTable(edo(6), "", t, err));
    auto before = t;

    REQUIRE_FALSE(buildRetuneTable("this is definitely not a scale", "", t, err));
    REQUIRE_FALSE(err.empty());
    REQUIRE(t.semitones == before.semitones);

    TuningHandoff h;
    err.clear();
    REQUIRE_FALSE(publishSclKbm("not a scale either", "", h, err));
    REQUIRE_FALSE(err.empty());
    REQUIRE_FALSE(h.consume());
}

TEST_CASE("Tuning - the handoff hands over only the newest table, once", "[tuning]")
{
    TuningHandoff h;
    REQUIRE_FALSE(h.consume());

    std::string err;
    REQUIRE(publishSclKbm(edo(6), "", h, err));
    REQUIRE(publishSclKbm(edo(19), "", h, err));

    REQUIRE(h.consume());
    RetuneTable nineteen;
    REQUIRE(buildRetuneTable(edo(19), "", nineteen, err));
    REQUIRE(h.frontBuffer().semitones == nineteen.semitones);
    REQUIRE_FALSE(h.consume());

    // clearing is a publish too
    REQUIRE(publishSclKbm("", "", h, err));
    REQUIRE(h.consume());
    REQUIRE_FALSE(h.frontBuffer().valid);
}

TEST_CASE("Tuning - a mapping with no scale means 12-TET under that mapping", "[tuning]")
{
    TuningHandoff h;
    std::string err;
    REQUIRE(publishSclKbm("", Tunings::tuneA69To(432.0).rawText, h, err));
    REQUIRE(h.consume());
    REQUIRE(h.frontBuffer().valid);
    REQUIRE(h.frontBuffer().semitones[69] == Approx(12.0 * std::log2(432.0 / 440.0)).margin(1e-4));
}

TEST_CASE("Tuning - the session mode picks the source", "[tuning]")
{
    auto s = makeSineSynth();
    std::string err;
    REQUIRE(publishSclKbm(edo(6), "", s->tuningHandoff, err));

    auto &tuning = s->monoValues.tuning;

    setTuningMode(*s, TM_SCL_KBM);
    s->process(nullptr);
    REQUIRE(tuning.mode == TM_SCL_KBM);
    REQUIRE(tuning.retunes());
    // 6 EDO: every key is two semitones, so 66 sounds where 72 would
    REQUIRE(tuning.retuningInSemitones(66, 0) == Approx(6.f).margin(1e-4));

    // the scale stays loaded but 12-TET ignores it
    setTuningMode(*s, TM_12TET);
    s->process(nullptr);
    REQUIRE_FALSE(tuning.retunes());
    REQUIRE(tuning.retuningInSemitones(66, 0) == 0.f);

    // and SCL/KBM with no scale behind it is plain 12-TET, not silence or garbage
    REQUIRE(publishSclKbm("", "", s->tuningHandoff, err));
    setTuningMode(*s, TM_SCL_KBM);
    s->process(nullptr);
    REQUIRE_FALSE(tuning.retunes());
    REQUIRE(tuning.retuningInSemitones(66, 0) == 0.f);
}

TEST_CASE("Tuning - a voice under 6 EDO sounds the 12-TET key the scale points at", "[tuning]")
{
    static constexpr int blocks{1024};

    auto reference = makeSineSynth();
    auto expected = render(*reference, 72, blocks);

    auto tuned = makeSineSynth();
    std::string err;
    REQUIRE(publishSclKbm(edo(6), "", tuned->tuningHandoff, err));
    setTuningMode(*tuned, TM_SCL_KBM);
    auto got = render(*tuned, 66, blocks);

    auto untuned = makeSineSynth();
    auto plain66 = render(*untuned, 66, blocks);

    REQUIRE(maxDiff(expected, got) < 1e-4f);
    REQUIRE(maxDiff(expected, plain66) > 1e-2f);
}

TEST_CASE("Tuning - the scale and mapping round-trip through DAW extra state", "[tuning]")
{
    auto scl = edo(6);
    auto kbm = Tunings::tuneA69To(432.0).rawText;

    Synth::DawStateMain saved{};
    saved.main.sclText = scl;
    saved.main.kbmText = kbm;
    saved.audio.tuningMode = TM_SCL_KBM;
    TiXmlElement el("dawExtraState");
    Synth::toDawExtraState(el, saved);

    Synth::DawStateMain restored{};
    Synth::fromDawExtraState(el, restored);
    REQUIRE(restored.main.sclText == scl);
    REQUIRE(restored.main.kbmText == kbm);
    REQUIRE(restored.audio.tuningMode == TM_SCL_KBM);

    // and a session without them carries no tuning
    Synth::DawStateMain none{};
    TiXmlElement el2("dawExtraState");
    Synth::toDawExtraState(el2, none);
    REQUIRE(el2.FirstChildElement("scl") == nullptr);
    Synth::DawStateMain restoredNone{};
    restoredNone.main.sclText = "stale";
    Synth::fromDawExtraState(el2, restoredNone);
    REQUIRE(restoredNone.main.sclText.empty());
    REQUIRE(restoredNone.main.kbmText.empty());
    REQUIRE(restoredNone.audio.tuningMode == TM_MTS);
}

TEST_CASE("Tuning - a session stream carries the mode and scale, a patch carries neither",
          "[tuning]")
{
    auto a = std::make_unique<Synth>(false);
    a->dawStateMain.main.sclText = edo(6);
    a->dawStateMain.audio.tuningMode = TM_SCL_KBM;

    auto b = std::make_unique<Synth>(false);
    REQUIRE(b->patchMain.fromState(a->patchMain.toState(/*withDawExtraState*/ true)));
    REQUIRE(b->dawStateMain.main.sclText == edo(6));
    REQUIRE(b->dawStateMain.audio.tuningMode == TM_SCL_KBM);

    auto c = std::make_unique<Synth>(false);
    REQUIRE(c->patchMain.fromState(a->patchMain.toState(/*withDawExtraState*/ false)));
    REQUIRE(c->dawStateMain.main.sclText.empty());
    REQUIRE(c->dawStateMain.audio.tuningMode == TM_MTS);
}

TEST_CASE("Tuning - loading a patch leaves the session tuning alone", "[tuning]")
{
    auto s = makeSineSynth();
    std::string err;
    s->dawStateMain.main.sclText = edo(6);
    REQUIRE(publishSclKbm(edo(6), "", s->tuningHandoff, err));
    setTuningMode(*s, TM_SCL_KBM);
    s->process(nullptr);

    // a preset file is a patch stream with no dawExtraState
    auto other = std::make_unique<Synth>(false);
    REQUIRE(s->patchMain.fromState(other->patchMain.toState(/*withDawExtraState*/ false)));
    for (const auto *p : s->patchMain.params)
        s->mainToAudio.push(
            {Synth::MainToAudioMsg::SET_PARAM_WITHOUT_NOTIFYING, p->meta.id, p->value});
    s->mainToAudio.push({Synth::MainToAudioMsg::SEND_POST_LOAD});
    s->process(nullptr);

    REQUIRE(s->dawStateMain.audio.tuningMode == TM_SCL_KBM);
    REQUIRE(s->dawStateMain.main.sclText == edo(6));
    REQUIRE(s->monoValues.tuning.mode == TM_SCL_KBM);
    REQUIRE(s->monoValues.tuning.retuningInSemitones(66, 0) == Approx(6.f).margin(1e-4));
}

TEST_CASE("Tuning - the status name comes from the scale description", "[tuning]")
{
    REQUIRE(sclKbmDisplayName("", "").empty());
    REQUIRE(sclKbmDisplayName("", defaultKbmText()) == "12-TET (KBM)");
    REQUIRE(sclKbmDisplayName(edo(19), "") == Tunings::parseSCLData(edo(19)).description);
    REQUIRE(sclKbmDisplayName("garbage", "") == "Invalid scale");
}
