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

#ifndef BACONPAUL_SIX_SINES_DSP_WAVETABLE_H
#define BACONPAUL_SIX_SINES_DSP_WAVETABLE_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "configuration.h"
#include "dsp/sintable.h"

namespace baconpaul::six_sines
{
namespace wt
{
/*
 * 16 points per period of the top harmonic puts cubic Hermite interpolation error near -84dB,
 * and that ratio holds at every level down the chain.
 *
 * 1024 harmonics is what a 2048 sample cycle carries, which is the common case for both Serum
 * and Surge tables. Keeping fewer would mean band limiting the source even at the bottom of
 * the keyboard where every harmonic reproduces perfectly, and a brick wall truncation of a
 * sharp waveform rings: a hard step loses 8.95% of its jump to Gibbs overshoot no matter how
 * many terms are kept. Keeping the lot means the only band limiting is the mip chain's, which
 * is there to stop aliasing rather than to save room.
 *
 * The chain is sized from the source rather than always starting at basePoints, so a short
 * cycle does not get stored at 128 times the resolution it needs.
 */
static constexpr uint32_t pointsPerTopHarmonic{16};
static constexpr uint32_t baseHarmonics{1024};
static constexpr uint32_t basePoints{baseHarmonics * pointsPerTopHarmonic};
static constexpr uint32_t basePosBits{14}; // log2(basePoints)
static constexpr uint32_t minPoints{64};   // pffft floor is 32; stop well above it
static constexpr uint32_t maxLevels{9};    // 16384 .. 64 points
static constexpr uint32_t maxFrames{64};
static constexpr uint32_t maxSourceFrames{512}; // surge's own max_subtables

static_assert(basePoints == (1u << basePosBits));
static_assert((basePoints >> (maxLevels - 1)) == minPoints);
} // namespace wt

/*
 * How a source becomes a table. These stream, so append only.
 *
 * BAND_LIMITED keeps every harmonic the source carries and band limits only down the mip
 * chain, which is there to stop aliasing. Faithful: the table passes through the source
 * samples. A hard step still rings, because a band limited signal through those samples does.
 *
 * BAND_LIMITED_TAPERED rolls the harmonics off with Lanczos sigma factors instead of ending them abruptly.
 * That is the textbook cure for ringing and it works, at the cost of real brightness - the
 * half band point sits about 4dB down.
 *
 * DIRECT skips the transform: the source samples are the table, with Catmull-Rom slopes, and
 * there is no mip chain. It aliases, on purpose, which for 8 bit era material is often the
 * whole point. It is also by far the cheapest - a 2048 point cycle is 16KB a frame.
 *
 * Whether there is a mip chain is a separate question - see the mip chain flag. Turning it off
 * under BAND_LIMITED gives the aliasing and brightness of Direct while still reading a
 * properly reconstructed cycle, rather than a cubic through points 2048 apart. DIRECT has one
 * level by construction, so the flag means nothing for it.
 *
 * DIRECT_ZOH reads the same table DIRECT builds, holding each sample instead of interpolating
 * between them. That is a property of the read rather than of the table, so the two share one
 * build - and it is the crunch of a sampler that never interpolated, which is a different
 * sound from a band limited one, not merely a worse one.
 */
enum struct WavetableBandLimit : uint32_t
{
    BAND_LIMITED = 0,
    BAND_LIMITED_TAPERED = 1,
    DIRECT = 2,
    DIRECT_ZOH = 3
};

/*
 * An immutable built wavetable: a band-limited mip chain per frame, each level holding
 * interleaved (value, slope) float pairs. An unaligned 16 byte load at offset 2*i yields
 * (v[i], dv[i], v[i+1], dv[i+1]) - exactly the cubic Hermite 4-vector, for half the memory
 * the pre-packed SinTable layout needs. Index nPoints repeats index 0 so the top of the
 * table needs no wrap test.
 *
 * Shared across every voice; never mutated after build.
 */
struct Wavetable
{
    struct Level
    {
        uint32_t nPoints{0};
        uint32_t topHarmonic{0};
        uint32_t posShift{0};  // (ph >> posShift) & posMask -> table index
        uint32_t fracShift{0}; // (ph >> fracShift) & 0xFFF -> simdCubic index
        uint32_t posMask{0};
        uint32_t frameStride{0}; // floats per frame, 2 * (nPoints + 1)
        const float *data{nullptr};
    };

    uint64_t hash{0}; // of the source bytes; the dedup key
    std::string name;
    uint16_t nFrames{0};
    uint8_t nLevels{0};
    std::array<Level, wt::maxLevels> levels{};

    std::vector<float> storage; // owns every Level::data above

    size_t memoryBytes() const { return storage.size() * sizeof(float); }
};

/*
 * Points an operator at one level of one frame pair of a Wavetable. Level is chosen per
 * block from the note frequency; morph moves per sample.
 *
 * A single frame table sets both frame pointers to the same frame and the blend weight to
 * zero, so at() stays branch free - the blend is executed and returns the frame bit-exactly
 * rather than being skipped. That matters because MSVC on x86 does not reliably hoist a
 * loop-invariant test out of this loop.
 */
struct WavetableReader
{
    void setTable(const Wavetable *t)
    {
        table = t;
        if (!table || table->nLevels == 0)
        {
            table = nullptr;
            return;
        }
        setLevel(0);
        setMorph(0.f);
    }

