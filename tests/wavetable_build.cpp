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
#include <vector>

#include "dsp/wavetable.h"
#include "dsp/wavetable_build.h"

using namespace baconpaul::six_sines;

namespace
{
// one cycle of sum_n a_n cos(2 pi n x) + b_n sin(2 pi n x), sampled at cycleLength
struct Harmonic
{
    int n;
    double a, b;
};

double evalHarmonics(const std::vector<Harmonic> &hs, double x)
{
    double v{0};
    for (const auto &h : hs)
        v += h.a * std::cos(2 * M_PI * h.n * x) + h.b * std::sin(2 * M_PI * h.n * x);
    return v;
}

double evalHarmonicsDeriv(const std::vector<Harmonic> &hs, double x)
{
    double v{0};
    for (const auto &h : hs)
        v += 2 * M_PI * h.n *
             (-h.a * std::sin(2 * M_PI * h.n * x) + h.b * std::cos(2 * M_PI * h.n * x));
    return v;
}

SourceFrames makeSource(const std::vector<std::vector<Harmonic>> &frames, uint32_t cycleLength)
{
    SourceFrames sf;
    sf.cycleLength = cycleLength;
    sf.nFrames = static_cast<uint32_t>(frames.size());
    sf.samples.resize(frames.size() * cycleLength);
    for (size_t f = 0; f < frames.size(); ++f)
        for (uint32_t i = 0; i < cycleLength; ++i)
            sf.samples[f * cycleLength + i] =
                static_cast<float>(evalHarmonics(frames[f], 1.0 * i / cycleLength));
    return sf;
}
} // namespace

TEST_CASE("Built table reproduces its source waveform off-grid", "[wavetable]")
{
    std::vector<Harmonic> hs{{1, 1.0, 0.0}, {3, 0.0, -0.4}, {7, 0.15, 0.2}};
    auto src = makeSource({hs}, 2048);

    auto wt = buildWavetableFromSource(src, 0xABCDull, "probe");
    REQUIRE(wt);
    REQUIRE(wt->nFrames == 1);
    REQUIRE(wt->hash == 0xABCDull);

    WavetableReader rd;
    rd.setTable(wt.get());
    rd.setLevel(0);
    rd.setMorph(0.f);

    // deliberately off the table grid so the Hermite slope actually participates
    for (int t = 0; t < 97; ++t)
    {
        double x = t / 97.0 + 0.0031;
        auto ph = static_cast<uint32_t>(x * phase::phaseMaxF);
        REQUIRE_THAT(rd.at(ph), Catch::Matchers::WithinAbs(evalHarmonics(hs, x), 1e-4));
    }
}

TEST_CASE("Stored slope is the analytic derivative per table step", "[wavetable]")
{
    std::vector<Harmonic> hs{{1, 1.0, 0.0}, {5, 0.3, -0.25}};
    auto src = makeSource({hs}, 2048);
    auto wt = buildWavetableFromSource(src, 1, "slope");
    REQUIRE(wt);

    const auto &l0 = wt->levels[0];
    auto n = l0.nPoints;
    REQUIRE(n == wt::basePoints);

    for (uint32_t i : {0u, 1u, n / 8, n / 4, n / 3, n / 2, n - 1})
    {
        double x = 1.0 * i / n;
        // stored slope is dv per table index, i.e. dv/dx / n
        auto expect = evalHarmonicsDeriv(hs, x) / n;
        REQUIRE_THAT(l0.data[2 * i + 1], Catch::Matchers::WithinAbs(expect, 1e-6));
    }
}

TEST_CASE("Table wraps: index n equals index 0", "[wavetable]")
{
    auto src = makeSource({{{1, 1.0, 0.0}, {4, 0.2, 0.1}}}, 2048);
    auto wt = buildWavetableFromSource(src, 1, "wrap");
    REQUIRE(wt);

    for (uint8_t k = 0; k < wt->nLevels; ++k)
    {
        const auto &l = wt->levels[k];
        REQUIRE_THAT(l.data[2 * l.nPoints], Catch::Matchers::WithinAbs(l.data[0], 1e-7));
        REQUIRE_THAT(l.data[2 * l.nPoints + 1], Catch::Matchers::WithinAbs(l.data[1], 1e-7));
    }
}

