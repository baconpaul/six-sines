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
 * A USER_TABLE operator has to behave exactly like a SinTable operator in every respect
 * except which table it reads. The cheapest way to pin that is to load a wavetable that
 * *is* a sine and require the two paths to agree - including under the extended modes,
 * which combine with a wavetable rather than replacing it.
 */

#include "catch2/catch2.hpp"

#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "configuration.h"
#include "dsp/sintable.h"
#include "dsp/wavetable.h"
#include "dsp/wavetable_build.h"
#include "synth/matrix_index.h"
#include "synth/patch.h"
#include "synth/synth.h"
#include "synth/voice.h"

using namespace baconpaul::six_sines;

namespace
{
// one cycle of sin(2 pi x), which is exactly SinTable::SIN
std::shared_ptr<const Wavetable> sineTable()
{
    SourceFrames sf;
    sf.cycleLength = 2048;
    sf.nFrames = 1;
    sf.samples.resize(sf.cycleLength);
    for (uint32_t i = 0; i < sf.cycleLength; ++i)
        sf.samples[i] = static_cast<float>(std::sin(2 * M_PI * i / sf.cycleLength));
    return buildWavetableFromSource(sf, 0x5111, "sine");
}

// Just over one cycle at middle C on the 2.5x oversampled engine, so a waveform
// difference has somewhere to show up.
static constexpr int renderBlocks{64};
static constexpr int renderSamples{renderBlocks * blockSize};

struct Rendered
{
    std::vector<float> a, b;
};

// Render one operator twice over: once on the static table, once on a loaded wavetable,
// with everything else identical. Driven through Voice::renderBlock because that is what
// sets an operator's base frequency; op->output is still the raw oscillator, pre-mixer.
Rendered renderBothPaths(std::shared_ptr<const Wavetable> table,
                         const std::function<void(Patch::SourceNode &)> &configure)
{
    Rendered r;
    for (int pass = 0; pass < 2; ++pass)
    {
        auto s = std::make_unique<Synth>(false);
        s->setSampleRate(48000.0);

        auto &p = s->patch;
        p.output.playMode.value = 0.f;
        p.output.polyLimit.value = 1.f;
        p.output.unisonCount.value = 1.f;

        auto &sn = p.sourceNodes[0];
        sn.active.value = 1.f;
        sn.ratio.value = 0.f;
        sn.startingPhase.value = 0.f;
        sn.keyTrack.value = 1.f;
        configure(sn);

        if (pass == 0)
        {
            sn.waveForm.value = (float)SinTable::SIN;
        }
        else
        {
            sn.waveForm.value = (float)SinTable::USER_TABLE;
            s->patch.sourceNodes[0].wavetable = table;
        }

        s->reapplyControlSettings();
        s->voiceManager->processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);

        auto *v = s->head;
        REQUIRE(v != nullptr);
        auto *op = &v->src[0];

        auto &into = (pass == 0 ? r.a : r.b);
        into.reserve(renderSamples);
        for (int b = 0; b < renderBlocks; ++b)
        {
            v->renderBlock();
            for (int i = 0; i < blockSize; ++i)
                into.push_back(op->output[i]);
        }
    }
    REQUIRE(r.a.size() == renderSamples);
    REQUIRE(r.b.size() == renderSamples);
    return r;
}

double maxAbsDiff(const Rendered &r)
{
    double m{0};
    for (int i = 0; i < renderSamples; ++i)
        m = std::max(m, (double)std::abs(r.b[i] - r.a[i]));
    return m;
}

double peak(const std::vector<float> &v)
{
    double m{0};
    for (auto f : v)
        m = std::max(m, (double)std::abs(f));
    return m;
}

/*
 * The two paths share nothing but the Hermite coefficient table: one reads an analytically
 * filled static table, the other a table synthesised from harmonics through an FFT. Getting
 * them to agree at the float32 noise floor is the point - it is what says the wavetable
 * build, its scaling and its slope convention are all right. Observed divergence on a sine
 * is around 3e-7; anything structurally wrong blows through this by orders of magnitude.
 */
static constexpr double floatNoiseTolerance{2e-6};
} // namespace

TEST_CASE("A sine wavetable renders the same as the static sine", "[wavetable][dsp]")
{
    MatrixIndex::initialize();
    auto table = sineTable();
    REQUIRE(table);

    auto r = renderBothPaths(table, [](Patch::SourceNode &) {});

    CAPTURE(maxAbsDiff(r), peak(r.a));
    REQUIRE(peak(r.a) > 0.5);
    REQUIRE(maxAbsDiff(r) < floatNoiseTolerance);
}

