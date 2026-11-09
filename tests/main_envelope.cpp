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
#include <memory>

#include "configuration.h"
#include "dsp/sintable.h"
#include "synth/patch.h"
#include "synth/synth.h"

using namespace baconpaul::six_sines;

namespace
{
static constexpr double hostRate{48000.0};

// every envelope is zero time up to sustain, then the longest release
std::unique_ptr<Synth> makeLongReleaseSynth()
{
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(hostRate);

    auto &p = s->patch;
    p.output.playMode.value = 0.f;
    p.output.unisonCount.value = 1.f;

    auto flatEnv = [](auto &n)
    {
        n.delay.value = 0.f;
        n.attack.value = 0.f;
        n.hold.value = 0.f;
        n.decay.value = 0.f;
        n.sustain.value = 1.f;
        n.release.value = 1.f;
    };

    auto &sn = p.sourceNodes[0];
    sn.active.value = 1.f;
    sn.waveForm.value = (float)SinTable::SIN;
    sn.ratio.value = 0.f;
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

double rmsOverSeconds(Synth &s, double seconds)
{
    double acc{0};
    size_t n{0};
    auto samples = (int)(seconds * hostRate);
    while ((int)n < samples)
    {
        s.process(nullptr);
        for (size_t i = 0; i < s.blockSize; ++i)
        {
            acc += (double)s.output[0][i] * s.output[0][i];
            ++n;
        }
    }
    return std::sqrt(acc / n);
}
} // namespace

TEST_CASE("A main envelope with no attack and the longest release plays and releases", "[main-env]")
{
    auto s = makeLongReleaseSynth();
    s->voiceManager->processNoteOnEvent(0, 0, 57, -1, 1.0f, 0.f);

    auto held = rmsOverSeconds(*s, 0.2);
    REQUIRE(s->head);
    REQUIRE(held > 0.05);

    s->voiceManager->processNoteOffEvent(0, 0, 57, -1, 0.f);
    auto released = rmsOverSeconds(*s, 0.2);
    REQUIRE(s->head);
    REQUIRE(released > 0.5 * held);
    REQUIRE(released < held);
}