TEST_CASE("Mip level holds no energy above its top harmonic", "[wavetable]")
{
    // a saw-ish stack running well past the level-1 limit of 256
    std::vector<Harmonic> hs;
    for (int n = 1; n <= 400; ++n)
        hs.push_back({n, 0.0, 1.0 / n});
    auto src = makeSource({hs}, 2048);

    auto wt = buildWavetableFromSource(src, 1, "saw");
    REQUIRE(wt);
    REQUIRE(wt->nLevels > 2);

    for (uint8_t k = 0; k < wt->nLevels; ++k)
    {
        const auto &l = wt->levels[k];
        auto spec = analyzeLevelForTest(*wt, k, 0);
        // spec[n] is the magnitude of harmonic n in the built level
        double peak{0};
        for (uint32_t n = 1; n <= l.topHarmonic && n < spec.size(); ++n)
            peak = std::max(peak, spec[n]);
        double leak{0};
        for (uint32_t n = l.topHarmonic + 1; n < spec.size(); ++n)
            leak = std::max(leak, spec[n]);
        INFO("level " << (int)k << " topHarmonic " << l.topHarmonic << " peak " << peak << " leak "
                      << leak);
        REQUIRE(peak > 1e-3);
        REQUIRE(leak < peak * 1e-6);
    }
}

TEST_CASE("Frame axis is resampled down to the frame cap", "[wavetable]")
{
    std::vector<std::vector<Harmonic>> frames;
    for (int f = 0; f < 200; ++f)
        frames.push_back({{1, 1.0 - f / 400.0, 0.0}, {2, f / 400.0, 0.0}});
    auto src = makeSource(frames, 512);

    auto wt = buildWavetableFromSource(src, 1, "many");
    REQUIRE(wt);
    REQUIRE(wt->nFrames == wt::maxFrames);
}

TEST_CASE("Frame counts at or below the cap are kept exactly", "[wavetable]")
{
    std::vector<std::vector<Harmonic>> frames;
    for (int f = 0; f < 11; ++f)
        frames.push_back({{1, 1.0, 0.0}, {3, f / 11.0, 0.0}});
    auto src = makeSource(frames, 512);

    auto wt = buildWavetableFromSource(src, 1, "eleven");
    REQUIRE(wt);
    REQUIRE(wt->nFrames == 11);
}

TEST_CASE("Morph at a frame boundary reads that frame exactly", "[wavetable]")
{
    std::vector<Harmonic> f0{{1, 1.0, 0.0}};
    std::vector<Harmonic> f1{{2, 0.0, 1.0}};
    std::vector<Harmonic> f2{{3, 0.5, 0.0}};
    auto src = makeSource({f0, f1, f2}, 2048);
    auto wt = buildWavetableFromSource(src, 1, "three");
    REQUIRE(wt);
    REQUIRE(wt->nFrames == 3);

    WavetableReader rd;
    rd.setTable(wt.get());
    rd.setLevel(0);

    struct
    {
        float morph;
        const std::vector<Harmonic> *hs;
    } cases[]{{0.0f, &f0}, {0.5f, &f1}, {1.0f, &f2}};

    for (const auto &c : cases)
    {
        rd.setMorph(c.morph);
        for (int t = 0; t < 33; ++t)
        {
            double x = t / 33.0 + 0.0017;
            auto ph = static_cast<uint32_t>(x * phase::phaseMaxF);
            REQUIRE_THAT(rd.at(ph), Catch::Matchers::WithinAbs(evalHarmonics(*c.hs, x), 1e-4));
        }
    }
}

