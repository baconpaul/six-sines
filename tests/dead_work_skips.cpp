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
 * Work the voice can skip without changing the sound: self feedback that contributes nothing,
 * operators whose output nobody reads, and envelopes holding a settled sustain.
 */

#include "catch2/catch2.hpp"

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "configuration.h"
#include "dsp/sintable.h"
#include "synth/matrix_index.h"
#include "synth/patch.h"
#include "synth/synth.h"
#include "synth/voice.h"

using namespace baconpaul::six_sines;

namespace
{
static constexpr double hostRate{48000.0};

// op 0 audible at full level, everything else off, flat envelopes so nothing decays
std::unique_ptr<Synth> makeSynth(const std::function<void(Patch &)> &configure)
{
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(hostRate);

    auto &p = s->patch;
    p.output.playMode.value = 0.f;
    p.output.polyLimit.value = 1.f;
    p.output.unisonCount.value = 1.f;
    p.output.velSensitivity.value = 0.f;

    auto flatEnv = [](auto &n)
    {
        n.delay.value = 0.f;
        n.attack.value = 0.f;
        n.hold.value = 0.f;
        n.decay.value = 0.f;
        n.sustain.value = 1.f;
        n.release.value = 0.f;
    };
    flatEnv(p.output);
    for (int i = 0; i < (int)numOps; ++i)
    {
        flatEnv(p.sourceNodes[i]);
        flatEnv(p.mixerNodes[i]);
        flatEnv(p.selfNodes[i]);
        p.sourceNodes[i].active.value = (i == 0) ? 1.f : 0.f;
        p.sourceNodes[i].waveForm.value = (float)SinTable::SIN;
        p.mixerNodes[i].active.value = (i == 0) ? 1.f : 0.f;
        p.mixerNodes[i].level.value = 1.f;
        p.selfNodes[i].active.value = 0.f;
    }
    for (auto &m : p.matrixNodes)
    {
        flatEnv(m);
        m.active.value = 0.f;
    }

    configure(p);

    s->reapplyControlSettings();
    s->voiceManager->processNoteOnEvent(0, 0, 57, -1, 1.0f, 0.f);
    return s;
}

std::vector<float> render(Synth &s, int blocks)
{
    std::vector<float> res;
    for (int b = 0; b < blocks; ++b)
    {
        s.process(nullptr);
        for (int i = 0; i < (int)blockSize; ++i)
            res.push_back(s.output[0][i]);
    }
    return res;
}

Voice &firstVoice(Synth &s)
{
    REQUIRE(s.head != nullptr);
    return *s.head;
}
} // namespace

TEST_CASE("Self feedback at level zero takes the no feedback path", "[dead-work]")
{
    auto s = makeSynth(
        [](Patch &p)
        {
            p.selfNodes[0].active.value = 1.f;
            p.selfNodes[0].fbLevel.value = 0.f;
        });
    render(*s, 64);
    REQUIRE_FALSE(firstVoice(*s).src[0].hasActiveFeedback);
}

TEST_CASE("Self feedback at level zero sounds the same as feedback off", "[dead-work]")
{
    auto zero = makeSynth(
        [](Patch &p)
        {
            p.selfNodes[0].active.value = 1.f;
            p.selfNodes[0].fbLevel.value = 0.f;
        });
    auto off = makeSynth([](Patch &) {});
    REQUIRE(render(*zero, 512) == render(*off, 512));
}

TEST_CASE("Raising self feedback from zero mid note engages feedback", "[dead-work]")
{
    auto s = makeSynth(
        [](Patch &p)
        {
            p.selfNodes[0].active.value = 1.f;
            p.selfNodes[0].fbLevel.value = 0.f;
        });
    auto off = makeSynth([](Patch &) {});
    render(*s, 64);
    render(*off, 64);

    s->patch.selfNodes[0].fbLevel.value = 0.5f;
    auto withFb = render(*s, 64);
    REQUIRE(firstVoice(*s).src[0].hasActiveFeedback);
    REQUIRE(withFb != render(*off, 64));
}

namespace
{
bool allZero(const float *f)
{
    for (int i = 0; i < (int)blockSize; ++i)
        if (f[i] != 0.f)
            return false;
    return true;
}

void route(Patch &p, int from, int to)
{
    p.matrixNodes[MatrixIndex::positionForSourceTarget(from, to)].active.value = 1.f;
}
} // namespace