    bool valid() const { return table != nullptr; }

    // Hermite by default; ZOH points at a table of (1,0,0,0) so at() holds instead of
    // interpolating, with no branch and the same instruction sequence.
    void setZeroOrderHold(bool z) { coeffs = z ? SinTable::simdZOH : SinTable::simdCubic; }

    void setLevel(uint32_t k)
    {
        if (!table)
            return;
        levelIdx = std::min(k, static_cast<uint32_t>(table->nLevels - 1));
        const auto &l = table->levels[levelIdx];
        posShift = l.posShift;
        fracShift = l.fracShift;
        posMask = l.posMask;
        frameStride = l.frameStride;
        levelBase = l.data;
        refreshFramePointers();
    }

    // Pick the coarsest level whose harmonic content still fits below the engine's Nyquist,
    // from scratch. Used at attack and by tests; per block, prefer updateLevelForFrequency.
    void setLevelForFrequency(float f0, float engineSampleRate)
    {
        if (!table)
            return;
        auto allowed = 0.45f * engineSampleRate / std::max(f0, 0.0001f);
        uint32_t k{0};
        while (k + 1u < table->nLevels && table->levels[k].topHarmonic > allowed)
            ++k;
        setLevel(k);
    }

    /*
     * Per-block level choice, stepping from wherever we already are. Going coarser happens
     * as soon as the current level stops fitting under 0.45 * SR; coming back finer waits
     * until 0.40, so a portamento or a slow pitch envelope sitting on a boundary does not
     * flip back and forth every block. Levels differ only in their top octave of harmonics,
     * so a step is a small discontinuity and needs no crossfade.
     */
    void updateLevelForFrequency(float f0, float engineSampleRate)
    {
        if (!table)
            return;
        auto f = std::max(f0, 0.0001f);
        auto coarser = 0.45f * engineSampleRate / f;
        auto finer = 0.40f * engineSampleRate / f;

        auto k = levelIdx;
        while (k + 1u < table->nLevels && table->levels[k].topHarmonic > coarser)
            ++k;
        while (k > 0 && table->levels[k - 1].topHarmonic <= finer)
            --k;
        if (k != levelIdx)
            setLevel(k);
    }

    uint32_t levelCount() const { return table ? table->nLevels : 0; }

    uint32_t currentLevel() const { return levelIdx; }
    uint32_t currentTopHarmonic() const { return table ? table->levels[levelIdx].topHarmonic : 0; }

    void setMorph(float m)
    {
        if (!table)
            return;
        auto pos = std::clamp(m, 0.f, 1.f) * (table->nFrames - 1);
        auto lo = static_cast<uint32_t>(pos);
        if (lo + 1u >= table->nFrames)
        {
            // sitting on the last frame: blend against itself with weight zero
            frameLoIdx = table->nFrames - 1u;
            frameHiIdx = frameLoIdx;
            blend = 0.f;
        }
        else
        {
            frameLoIdx = lo;
            frameHiIdx = lo + 1u;
            blend = pos - lo;
        }
        refreshFramePointers();
    }

    inline float at(uint32_t ph) const
    {
        auto ub = (ph >> posShift) & posMask;
        auto lb = (ph >> fracShift) & 0xFFFu;

        auto q = SIMD_MM(loadu_ps)(frameLo + 2 * ub);
        auto qh = SIMD_MM(loadu_ps)(frameHi + 2 * ub);
        q = SIMD_MM(add_ps)(q, SIMD_MM(mul_ps)(SIMD_MM(sub_ps)(qh, q), blendV));

        auto r = SIMD_MM(mul_ps)(q, coeffs[lb]);
        auto h = SIMD_MM(hadd_ps)(r, r);
        auto v = SIMD_MM(hadd_ps)(h, h);
        return SIMD_MM(cvtss_f32)(v);
    }

  private:
    void refreshFramePointers()
    {
        if (!levelBase)
            return;
        frameLo = levelBase + static_cast<size_t>(frameLoIdx) * frameStride;
        frameHi = levelBase + static_cast<size_t>(frameHiIdx) * frameStride;
        blendV = SIMD_MM(set1_ps)(blend);
    }

    const SIMD_M128 *coeffs{SinTable::simdCubic};
    const Wavetable *table{nullptr};
    const float *levelBase{nullptr};
    const float *frameLo{nullptr}, *frameHi{nullptr};
    SIMD_M128 blendV alignas(16){};
    uint32_t levelIdx{0};
    uint32_t frameLoIdx{0}, frameHiIdx{0};
    uint32_t posShift{wt::basePosBits}, fracShift{1}, posMask{wt::basePoints - 1};
    uint32_t frameStride{0};
    float blend{0.f};
};
} // namespace baconpaul::six_sines
#endif // BACONPAUL_SIX_SINES_DSP_WAVETABLE_H