TEST_CASE("Morph between frames is the linear blend of the two", "[wavetable]")
{
    std::vector<Harmonic> f0{{1, 1.0, 0.0}};
    std::vector<Harmonic> f1{{2, 0.0, 1.0}};
    auto src = makeSource({f0, f1}, 2048);
    auto wt = buildWavetableFromSource(src, 1, "two");
    REQUIRE(wt);

    WavetableReader rd;
    rd.setTable(wt.get());
    rd.setLevel(0);
    rd.setMorph(0.25f);

    for (int t = 0; t < 33; ++t)
    {
        double x = t / 33.0 + 0.0017;
        auto ph = static_cast<uint32_t>(x * phase::phaseMaxF);
        auto expect = 0.75 * evalHarmonics(f0, x) + 0.25 * evalHarmonics(f1, x);
        REQUIRE_THAT(rd.at(ph), Catch::Matchers::WithinAbs(expect, 1e-4));
    }
}

TEST_CASE("Single frame table reads identically at every morph value", "[wavetable]")
{
    auto src = makeSource({{{1, 1.0, 0.0}, {2, 0.3, 0.0}}}, 2048);
    auto wt = buildWavetableFromSource(src, 1, "one");
    REQUIRE(wt);
    REQUIRE(wt->nFrames == 1);

    WavetableReader rd;
    rd.setTable(wt.get());
    rd.setLevel(0);

    for (int t = 0; t < 17; ++t)
    {
        auto ph = static_cast<uint32_t>((t / 17.0 + 0.011) * phase::phaseMaxF);
        rd.setMorph(0.f);
        auto base = rd.at(ph);
        for (float m : {0.1f, 0.5f, 0.9f, 1.0f})
        {
            rd.setMorph(m);
            REQUIRE(rd.at(ph) == base);
        }
    }
}

TEST_CASE("Level selection picks the coarsest level that still covers the note", "[wavetable]")
{
    auto src = makeSource({{{1, 1.0, 0.0}}}, 2048);
    auto wt = buildWavetableFromSource(src, 1, "levels");
    REQUIRE(wt);

    WavetableReader rd;
    rd.setTable(wt.get());

    // 0.45 * 240000 = 108000 Hz of usable harmonic space
    struct
    {
        float f0;
        uint32_t topHarmonic;
        // 0.45 * 240000 = 108000Hz of usable harmonic space, against a chain of
        // 1024/512/256/... so the level holding the most harmonics that still fit wins
    } cases[]{{55.f, 1024}, {220.f, 256}, {440.f, 128},
              {880.f, 64},  {1760.f, 32}, {3520.f, 16}};

    for (const auto &c : cases)
    {
        rd.setLevelForFrequency(c.f0, 240000.f);
        INFO("f0 " << c.f0);
        REQUIRE(rd.currentTopHarmonic() == c.topHarmonic);
    }
}

TEST_CASE("Level selection clamps at both ends of the chain", "[wavetable]")
{
    auto src = makeSource({{{1, 1.0, 0.0}}}, 2048);
    auto wt = buildWavetableFromSource(src, 1, "clamp");
    REQUIRE(wt);

    WavetableReader rd;
    rd.setTable(wt.get());

    rd.setLevelForFrequency(1.f, 240000.f);
    REQUIRE(rd.currentLevel() == 0);

    rd.setLevelForFrequency(40000.f, 240000.f);
    REQUIRE(rd.currentLevel() == wt->nLevels - 1);
}

TEST_CASE("A source cycle shorter than the harmonic budget is not invented past Nyquist",
          "[wavetable]")
{
    // 128-point cycle carries at most 63 usable harmonics
    auto src = makeSource({{{1, 1.0, 0.0}, {40, 0.2, 0.0}}}, 128);
    auto wt = buildWavetableFromSource(src, 1, "short");
    REQUIRE(wt);

    auto spec = analyzeLevelForTest(*wt, 0, 0);
    for (size_t n = 64; n < spec.size(); ++n)
    {
        INFO("harmonic " << n);
        REQUIRE(spec[n] < 1e-6);
    }
}

