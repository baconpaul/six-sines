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
static constexpr int sineKey{105}; // 3520 Hz

// one steady sine at full level with flat envelopes, at 3520 Hz times 2^ratio
std::unique_ptr<Synth> makeSineSynth(int forced, float ratio)
{
    auto s = std::make_unique<Synth>(false);
    s->forcedSampleRateStrategy = forced;
    s->setSampleRate(hostRate);

    auto &p = s->patch;
    p.output.playMode.value = 0.f;
    p.output.polyLimit.value = 1.f;
    p.output.unisonCount.value = 1.f;
    p.output.ultrasonicFilter.value = 0.f;

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
    sn.ratio.value = ratio;
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
    s->voiceManager->processNoteOnEvent(0, 0, sineKey, -1, 1.0f, 0.f);

    // let the envelopes and the decimator settle before anything is captured
    for (int i = 0; i < 4096 / blockSize; ++i)
        s->process(nullptr);
    return s;
}

struct Capture
{
    double rms{0};
    int upCrossings{0};
};

Capture captureOneSecond(Synth &s)
{
    Capture c;
    double acc{0};
    size_t n{0};
    float prior{0};
    for (int b = 0; b < (int)(hostRate / blockSize); ++b)
    {
        s.process(nullptr);
        for (int i = 0; i < (int)blockSize; ++i)
        {
            auto v = s.output[0][i];
            acc += (double)v * v;
            if (prior <= 0 && v > 0)
                c.upCrossings++;
            prior = v;
            ++n;
        }
    }
    c.rms = std::sqrt(acc / n);
    return c;
}
} // namespace

TEST_CASE("Forced 2x and 1x run at an exact multiple of the host rate", "[engine-rate]")
{
    REQUIRE(makeSineSynth(SR_2X, 0.f)->engineSampleRate == 2 * hostRate);
    REQUIRE(makeSineSynth(SR_1X, 0.f)->engineSampleRate == hostRate);
}

TEST_CASE("The forced strategy wins over the patch", "[engine-rate]")
{
    auto s = makeSineSynth(SR_2X, 0.f);
    s->patch.output.sampleRateStrategy.value = (float)SR_220240;
    s->reapplyControlSettings();
    REQUIRE(s->engineSampleRate == 2 * hostRate);
}

TEST_CASE("A patch cannot select 2x or 1x without the build override", "[engine-rate]")
{
    for (auto v : {SR_2X, SR_1X})
    {
        auto s = makeSineSynth(-1, 0.f);
        s->patch.output.sampleRateStrategy.value = (float)v;
        s->reapplyControlSettings();
        REQUIRE(s->engineSampleRate == 120000.0);
    }
}

TEST_CASE("2x and 1x pass an in-band sine at pitch", "[engine-rate]")
{
    for (auto forced : {SR_2X, SR_1X})
    {
        INFO("strategy " << forced);
        auto c = captureOneSecond(*makeSineSynth(forced, 0.f));
        REQUIRE(c.rms > 0.05);
        REQUIRE(std::abs(c.upCrossings - 3520) <= 2);
    }
}

TEST_CASE("2x suppresses a tone above the host Nyquist", "[engine-rate]")
{
    auto inBand = captureOneSecond(*makeSineSynth(SR_2X, 0.f));
    auto ultrasonic = captureOneSecond(*makeSineSynth(SR_2X, std::log2(30000.f / 3520.f)));

    auto db = 20 * std::log10(ultrasonic.rms / inBand.rms);
    INFO("30 kHz tone at 2x comes through at " << db << " dB");
    REQUIRE(db < -60);
}