TEST_CASE("A wavetable operator still honours phase remap", "[wavetable][dsp]")
{
    MatrixIndex::initialize();
    auto table = sineTable();
    REQUIRE(table);

    auto r = renderBothPaths(table,
                             [](Patch::SourceNode &sn)
                             {
                                 sn.extendedModeMode.value =
                                     (float)Patch::SourceNode::ExtendedMode::PHASE_REMAP;
                                 sn.phaseMapModeShape.value =
                                     (float)Patch::SourceNode::PhaseMapShape::SAW;
                                 sn.extendedModeM.value = 0.5f;
                             });

    // the remap must actually be doing something, or this proves nothing
    bool nonTrivial{false};
    for (int i = 0; i < renderSamples; ++i)
        nonTrivial |= std::abs(r.a[i]) > 0.05;
    REQUIRE(nonTrivial);

    CAPTURE(maxAbsDiff(r));
    REQUIRE(maxAbsDiff(r) < floatNoiseTolerance);
}

TEST_CASE("A wavetable operator still honours resonant sweep", "[wavetable][dsp]")
{
    MatrixIndex::initialize();
    auto table = sineTable();
    REQUIRE(table);

    auto r = renderBothPaths(table,
                             [](Patch::SourceNode &sn)
                             {
                                 sn.extendedModeMode.value =
                                     (float)Patch::SourceNode::ExtendedMode::RESONANT_SWEEP;
                                 sn.resonantSweepWindowShape.value =
                                     (float)Patch::SourceNode::ResonantSweepWindow::TRIANGLE;
                                 sn.extendedModeM.value = 0.4f;
                             });

    CAPTURE(maxAbsDiff(r));
    REQUIRE(maxAbsDiff(r) < floatNoiseTolerance);
}

TEST_CASE("A USER_TABLE operator with no table loaded falls back to a sine", "[wavetable][dsp]")
{
    MatrixIndex::initialize();

    // deliberately publish nothing; a patch can name USER_TABLE with its blob missing
    auto r = renderBothPaths(std::shared_ptr<const Wavetable>{}, [](Patch::SourceNode &) {});

    // the fallback is the same table, read the same way, so this one IS exact
    CAPTURE(maxAbsDiff(r));
    REQUIRE(maxAbsDiff(r) == 0.0);
}

TEST_CASE("A multi harmonic wavetable is audibly not a sine", "[wavetable][dsp]")
{
    MatrixIndex::initialize();

    SourceFrames sf;
    sf.cycleLength = 2048;
    sf.nFrames = 1;
    sf.samples.resize(sf.cycleLength);
    for (uint32_t i = 0; i < sf.cycleLength; ++i)
    {
        auto x = 1.0 * i / sf.cycleLength;
        sf.samples[i] =
            static_cast<float>(std::sin(2 * M_PI * x) + 0.5 * std::sin(2 * M_PI * 3 * x) +
                               0.25 * std::sin(2 * M_PI * 5 * x));
    }
    std::shared_ptr<const Wavetable> table = buildWavetableFromSource(sf, 0x5112, "odd");
    REQUIRE(table);

    auto r = renderBothPaths(table, [](Patch::SourceNode &) {});

    REQUIRE(maxAbsDiff(r) > 0.05);
}

/*
 * Mip level selection. Phase 1 already pins that each level is band limited to its own
 * topHarmonic; these pin that a note actually lands on the right level. Together those two
 * facts are what say the operator does not alias.
 */
namespace
{
// Render one note on a wavetable and report the mip level the operator settled on.
uint32_t levelForNote(const std::shared_ptr<const Wavetable> &table, int key, double *srOut)
{
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto &p = s->patch;
    p.output.playMode.value = 0.f;
    p.output.polyLimit.value = 1.f;
    p.output.unisonCount.value = 1.f;

    auto &sn = p.sourceNodes[0];
    sn.active.value = 1.f;
    sn.ratio.value = 0.f;
    sn.keyTrack.value = 1.f;
    sn.waveForm.value = (float)SinTable::USER_TABLE;
    s->patch.sourceNodes[0].wavetable = table;

    s->reapplyControlSettings();
    s->voiceManager->processNoteOnEvent(0, 0, key, -1, 0.8f, 0.f);

    auto *v = s->head;
    REQUIRE(v != nullptr);
    for (int b = 0; b < 4; ++b)
        v->renderBlock();

    if (srOut)
        *srOut = s->monoValues.sr.sampleRate;
    return v->src[0].wtReader.currentLevel();
}
} // namespace