TEST_CASE("Unusable sources are rejected rather than half built", "[wavetable]")
{
    SECTION("zero frames")
    {
        SourceFrames sf;
        sf.cycleLength = 2048;
        sf.nFrames = 0;
        REQUIRE(buildWavetableFromSource(sf, 1, "empty") == nullptr);
    }

    SECTION("cycle length too short for an FFT")
    {
        SourceFrames sf;
        sf.cycleLength = 16;
        sf.nFrames = 1;
        sf.samples.resize(16, 0.f);
        REQUIRE(buildWavetableFromSource(sf, 1, "tiny") == nullptr);
    }

    SECTION("sample buffer smaller than the declared geometry")
    {
        SourceFrames sf;
        sf.cycleLength = 2048;
        sf.nFrames = 4;
        sf.samples.resize(2048, 0.f);
        REQUIRE(buildWavetableFromSource(sf, 1, "truncated") == nullptr);
    }
}

TEST_CASE("A hard step rings, and the ringing is the source's own", "[wavetable]")
{
    /*
     * Worth being precise about what this does and does not pin, because the obvious
     * expectation is wrong.
     *
     * A square sampled at 2048 points sits at exactly +1 and -1 either side of its jump. Any
     * band limited reconstruction has to pass through those samples, and getting from +1 to -1
     * in one step while staying band limited costs a large excursion - so it overshoots by
     * MORE than the 8.95% Gibbs figure for a continuous square, not less. Measured here at
     * about 1.28 keeping all 1024 harmonics, against about 1.20 when it was truncated to 512.
     *
     * So keeping every harmonic does not remove ringing on a hard step. Ringing is what a
     * band limited signal through those samples genuinely is. What it removes is the EXTRA
     * ringing from discarding half the source, and it makes the table faithful: the
     * reconstruction now passes through the source samples instead of a low passed version of
     * them. This bound is a guard against gross error, not a quality target.
     */
    SourceFrames sf;
    sf.cycleLength = 2048;
    sf.nFrames = 1;
    sf.samples.resize(sf.cycleLength);
    for (uint32_t i = 0; i < sf.cycleLength; ++i)
        sf.samples[i] = (i < sf.cycleLength / 2) ? 1.f : -1.f;

    auto wt = buildWavetableFromSource(sf, 1, "square");
    REQUIRE(wt);
    const auto &l = wt->levels[0];

    double peak{0};
    for (uint32_t i = 0; i < l.nPoints; ++i)
        peak = std::max(peak, (double)std::abs(l.data[2 * i]));
    CAPTURE(peak);
    REQUIRE(peak > 1.0);  // it does ring, and pretending otherwise would be the bug
    REQUIRE(peak < 1.35);
}

TEST_CASE("A short cycle is not stored at more resolution than it needs", "[wavetable]")
{
    // 128 points carries 63 harmonics; storing that at the full base size would be 128x
    // oversampled for nothing
    auto shortSrc = makeSource({{{1, 1.0, 0.0}, {5, 0.2, 0.0}}}, 128);
    auto small = buildWavetableFromSource(shortSrc, 1, "short");
    REQUIRE(small);

    auto longSrc = makeSource({{{1, 1.0, 0.0}, {5, 0.2, 0.0}}}, 2048);
    auto big = buildWavetableFromSource(longSrc, 2, "long");
    REQUIRE(big);

    INFO("short " << small->levels[0].nPoints << " long " << big->levels[0].nPoints);
    REQUIRE(small->levels[0].nPoints < big->levels[0].nPoints);
    REQUIRE(small->memoryBytes() < big->memoryBytes() / 4);
}