TEST_CASE("An operator nobody hears or modulates with is not rendered", "[dead-work]")
{
    auto s = makeSynth([](Patch &p) { p.sourceNodes[1].active.value = 1.f; });
    render(*s, 16);
    REQUIRE(allZero(firstVoice(*s).src[1].output));
}

TEST_CASE("An operator feeding only an unheard operator is not rendered", "[dead-work]")
{
    auto s = makeSynth(
        [](Patch &p)
        {
            p.sourceNodes[1].active.value = 1.f;
            p.sourceNodes[2].active.value = 1.f;
            route(p, 1, 2);
        });
    render(*s, 16);
    REQUIRE(allZero(firstVoice(*s).src[1].output));
    REQUIRE(allZero(firstVoice(*s).src[2].output));
}

TEST_CASE("An operator modulating a heard operator is still rendered", "[dead-work]")
{
    auto s = makeSynth(
        [](Patch &p)
        {
            // op 1 heard, op 0 only modulates it
            p.sourceNodes[1].active.value = 1.f;
            p.mixerNodes[1].active.value = 1.f;
            p.mixerNodes[0].active.value = 0.f;
            route(p, 0, 1);
        });
    render(*s, 16);
    REQUIRE_FALSE(allZero(firstVoice(*s).src[0].output));
}

TEST_CASE("An unheard operator does not change the sound", "[dead-work]")
{
    auto unheard = makeSynth([](Patch &p) { p.sourceNodes[1].active.value = 1.f; });
    auto off = makeSynth([](Patch &) {});
    REQUIRE(render(*unheard, 512) == render(*off, 512));
}

namespace
{
std::unique_ptr<Synth> makeSustainSynth()
{
    return makeSynth(
        [](Patch &p)
        {
            p.mixerNodes[0].sustain.value = 0.6f;
            p.mixerNodes[0].release.value = 0.3f;
            p.output.release.value = 0.5f;
        });
}

bool envIs(const Voice &v, float x)
{
    for (int i = 0; i < (int)blockSize; ++i)
        if (v.mixerNode[0].env.outputCache[i] != x)
            return false;
    return true;
}
} // namespace

TEST_CASE("A held sustain follows a sustain change mid note", "[dead-work]")
{
    auto s = makeSustainSynth();
    render(*s, 64);
    REQUIRE(envIs(firstVoice(*s), 0.6f));

    s->patch.mixerNodes[0].sustain.value = 0.3f;
    render(*s, 64);
    REQUIRE(envIs(firstVoice(*s), 0.3f));
}

TEST_CASE("A held sustain still releases", "[dead-work]")
{
    auto s = makeSustainSynth();
    render(*s, 64);
    REQUIRE(envIs(firstVoice(*s), 0.6f));

    s->voiceManager->processNoteOffEvent(0, 0, 57, -1, 0.f);
    render(*s, 16);
    REQUIRE(firstVoice(*s).mixerNode[0].env.outputCache[blockSize - 1] < 0.6f);
}

namespace
{
// left over right energy of a rendered stretch
double leftOverRight(Synth &s, int blocks)
{
    double l{0}, r{0};
    for (int b = 0; b < blocks; ++b)
    {
        s.process(nullptr);
        for (int i = 0; i < (int)blockSize; ++i)
        {
            l += s.output[0][i] * s.output[0][i];
            r += s.output[1][i] * s.output[1][i];
        }
    }
    return l / r;
}
} // namespace

TEST_CASE("A mixer pan change mid note moves the sound", "[dead-work]")
{
    auto s = makeSynth([](Patch &p) { p.mixerNodes[0].pan.value = 0.6f; });
    render(*s, 64);
    REQUIRE(leftOverRight(*s, 256) < 0.5);

    s->patch.mixerNodes[0].pan.value = -0.6f;
    render(*s, 64);
    REQUIRE(leftOverRight(*s, 256) > 2.0);
}

TEST_CASE("An output pan change mid note moves the sound", "[dead-work]")
{
    auto s = makeSynth([](Patch &p) { p.output.pan.value = 0.6f; });
    render(*s, 64);
    REQUIRE(leftOverRight(*s, 256) < 0.5);

    s->patch.output.pan.value = -0.6f;
    render(*s, 64);
    REQUIRE(leftOverRight(*s, 256) > 2.0);
}