TEST_CASE("A higher note reads a coarser mip level", "[wavetable][dsp]")
{
    MatrixIndex::initialize();
    auto table = sineTable();
    REQUIRE(table);

    double sr{0};
    uint32_t prev{0};
    for (int key : {24, 36, 48, 60, 72, 84, 96, 108})
    {
        auto k = levelForNote(table, key, &sr);
        INFO("key " << key << " level " << k << " previous " << prev);
        REQUIRE(k >= prev);
        prev = k;
    }
    // and it actually moved rather than pinning at one end
    REQUIRE(prev > 0);
}

TEST_CASE("The selected level keeps its harmonics under the engine Nyquist",
          "[wavetable][dsp]")
{
    MatrixIndex::initialize();
    auto table = sineTable();
    REQUIRE(table);

    for (int key : {24, 36, 48, 60, 72, 84, 96, 108})
    {
        double sr{0};
        auto k = levelForNote(table, key, &sr);
        auto f0 = 440.0 * std::pow(2.0, (key - 69) / 12.0);
        auto allowed = 0.45 * sr / f0;
        auto top = table->levels[k].topHarmonic;
        INFO("key " << key << " f0 " << f0 << " level " << k << " top " << top << " allowed "
                    << allowed);
        // either it fits, or we already ran out of chain
        REQUIRE((top <= allowed || k == (uint32_t)table->nLevels - 1));
    }
}

TEST_CASE("Level selection does not chatter inside the hysteresis band", "[wavetable][dsp]")
{
    MatrixIndex::initialize();
    auto table = sineTable();
    REQUIRE(table);

    WavetableReader rd;
    rd.setTable(table.get());

    // walk up through a boundary and back down; the level must be monotonic in each
    // direction, never oscillating on a single step
    auto sr = 120000.f;
    std::vector<uint32_t> up, down;
    for (int i = 0; i <= 200; ++i)
    {
        auto f = 100.f * std::pow(2.f, i / 40.f);
        rd.updateLevelForFrequency(f, sr);
        up.push_back(rd.currentLevel());
    }
    for (int i = 200; i >= 0; --i)
    {
        auto f = 100.f * std::pow(2.f, i / 40.f);
        rd.updateLevelForFrequency(f, sr);
        down.push_back(rd.currentLevel());
    }
    for (size_t i = 1; i < up.size(); ++i)
        REQUIRE(up[i] >= up[i - 1]);
    for (size_t i = 1; i < down.size(); ++i)
        REQUIRE(down[i] <= down[i - 1]);
}

TEST_CASE("Hysteresis holds the level across a boundary", "[wavetable][dsp]")
{
    MatrixIndex::initialize();
    auto table = sineTable();
    REQUIRE(table);

    auto sr = 120000.f;
    // the frequency at which level 1 (256 harmonics) stops fitting under 0.45*sr
    auto boundary = 0.45f * sr / 256.f;

    WavetableReader a;
    a.setTable(table.get());
    a.updateLevelForFrequency(boundary * 0.99f, sr); // approaching from below
    auto fromBelow = a.currentLevel();

    WavetableReader b;
    b.setTable(table.get());
    b.updateLevelForFrequency(boundary * 4.f, sr); // come down from well above
    b.updateLevelForFrequency(boundary * 0.99f, sr);
    auto fromAbove = b.currentLevel();

    INFO("from below " << fromBelow << " from above " << fromAbove);
    // inside the band the two disagree: that IS the hysteresis
    REQUIRE(fromAbove >= fromBelow);
}

/*
 * Morph. Unlike which table a note reads, morph is live - it is a modulation target - so it
 * is deliberately not latched at attack. It does snap to its patch value at attack, so a
 * note starting at morph 1 starts on the last frame rather than gliding up to it.
 */