namespace
{
SourceFrames hardSquare(uint32_t n = 2048)
{
    SourceFrames sf;
    sf.cycleLength = n;
    sf.nFrames = 1;
    sf.samples.resize(n);
    for (uint32_t i = 0; i < n; ++i)
        sf.samples[i] = (i < n / 2) ? 1.f : -1.f;
    return sf;
}

double peakOf(const Wavetable &w)
{
    const auto &l = w.levels[0];
    double p{0};
    for (uint32_t i = 0; i < l.nPoints; ++i)
        p = std::max(p, (double)std::abs(l.data[2 * i]));
    return p;
}
} // namespace

TEST_CASE("Direct playback is the source samples, untouched", "[wavetable][mode]")
{
    auto src = hardSquare();
    auto wt = buildWavetableFromSource(src, 1, "sq", WavetableBandLimit::DIRECT);
    REQUIRE(wt);

    // no transform, so no ringing at all and no mip chain to pick from
    REQUIRE(wt->nLevels == 1);
    REQUIRE(wt->levels[0].nPoints == src.cycleLength);
    REQUIRE_THAT(peakOf(*wt), Catch::Matchers::WithinAbs(1.0, 1e-6));

    for (uint32_t i : {0u, 1u, 500u, 1023u, 1024u, 2047u})
        REQUIRE_THAT(wt->levels[0].data[2 * i],
                     Catch::Matchers::WithinAbs(src.samples[i], 1e-6));
}

TEST_CASE("Tapering suppresses the ringing band limiting leaves", "[wavetable][mode]")
{
    auto src = hardSquare();
    auto plain = buildWavetableFromSource(src, 1, "sq", WavetableBandLimit::BAND_LIMITED);
    auto tapered =
        buildWavetableFromSource(src, 2, "sq", WavetableBandLimit::BAND_LIMITED_TAPERED);
    REQUIRE(plain);
    REQUIRE(tapered);

    CAPTURE(peakOf(*plain), peakOf(*tapered));
    REQUIRE(peakOf(*tapered) < peakOf(*plain));
    REQUIRE(peakOf(*tapered) < 1.05); // the overshoot is essentially gone
}

TEST_CASE("Tapering costs brightness, which is why it is not the default",
          "[wavetable][mode]")
{
    // a flat harmonic stack shows the rolloff plainly
    std::vector<Harmonic> hs;
    for (int n = 1; n <= 400; ++n)
        hs.push_back({n, 0.0, 0.05});
    auto src = makeSource({hs}, 2048);

    auto plain = buildWavetableFromSource(src, 1, "s", WavetableBandLimit::BAND_LIMITED);
    auto tapered = buildWavetableFromSource(src, 2, "s", WavetableBandLimit::BAND_LIMITED_TAPERED);
    auto sp = analyzeLevelForTest(*plain, 0, 0);
    auto st = analyzeLevelForTest(*tapered, 0, 0);

    // low harmonics barely move, high ones come down
    REQUIRE_THAT(st[1] / sp[1], Catch::Matchers::WithinAbs(1.0, 0.01));
    REQUIRE(st[400] < sp[400] * 0.95);
}

TEST_CASE("Mode is part of a table's identity", "[wavetable][mode]")
{
    // otherwise two operators on one file in different modes would share one table
    auto a = saltHashWithMode(12345, WavetableBandLimit::BAND_LIMITED, true);
    auto b = saltHashWithMode(12345, WavetableBandLimit::BAND_LIMITED_TAPERED, true);
    auto c = saltHashWithMode(12345, WavetableBandLimit::DIRECT, true);
    auto d = saltHashWithMode(12345, WavetableBandLimit::BAND_LIMITED, false);
    REQUIRE(a != b);
    REQUIRE(b != c);
    REQUIRE(a != c);
    REQUIRE(a != d); // the chain flag is part of it too
}

TEST_CASE("Direct falls back when the cycle cannot be read by bit mask", "[wavetable][mode]")
{
    // the reader indexes by mask, so a non power of two cycle has to take the transform path
    auto src = hardSquare(1536); // 512 * 3: a valid FFT length, not a power of two
    auto wt = buildWavetableFromSource(src, 1, "odd", WavetableBandLimit::DIRECT);
    REQUIRE(wt);
    REQUIRE(wt->nLevels > 1); // it built the chain instead of refusing
}

