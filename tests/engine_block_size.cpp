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
#include <set>

#include "configuration.h"
#include "dsp/sintable.h"
#include "synth/patch.h"
#include "synth/synth.h"
#include "synth/voice.h"

using namespace baconpaul::six_sines;

namespace
{
static constexpr double hostRate{48000.0};

void setBlockSize(Synth &s, EngineBlockSize b)
{
    s.mainToAudio.push({Synth::MainToAudioMsg::SET_PARAM_WITHOUT_NOTIFYING,
                        s.patch.output.engineBlockSize.meta.id, (float)b});
}

// one steady sine with flat envelopes
std::unique_ptr<Synth> makeSineSynth(EngineBlockSize b)
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
        n.release.value = 0.f;
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
    setBlockSize(*s, b);
    s->process(nullptr);
    return s;
}

void playSeconds(Synth &s, double seconds)
{
    auto samples = (int)(seconds * hostRate);
    for (int done = 0; done < samples; done += (int)s.blockSize)
        s.process(nullptr);
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

// held voices fade out at the old size before the switch lands
int processUntilBlockSize(Synth &s, size_t bs)
{
    int blocks{0};
    while (s.blockSize != bs && blocks < 1000)
    {
        s.process(nullptr);
        ++blocks;
    }
    return blocks;
}
} // namespace

TEST_CASE("Engine block size defaults to 8", "[block-size]")
{
    auto s = std::make_unique<Synth>(false);
    REQUIRE(s->patchMain.output.engineBlockSize.value == (float)EBS_8);
    REQUIRE(s->blockSize == 8);
}

TEST_CASE("Each block size plays a steady sine at the same level", "[block-size]")
{
    double rms[3]{};
    for (auto b : {EBS_8, EBS_16, EBS_32})
    {
        INFO("block size setting " << (int)b);
        auto s = makeSineSynth(b);
        REQUIRE(s->blockSize == (size_t)(8 << (int)b));
        s->voiceManager->processNoteOnEvent(0, 0, 57, -1, 1.0f, 0.f);
        playSeconds(*s, 0.1);
        REQUIRE(s->head);
        rms[b] = rmsOverSeconds(*s, 0.5);
        REQUIRE(rms[b] > 0.1);
    }
    REQUIRE(20 * std::log10(rms[EBS_16] / rms[EBS_8]) == Approx(0).margin(0.05));
    REQUIRE(20 * std::log10(rms[EBS_32] / rms[EBS_8]) == Approx(0).margin(0.05));
}

TEST_CASE("Changing block size ends held voices and the next note plays", "[block-size]")
{
    auto s = makeSineSynth(EBS_8);
    s->voiceManager->processNoteOnEvent(0, 0, 57, -1, 1.0f, 0.f);
    playSeconds(*s, 0.05);
    REQUIRE(s->head);
    REQUIRE(s->voiceManager->getVoiceCount() == 1);

    setBlockSize(*s, EBS_32);
    auto blocks = processUntilBlockSize(*s, 32);
    REQUIRE(blocks > 1);
    REQUIRE(blocks <= 256 / 8 + 2);
    REQUIRE(s->blockSize == 32);
    REQUIRE(s->head == nullptr);
    REQUIRE(s->voiceCount == 0);
    REQUIRE(s->voiceManager->getVoiceCount() == 0);

    // the old note's release finds nothing to end
    s->voiceManager->processNoteOffEvent(0, 0, 57, -1, 0.f);
    s->process(nullptr);

    s->voiceManager->processNoteOnEvent(0, 0, 60, -1, 1.0f, 0.f);
    playSeconds(*s, 0.05);
    REQUIRE(s->headAs<32>());
    REQUIRE(s->voiceManager->getVoiceCount() == 1);
    REQUIRE(rmsOverSeconds(*s, 0.1) > 0.1);

    setBlockSize(*s, EBS_8);
    blocks = processUntilBlockSize(*s, 8);
    REQUIRE(blocks > 1);
    REQUIRE(blocks <= 256 / 32 + 2);
    REQUIRE(s->blockSize == 8);
    REQUIRE(s->head == nullptr);
    REQUIRE(s->voiceManager->getVoiceCount() == 0);
}

TEST_CASE("A block size switch reuses voices built up front", "[block-size]")
{
    // building a pool takes milliseconds, which on the audio thread is a dropout
    auto s = makeSineSynth(EBS_8);
    auto first8 = s->voices;
    std::set<Synth::voice_t *> seen(first8.begin(), first8.end());

    for (auto [b, bs] : {std::pair{EBS_16, 16}, std::pair{EBS_32, 32}})
    {
        setBlockSize(*s, b);
        processUntilBlockSize(*s, bs);
        for (auto *v : s->voices)
            REQUIRE(seen.insert(v).second);
    }

    setBlockSize(*s, EBS_8);
    processUntilBlockSize(*s, 8);
    REQUIRE(s->voices == first8);
}