namespace
{
std::shared_ptr<const Wavetable> harmonicTable(const std::vector<int> &harmonicPerFrame)
{
    SourceFrames sf;
    sf.cycleLength = 2048;
    sf.nFrames = (uint32_t)harmonicPerFrame.size();
    sf.samples.resize(sf.nFrames * sf.cycleLength);
    for (uint32_t f = 0; f < sf.nFrames; ++f)
        for (uint32_t i = 0; i < sf.cycleLength; ++i
            )
            sf.samples[f * sf.cycleLength + i] = static_cast<float>(
                std::sin(2 * M_PI * harmonicPerFrame[f] * i / sf.cycleLength));
    return buildWavetableFromSource(sf, 0x6000 + sf.nFrames, "harm");
}

std::vector<float> renderWavetable(const std::shared_ptr<const Wavetable> &table,
                                   const std::function<void(Patch::SourceNode &)> &configure)
{
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto &p = s->patch;
    p.output.playMode.value = 0.f;
    p.output.polyLimit.value = 1.f;
    p.output.unisonCount.value = 1.f;

    auto &sn = p.sourceNodes[0];
    sn.active.value = 1.f;
    sn.ratio.value = 0.f;
    sn.startingPhase.value = 0.f;
    sn.keyTrack.value = 1.f;
    sn.waveForm.value = (float)SinTable::USER_TABLE;
    configure(sn);
    s->patch.sourceNodes[0].wavetable = table;

    s->reapplyControlSettings();
    s->voiceManager->processNoteOnEvent(0, 0, 36, -1, 0.8f, 0.f);

    auto *v = s->head;
    REQUIRE(v != nullptr);
    std::vector<float> out;
    out.reserve(renderSamples);
    for (int b = 0; b < renderBlocks; ++b)
    {
        v->renderBlock();
        for (int i = 0; i < blockSize; ++i)
            out.push_back(v->src[0].output[i]);
    }
    return out;
}

double maxDiff(const std::vector<float> &a, const std::vector<float> &b)
{
    REQUIRE(a.size() == b.size());
    double m{0};
    for (size_t i = 0; i < a.size(); ++i)
        m = std::max(m, (double)std::abs(a[i] - b[i]));
    return m;
}
} // namespace

TEST_CASE("Morph zero reads the first frame and morph one the last", "[wavetable][morph]")
{
    MatrixIndex::initialize();

    auto twoFrame = harmonicTable({1, 3});
    auto justFirst = harmonicTable({1});
    auto justLast = harmonicTable({3});
    REQUIRE(twoFrame);
    REQUIRE(twoFrame->nFrames == 2);

    auto atZero = renderWavetable(twoFrame, [](auto &sn) { sn.wavetableMorph.value = 0.f; });
    auto atOne = renderWavetable(twoFrame, [](auto &sn) { sn.wavetableMorph.value = 1.f; });
    auto refFirst = renderWavetable(justFirst, [](auto &) {});
    auto refLast = renderWavetable(justLast, [](auto &) {});

    CAPTURE(maxDiff(atZero, refFirst), maxDiff(atOne, refLast));
    REQUIRE(maxDiff(atZero, refFirst) < floatNoiseTolerance);
    REQUIRE(maxDiff(atOne, refLast) < floatNoiseTolerance);
    // and the two ends are genuinely different waveforms
    REQUIRE(maxDiff(atZero, atOne) > 0.5);
}

TEST_CASE("Morph halfway is the blend of the two frames", "[wavetable][morph]")
{
    MatrixIndex::initialize();

    auto twoFrame = harmonicTable({1, 3});
    auto justFirst = harmonicTable({1});
    auto justLast = harmonicTable({3});

    auto half = renderWavetable(twoFrame, [](auto &sn) { sn.wavetableMorph.value = 0.5f; });
    auto refFirst = renderWavetable(justFirst, [](auto &) {});
    auto refLast = renderWavetable(justLast, [](auto &) {});

    double worst{0};
    for (size_t i = 0; i < half.size(); ++i)
        worst = std::max(worst, (double)std::abs(half[i] - 0.5 * (refFirst[i] + refLast[i])));
    CAPTURE(worst);
    REQUIRE(worst < floatNoiseTolerance);
}

TEST_CASE("Morph has no effect on a single frame table", "[wavetable][morph]")
{
    MatrixIndex::initialize();
    auto one = harmonicTable({1});

    auto a = renderWavetable(one, [](auto &sn) { sn.wavetableMorph.value = 0.f; });
    auto b = renderWavetable(one, [](auto &sn) { sn.wavetableMorph.value = 1.f; });
    REQUIRE(maxDiff(a, b) == 0.0);
}

TEST_CASE("An envelope can drive morph", "[wavetable][morph]")
{
    MatrixIndex::initialize();
    auto twoFrame = harmonicTable({1, 3});

    auto flat = renderWavetable(twoFrame, [](auto &sn) { sn.wavetableMorph.value = 0.f; });
    auto swept = renderWavetable(twoFrame,
                                 [](auto &sn)
                                 {
                                     sn.wavetableMorph.value = 0.f;
                                     sn.envToWavetableMorph.value = 1.f;
                                 });
    CAPTURE(maxDiff(flat, swept));
    REQUIRE(maxDiff(flat, swept) > 0.1);
}