TEST_CASE("Turning the mip chain off leaves the finest level and nothing else",
          "[wavetable][mode]")
{
    auto src = hardSquare();
    auto chain = buildWavetableFromSource(src, 1, "sq", WavetableBandLimit::BAND_LIMITED);
    auto one = buildWavetableFromSource(src, 2, "sq", WavetableBandLimit::BAND_LIMITED, false);
    REQUIRE(chain);
    REQUIRE(one);

    REQUIRE(one->nLevels == 1);
    REQUIRE(chain->nLevels > 1);
    REQUIRE(one->levels[0].nPoints == chain->levels[0].nPoints);

    // the reconstruction is the same one, so it should be the same samples, not merely close
    for (uint32_t i = 0; i < one->levels[0].nPoints; i += 37)
    {
        INFO("point " << i);
        REQUIRE(one->levels[0].data[2 * i] == chain->levels[0].data[2 * i]);
        REQUIRE(one->levels[0].data[2 * i + 1] == chain->levels[0].data[2 * i + 1]);
    }
    // and it costs what one level costs
    REQUIRE(one->memoryBytes() < chain->memoryBytes() * 0.6);
}

TEST_CASE("Direct and chainless band limiting differ in reconstruction, not in band limit",
          "[wavetable][mode]")
{
    auto src = hardSquare();
    auto native = buildWavetableFromSource(src, 1, "sq", WavetableBandLimit::DIRECT);
    auto upsamp = buildWavetableFromSource(src, 2, "sq", WavetableBandLimit::BAND_LIMITED, false);
    REQUIRE(native);
    REQUIRE(upsamp);

    // neither has a chain
    REQUIRE(native->nLevels == 1);
    REQUIRE(upsamp->nLevels == 1);

    // native reads the raw grid, so a step stays a step; upsampled reconstructs it, so it
    // rings exactly as the band limited path does
    CAPTURE(peakOf(*native), peakOf(*upsamp));
    REQUIRE_THAT(peakOf(*native), Catch::Matchers::WithinAbs(1.0, 1e-6));
    REQUIRE(peakOf(*upsamp) > 1.2);
    REQUIRE(upsamp->levels[0].nPoints > native->levels[0].nPoints * 4);
}

TEST_CASE("The chain flag applies to tapering too", "[wavetable][mode]")
{
    // the combination that did not exist when this was four named modes
    auto src = hardSquare();
    auto t = buildWavetableFromSource(src, 1, "sq",
                                      WavetableBandLimit::BAND_LIMITED_TAPERED, false);
    REQUIRE(t);
    REQUIRE(t->nLevels == 1);
    CAPTURE(peakOf(*t));
    REQUIRE(peakOf(*t) < 1.05); // still tapered
}

TEST_CASE("Direct wraps cyclically at both ends", "[wavetable][mode]")
{
    // A ramp makes the wrap obvious: every interior slope is the same, and the two at the
    // seam are only right if they reach across it.
    SourceFrames sf;
    sf.cycleLength = 1024;
    sf.nFrames = 1;
    sf.samples.resize(sf.cycleLength);
    for (uint32_t i = 0; i < sf.cycleLength; ++i)
        sf.samples[i] = 2.f * i / sf.cycleLength - 1.f; // -1 .. +1 ramp, jump at the seam

    auto wt = buildWavetableFromSource(sf, 1, "ramp", WavetableBandLimit::DIRECT);
    REQUIRE(wt);
    const auto &l = wt->levels[0];
    auto n = l.nPoints;
    REQUIRE(n == sf.cycleLength);

    auto step = 2.f / sf.cycleLength;

    // interior: the Catmull-Rom tangent is the ramp's own slope
    REQUIRE_THAT(l.data[2 * 500 + 1], Catch::Matchers::WithinAbs(step, 1e-6));

    // index 0 reaches back to index n-1, and index n-1 reaches forward to index 0, so both
    // see the ramp's reset rather than running off the end of the buffer
    auto seamSlope = 0.5f * (sf.samples[1] - sf.samples[n - 1]);
    REQUIRE_THAT(l.data[1], Catch::Matchers::WithinAbs(seamSlope, 1e-6));
    auto lastSlope = 0.5f * (sf.samples[0] - sf.samples[n - 2]);
    REQUIRE_THAT(l.data[2 * (n - 1) + 1], Catch::Matchers::WithinAbs(lastSlope, 1e-6));

    // and the pair at the top is index 0, value and slope, since at() loads both together
    REQUIRE(l.data[2 * n] == l.data[0]);
    REQUIRE(l.data[2 * n + 1] == l.data[1]);
}

