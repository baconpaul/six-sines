/*
 * The phase-map read phase is a post-remap rotation of the underlying waveform:
 * out = wave(remap(phase) + theta), latched at attack. These tests pin the two
 * properties that make it safe to ship — old patches read it as zero, and the
 * rotation lands after the remap rather than before it.
 */

#include "catch2/catch2.hpp"

#include "configuration.h"
#include "synth/patch.h"
#include "synth/synth.h"
#include "synth/voice.h"
#include "dsp/sintable.h"

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace baconpaul::six_sines;

namespace
{
// Remove every <p id="..." .../> element naming one of `ids`, leaving a stream
// shaped like one written before the param existed.
std::string stripParams(std::string xml, const std::vector<uint32_t> &ids)
{
    for (auto id : ids)
    {
        auto needle = "<p id=\"" + std::to_string(id) + "\"";
        auto at = xml.find(needle);
        REQUIRE(at != std::string::npos);
        auto end = xml.find("/>", at);
        REQUIRE(end != std::string::npos);
        xml.erase(at, end + 2 - at);
    }
    return xml;
}

// One op, PHASE_REMAP/SAW over a sine, rendered raw off the OpSource so the
// mixer and output stage can't colour the comparison.
std::array<float, blockSize> renderPhaseRemapOp(float readPhase)
{
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto &p = s->patch;
    p.output.playMode.value = 0.f;
    p.output.polyLimit.value = 1.f;
    p.output.unisonCount.value = 1.f; // unison > 1 randomizes phase per voice

    auto &sn = p.sourceNodes[0];
    sn.active.value = 1.f;
    sn.waveForm.value = (float)SinTable::SIN;
    sn.ratio.value = 0.f;
    sn.startingPhase.value = 0.f;
    sn.keyTrack.value = 1.f;
    sn.extendedModeMode.value = (float)Patch::SourceNode::ExtendedMode::PHASE_REMAP;
    sn.phaseMapModeShape.value = (float)Patch::SourceNode::PhaseMapShape::SAW;
    sn.extendedModeM.value = 0.5f;
    sn.envToExtendedModeM.value = 0.f;
    sn.lfoToExtendedModeM.value = 0.f;
    sn.phaseMapReadPhase.value = readPhase;

    s->reapplyControlSettings();
    s->voiceManager->processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);

    auto *v = s->head;
    REQUIRE(v != nullptr);
    auto *op = &v->src[0];
    op->zeroInputs();
    op->renderBlock();

    std::array<float, blockSize> out{};
    for (int i = 0; i < blockSize; ++i)
        out[i] = op->output[i];
    return out;
}
} // namespace

TEST_CASE("Phase map read phase streaming", "[phase-map]")
{
    MatrixIndex::initialize();

    SECTION("a stream written without the param reads back as zero")
    {
        auto written = std::make_unique<Patch>();
        std::vector<uint32_t> ids;
        for (int i = 0; i < (int)numOps; ++i)
        {
            written->sourceNodes[i].phaseMapReadPhase.value = 0.6f + 0.01f * i;
            ids.push_back(written->sourceNodes[i].phaseMapReadPhase.meta.id);
        }
        auto legacy = stripParams(written->toState(), ids);

        auto read = std::make_unique<Patch>();
        // dirty the target first so a zero can only come from resetToInit, not from
        // the object happening to be fresh
        for (int i = 0; i < (int)numOps; ++i)
            read->sourceNodes[i].phaseMapReadPhase.value = 0.75f;

        REQUIRE(read->fromState(legacy));
        for (int i = 0; i < (int)numOps; ++i)
            REQUIRE(read->sourceNodes[i].phaseMapReadPhase.value == 0.f);
    }

    SECTION("a set value round-trips")
    {
        auto written = std::make_unique<Patch>();
        written->sourceNodes[2].phaseMapReadPhase.value = 0.25f;

        auto read = std::make_unique<Patch>();
        REQUIRE(read->fromState(written->toState()));
        REQUIRE(read->sourceNodes[2].phaseMapReadPhase.value == Approx(0.25f));
        REQUIRE(read->sourceNodes[0].phaseMapReadPhase.value == 0.f);
    }

    SECTION("the param defaults to zero so existing phase maps are unchanged")
    {
        auto p = std::make_unique<Patch>();
        for (int i = 0; i < (int)numOps; ++i)
            REQUIRE(p->sourceNodes[i].phaseMapReadPhase.meta.defaultVal == 0.f);
    }
}

TEST_CASE("Phase map read phase rotates the read", "[phase-map]")
{
    auto base = renderPhaseRemapOp(0.f);

    SECTION("a quarter cycle changes the output")
    {
        auto rotated = renderPhaseRemapOp(0.25f);
        bool anyDiff{false};
        for (int i = 0; i < blockSize; ++i)
            anyDiff = anyDiff || std::fabs(rotated[i] - base[i]) > 1e-4f;
        REQUIRE(anyDiff);
    }

    SECTION("a half cycle negates a sine underlyer")
    {
        // sin(x + pi) == -sin(x), and this only holds if the offset is added
        // *after* the remap — remap(ph + half) is not remap(ph) + half.
        auto flipped = renderPhaseRemapOp(0.5f);
        for (int i = 0; i < blockSize; ++i)
            REQUIRE(flipped[i] == Approx(-base[i]).margin(1e-6));
    }

    SECTION("a full cycle wraps back to no offset")
    {
        auto wrapped = renderPhaseRemapOp(1.f);
        for (int i = 0; i < blockSize; ++i)
            REQUIRE(wrapped[i] == Approx(base[i]).margin(1e-6));
    }
}