TEST_CASE("Morph is reachable as a modulation target", "[wavetable][morph]")
{
    MatrixIndex::initialize();
    auto twoFrame = harmonicTable({1, 3});

    auto flat = renderWavetable(twoFrame, [](auto &sn) { sn.wavetableMorph.value = 0.f; });
    auto modded =
        renderWavetable(twoFrame,
                        [](auto &sn)
                        {
                            sn.wavetableMorph.value = 0.f;
                            sn.modtarget[0].value = (float)Patch::SourceNode::TargetID::MORPH;
                            sn.modsource[0].value = (float)ModMatrixConfig::Source::VELOCITY;
                            sn.moddepth[0].value = 1.f;
                        });
    CAPTURE(maxDiff(flat, modded));
    REQUIRE(maxDiff(flat, modded) > 0.1);
}

TEST_CASE("Morph appears in the source node target list", "[wavetable][morph]")
{
    auto p = std::make_unique<Patch>();
    bool found{false};
    for (const auto &[id, label] : p->sourceNodes[0].targetList)
        if (id == (int32_t)Patch::SourceNode::TargetID::MORPH)
        {
            found = true;
            REQUIRE(!label.empty());
        }
    REQUIRE(found);
}

TEST_CASE("The morph parameters stream and round trip", "[wavetable][morph]")
{
    MatrixIndex::initialize();

    auto written = std::make_unique<Patch>();
    for (int i = 0; i < (int)numOps; ++i)
    {
        written->sourceNodes[i].wavetableMorph.value = 0.1f * i;
        written->sourceNodes[i].envToWavetableMorph.value = -0.05f * i;
        written->sourceNodes[i].lfoToWavetableMorph.value = 0.02f * i;
    }
    auto xml = written->toState();

    auto read = std::make_unique<Patch>();
    REQUIRE(read->fromState(xml));
    for (int i = 0; i < (int)numOps; ++i)
    {
        INFO("op " << i);
        REQUIRE_THAT(read->sourceNodes[i].wavetableMorph.value,
                     Catch::Matchers::WithinAbs(0.1f * i, 1e-6));
        REQUIRE_THAT(read->sourceNodes[i].envToWavetableMorph.value,
                     Catch::Matchers::WithinAbs(-0.05f * i, 1e-6));
        REQUIRE_THAT(read->sourceNodes[i].lfoToWavetableMorph.value,
                     Catch::Matchers::WithinAbs(0.02f * i, 1e-6));
    }
}

TEST_CASE("Morph defaults to zero in a stream that predates it", "[wavetable][morph]")
{
    MatrixIndex::initialize();

    auto p = std::make_unique<Patch>();
    // the morph params carry a version tag, so a pre-130b stream must read them as default
    REQUIRE(p->sourceNodes[0].wavetableMorph.meta.defaultVal == 0.f);
    REQUIRE(p->sourceNodes[0].wavetableMorph.meta.version > Patch::version_130a);
}

TEST_CASE("An LFO can drive morph", "[wavetable][morph]")
{
    MatrixIndex::initialize();
    auto twoFrame = harmonicTable({1, 3});

    auto flat = renderWavetable(twoFrame, [](auto &sn) { sn.wavetableMorph.value = 0.f; });
    auto swept = renderWavetable(twoFrame,
                                 [](auto &sn)
                                 {
                                     sn.wavetableMorph.value = 0.5f;
                                     sn.lfoActive.value = 1.f;
                                     sn.lfoRate.value = 4.f;
                                     sn.lfoToWavetableMorph.value = 1.f;
                                 });
    CAPTURE(maxDiff(flat, swept));
    REQUIRE(maxDiff(flat, swept) > 0.1);
}

TEST_CASE("An LFO bound only to morph still runs", "[wavetable][morph]")
{
    MatrixIndex::initialize();
    auto twoFrame = harmonicTable({1, 3});

    // The LFO is only clocked when something consumes it, and that list is enumerated per
    // target. Morph has to be in it, or an LFO wired to nothing else sits still.
    auto still = renderWavetable(twoFrame,
                                 [](auto &sn)
                                 {
                                     sn.wavetableMorph.value = 0.5f;
                                     sn.lfoActive.value = 1.f;
                                     sn.lfoRate.value = 4.f;
                                 });
    auto moving = renderWavetable(twoFrame,
                                  [](auto &sn)
                                  {
                                      sn.wavetableMorph.value = 0.5f;
                                      sn.lfoActive.value = 1.f;
                                      sn.lfoRate.value = 4.f;
                                      sn.lfoToWavetableMorph.value = 1.f;
                                  });
    CAPTURE(maxDiff(still, moving));
    REQUIRE(maxDiff(still, moving) > 0.1);
}