TEST_CASE("Reading a direct table across the seam is continuous", "[wavetable][mode]")
{
    auto src = makeSource({{{1, 1.0, 0.0}, {3, 0.2, 0.0}}}, 1024);
    auto wt = buildWavetableFromSource(src, 1, "sine", WavetableBandLimit::DIRECT);
    REQUIRE(wt);

    WavetableReader rd;
    rd.setTable(wt.get());
    rd.setMorph(0.f);

    // walk right through phase 0 and require no step in the output
    auto stepPh = phase::phaseMax / wt->levels[0].nPoints / 4;
    float prev = rd.at(0u - 8 * stepPh);
    for (int i = -7; i <= 8; ++i)
    {
        auto v = rd.at(static_cast<uint32_t>(i * (int64_t)stepPh));
        INFO("i " << i << " prev " << prev << " v " << v);
        REQUIRE(std::abs(v - prev) < 0.02);
        prev = v;
    }
}

TEST_CASE("Zero order hold holds instead of interpolating", "[wavetable][mode]")
{
    // A ramp makes it obvious: Hermite tracks it smoothly, ZOH steps.
    SourceFrames sf;
    sf.cycleLength = 64;
    sf.nFrames = 1;
    sf.samples.resize(sf.cycleLength);
    for (uint32_t i = 0; i < sf.cycleLength; ++i)
        sf.samples[i] = 1.f * i / sf.cycleLength;

    auto wt = buildWavetableFromSource(sf, 1, "ramp", WavetableBandLimit::DIRECT);
    REQUIRE(wt);

    WavetableReader herm, zoh;
    herm.setTable(wt.get());
    zoh.setTable(wt.get());
    zoh.setZeroOrderHold(true);

    auto step = phase::phaseMax / sf.cycleLength;

    // on the grid the two agree exactly, since the sample is the sample
    for (uint32_t i : {0u, 7u, 31u, 60u})
    {
        auto ph = i * step;
        INFO("grid point " << i);
        REQUIRE(zoh.at(ph) == herm.at(ph));
    }

    // between grid points ZOH does not move at all, and Hermite does
    for (uint32_t i : {3u, 17u, 44u})
    {
        auto base = i * step;
        auto held = zoh.at(base);
        for (auto frac : {step / 8, step / 3, step / 2, (step * 7) / 8})
        {
            INFO("point " << i << " frac " << frac);
            REQUIRE(zoh.at(base + frac) == held);
        }
        REQUIRE(herm.at(base + step / 2) != held);
    }
}

TEST_CASE("Zero order hold shares Direct's table rather than building a second",
          "[wavetable][mode]")
{
    // it is how the table is read, not how it is built
    auto a = saltHashWithMode(999, WavetableBandLimit::DIRECT, false);
    auto b = saltHashWithMode(999, WavetableBandLimit::DIRECT_ZOH, false);
    REQUIRE(a == b);
    // but still distinct from the band limited builds
    REQUIRE(a != saltHashWithMode(999, WavetableBandLimit::BAND_LIMITED, false));
}
