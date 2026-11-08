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

#include <memory>

#include "configuration.h"
#include "dsp/node_support.h"
#include "dsp/sintable.h"
#include "synth/patch.h"
#include "synth/synth.h"
#include "synth/voice.h"

using namespace baconpaul::six_sines;

namespace
{
// a mono patch whose op 1 envelope retriggers on key press
std::unique_ptr<Synth> makeMonoRetriggerSynth()
{
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto &p = s->patch;
    p.output.playMode.value = 1.f;
    p.output.unisonCount.value = 1.f;
    p.output.velSensitivity.value = 0.f;

    auto &sn = p.sourceNodes[0];
    sn.active.value = 1.f;
    sn.waveForm.value = (float)SinTable::SIN;
    sn.triggerMode.value = (float)TriggerMode::KEY_PRESS;
    sn.attack.value = 0.3f;
    sn.sustain.value = 1.f;
    p.mixerNodes[0].level.value = 1.f;

    s->reapplyControlSettings();
    return s;
}

void playChord(Synth &s, int root)
{
    for (int k : {0, 4, 7, 12})
        s.voiceManager->processNoteOnEvent(0, 0, root + k, -1, 1.0f, 0.f);
}
} // namespace

TEST_CASE("A chord on a fresh mono voice retriggers from silence", "[envelope]")
{
    // the voices were built with the default patch, whose constant envelopes leave 1 in the cache
    auto s = makeMonoRetriggerSynth();
    playChord(*s, 48);
    s->process(nullptr);
    REQUIRE(s->head);
    REQUIRE(s->head->src[0].env.outputCache[0] < 0.1f);
}

TEST_CASE("A chord on a reused mono voice retriggers from silence", "[envelope]")
{
    auto s = makeMonoRetriggerSynth();
    s->voiceManager->processNoteOnEvent(0, 0, 60, -1, 1.0f, 0.f);
    for (int i = 0; i < 4000; ++i)
        s->process(nullptr);
    REQUIRE(s->head);
    auto *held = s->head;
    REQUIRE(held->src[0].env.outputCache[blockSize - 1] > 0.9f);

    // end the note at full level, then land a chord on the same voice
    s->voiceManager->allSoundsOff();
    for (int i = 0; i < 100; ++i)
        s->process(nullptr);
    REQUIRE(s->head == nullptr);

    playChord(*s, 48);
    s->process(nullptr);
    REQUIRE(s->head == held);
    REQUIRE(s->head->src[0].env.outputCache[0] < 0.1f);
}
