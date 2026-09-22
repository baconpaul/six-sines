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

#include "dsp/wavetable_build.h"

#include <bit>
#include <cmath>
#include <cstring>

#include "pffft.h"

namespace baconpaul::six_sines
{
namespace
{
/*
 * pffft's real transform, ordered, is plain interleaved complex: harmonic k sits at
 * [2k] (re) and [2k+1] (im), k = 0 .. n/2-1, with no Nyquist slot. Forward carries a
 * factor of n. Buffers must be 16 byte aligned, hence pffft_aligned_malloc.
 */
struct RealFFT
{
    explicit RealFFT(uint32_t nn) : n(nn)
    {
        setup = pffft::pffft_new_setup(static_cast<int>(n), pffft::PFFFT_REAL);
        work = static_cast<float *>(pffft::pffft_aligned_malloc(n * sizeof(float)));
        a = static_cast<float *>(pffft::pffft_aligned_malloc(n * sizeof(float)));
        b = static_cast<float *>(pffft::pffft_aligned_malloc(n * sizeof(float)));
    }
    ~RealFFT()
    {
        if (setup)
            pffft::pffft_destroy_setup(setup);
        pffft::pffft_aligned_free(work);
        pffft::pffft_aligned_free(a);
        pffft::pffft_aligned_free(b);
    }
    RealFFT(const RealFFT &) = delete;
    RealFFT &operator=(const RealFFT &) = delete;

    bool ok() const { return setup && work && a && b; }

    void forward() { pffft::pffft_transform_ordered(setup, a, b, work, pffft::PFFFT_FORWARD); }
    void backward() { pffft::pffft_transform_ordered(setup, a, b, work, pffft::PFFFT_BACKWARD); }

    uint32_t n;
    pffft::PFFFT_Setup *setup{nullptr};
    float *work{nullptr};
    float *a{nullptr}; // input
    float *b{nullptr}; // output
};

// highest harmonic a cycle of this length can carry; the missing Nyquist slot costs one
uint32_t usableHarmonics(uint32_t cycleLength) { return cycleLength / 2 - 1; }

/*
 * Lanczos sigma factor. Ending a series abruptly rings; rolling it off with sinc(pi n/(H+1))
 * is the textbook cure. It is not free - the half band point lands about 4dB down - which is
 * why it is a mode rather than the default.
 */
float taper(uint32_t n, uint32_t top, WavetableBandLimit mode)
{
    if (mode != WavetableBandLimit::BAND_LIMITED_TAPERED || n == 0 || top == 0)
        return 1.f;
    auto x = M_PI * (double)n / (top + 1);
    return static_cast<float>(std::sin(x) / x);
}
} // namespace

bool isUsableFFTLength(uint32_t n)
{
    if (n < 32 || (n % 32) != 0)
        return false;
    auto r = n;
    for (auto f : {2u, 3u, 5u})
        while (r % f == 0)
            r /= f;
    return r == 1;
}

std::unique_ptr<HarmonicFrames> analyzeSource(const SourceFrames &src)
{
    if (src.nFrames == 0 || src.nFrames > wt::maxSourceFrames)
        return nullptr;
    if (!isUsableFFTLength(src.cycleLength))
        return nullptr;
    if (src.samples.size() < static_cast<size_t>(src.nFrames) * src.cycleLength)
        return nullptr;

    RealFFT fft(src.cycleLength);
    if (!fft.ok())
        return nullptr;

    auto res = std::make_unique<HarmonicFrames>();
    res->nHarmonics = std::min(wt::baseHarmonics, usableHarmonics(src.cycleLength));
    res->nFrames = src.nFrames;
    res->coeffs.assign(static_cast<size_t>(res->nFrames) * (res->nHarmonics + 1) * 2, 0.f);

    // forward is unnormalized, so fold the 1/L in here and every later stage is in true
    // coefficient units
    auto scale = 1.f / src.cycleLength;
    for (uint32_t f = 0; f < src.nFrames; ++f)
    {
        std::memcpy(fft.a, src.samples.data() + static_cast<size_t>(f) * src.cycleLength,
                    src.cycleLength * sizeof(float));
        fft.forward();
        auto *dst = res->frame(f);
        for (uint32_t k = 0; k <= res->nHarmonics; ++k)
        {
            dst[2 * k] = fft.b[2 * k] * scale;
            dst[2 * k + 1] = fft.b[2 * k + 1] * scale;
        }
    }
    return res;
}

void resampleFrameAxis(HarmonicFrames &hf, uint32_t targetFrames)
{
    if (targetFrames == 0 || hf.nFrames == 0 || hf.nFrames == targetFrames)
        return;

    auto per = (hf.nHarmonics + 1) * 2;
    std::vector<float> out(static_cast<size_t>(targetFrames) * per, 0.f);

    // Lerping coefficients is identical to lerping samples - the transform is linear - so
    // this is the same operation the runtime morph performs, just applied once at load.
    for (uint32_t f = 0; f < targetFrames; ++f)
    {
        double pos = targetFrames == 1 ? 0.0 : 1.0 * f / (targetFrames - 1) * (hf.nFrames - 1);
        auto lo = static_cast<uint32_t>(pos);
        auto hi = std::min(lo + 1u, hf.nFrames - 1u);
        auto fr = static_cast<float>(pos - lo);

        const auto *a = hf.frame(lo);
        const auto *b = hf.frame(hi);
        auto *dst = out.data() + static_cast<size_t>(f) * per;
        for (uint32_t i = 0; i < per; ++i)
            dst[i] = a[i] + (b[i] - a[i]) * fr;
    }

    hf.coeffs = std::move(out);
    hf.nFrames = targetFrames;
}

uint64_t saltHashWithMode(uint64_t hash, WavetableBandLimit mode, bool mipChain)
{
    // ZOH is how the table is read, not how it is built, so it shares Direct's build rather
    // than storing a second identical copy.
    if (mode == WavetableBandLimit::DIRECT_ZOH)
        mode = WavetableBandLimit::DIRECT;
    auto h = hash ^ ((uint64_t)mode * 0x9E3779B97F4A7C15ull);
    return mipChain ? h : (h ^ 0xD1B54A32D192ED03ull);
}

namespace
{
/*
 * Build a Direct table: the source samples are the table, with Catmull-Rom slopes, and no mip
 * chain. Needs a power of two cycle because the reader indexes by bit mask; anything else
 * falls back to the transform path.
 */
std::unique_ptr<Wavetable> buildDirect(const SourceFrames &src, uint64_t hash,
                                       const std::string &name)
{
    auto n = src.cycleLength;
    if (n < wt::minPoints || (n & (n - 1)) != 0 || n > wt::basePoints)
        return nullptr;
    if (src.nFrames == 0 || src.nFrames > wt::maxFrames)
        return nullptr;

    SinTable::initializeStatics();

    auto res = std::make_unique<Wavetable>();
    res->hash = hash;
    res->name = name;
    res->nFrames = static_cast<uint16_t>(src.nFrames);
    res->nLevels = 1;

    auto stride = 2 * (n + 1);
    res->storage.assign(static_cast<size_t>(stride) * src.nFrames, 0.f);

    auto &lvl = res->levels[0];
    auto posBits = static_cast<uint32_t>(std::bit_width(n)) - 1;
    lvl.nPoints = n;
    lvl.topHarmonic = n / 2; // nominal; nothing was removed
    lvl.posShift = phase::phaseBits - posBits;
    lvl.fracShift = lvl.posShift - 12;
    lvl.posMask = n - 1;
    lvl.frameStride = stride;
    lvl.data = res->storage.data();

    for (uint32_t f = 0; f < src.nFrames; ++f)
    {
        const auto *in = src.samples.data() + static_cast<size_t>(f) * n;
        auto *dst = res->storage.data() + static_cast<size_t>(f) * stride;
        for (uint32_t i = 0; i < n; ++i)
        {
            // central difference across the wrap: Hermite with these slopes is Catmull-Rom,
            // which is the classic direct wavetable read
            auto prev = in[(i + n - 1) % n];
            auto next = in[(i + 1) % n];
            dst[2 * i] = in[i];
            dst[2 * i + 1] = 0.5f * (next - prev);
        }
        dst[2 * n] = dst[0];
        dst[2 * n + 1] = dst[1];
    }
    return res;
}
} // namespace

std::unique_ptr<Wavetable> buildWavetable(const HarmonicFrames &hf, uint64_t hash,
                                          const std::string &name, WavetableBandLimit mode,
                                          bool mipChain)
{
    if (hf.nFrames == 0 || hf.nFrames > wt::maxFrames)
        return nullptr;

    // WavetableReader::at() shares SinTable's Hermite coefficient table, so a built table
    // is only readable once those statics exist. Doing it here pins the guarantee to the
    // main thread at load, before any voice can point at the result.
    SinTable::initializeStatics();

    auto res = std::make_unique<Wavetable>();
    res->hash = hash;
    res->name = name;
    res->nFrames = static_cast<uint16_t>(hf.nFrames);

    // Lay out every level's geometry first so storage is one allocation.
    struct LevelPlan
    {
        uint32_t nPoints, topHarmonic, stride;
        size_t offset;
    };
    /*
     * Skip the leading levels that are bigger than this source can fill. A cycle carrying 63
     * harmonics stored at 16384 points is 128x oversampled for nothing; starting at the
     * smallest level that still gives 16 points per period of its top harmonic keeps the
     * quality and drops the memory.
     */
    uint32_t firstLevel{0};
    while (firstLevel + 1 < wt::maxLevels &&
           (wt::baseHarmonics >> (firstLevel + 1)) >= hf.nHarmonics)
        ++firstLevel;

    std::vector<LevelPlan> plan;
    size_t total{0};
    for (uint32_t k = firstLevel; k < wt::maxLevels; ++k)
    {
        auto nPoints = wt::basePoints >> k;
        if (nPoints < wt::minPoints)
            break;
        auto top = wt::baseHarmonics >> k;
        if (top < 1)
            break;
        auto stride = 2 * (nPoints + 1);
        plan.push_back({nPoints, top, stride, total});
        total += static_cast<size_t>(stride) * hf.nFrames;

        // With the chain off there is only the finest level. Stopping here rather than
        // trimming afterwards keeps `total` honest, so storage is sized for what is built.
        if (!mipChain)
            break;
    }
    if (plan.empty())
        return nullptr;

    res->storage.assign(total, 0.f);
    res->nLevels = static_cast<uint8_t>(plan.size());

    for (uint32_t k = 0; k < plan.size(); ++k)
    {
        const auto &p = plan[k];
        RealFFT fft(p.nPoints);
        if (!fft.ok())
            return nullptr;

        // R = 16 guarantees top <= nPoints/16, so DC and the absent Nyquist slot never
        // need special handling and the derivative multiply is always representable.
        auto top = std::min(p.topHarmonic, hf.nHarmonics);
        // derived from this level's own size, since the plan may not start at level 0
        auto posBits = static_cast<uint32_t>(std::bit_width(p.nPoints)) - 1;

        auto &lvl = res->levels[k];
        lvl.nPoints = p.nPoints;
        lvl.topHarmonic = p.topHarmonic;
        lvl.posShift = phase::phaseBits - posBits;
        lvl.fracShift = lvl.posShift - 12;
        lvl.posMask = p.nPoints - 1;
        lvl.frameStride = p.stride;
        lvl.data = res->storage.data() + p.offset;

        for (uint32_t f = 0; f < hf.nFrames; ++f)
        {
            const auto *src = hf.frame(f);
            auto *dst = res->storage.data() + p.offset + static_cast<size_t>(f) * p.stride;

            // values
            std::fill(fft.a, fft.a + p.nPoints, 0.f);
            for (uint32_t kk = 0; kk <= top; ++kk)
            {
                auto w = taper(kk, top, mode);
                fft.a[2 * kk] = src[2 * kk] * w;
                fft.a[2 * kk + 1] = src[2 * kk + 1] * w;
            }
            fft.backward();
            for (uint32_t i = 0; i < p.nPoints; ++i)
                dst[2 * i] = fft.b[i];

            // slopes: d/dx multiplies coefficient kk by j*2*pi*kk, then dv per table index
            // is dv/dx / nPoints
            std::fill(fft.a, fft.a + p.nPoints, 0.f);
            for (uint32_t kk = 1; kk <= top; ++kk)
            {
                auto w = 2.0 * M_PI * kk * taper(kk, top, mode);
                fft.a[2 * kk] = static_cast<float>(-w * src[2 * kk + 1]);
                fft.a[2 * kk + 1] = static_cast<float>(w * src[2 * kk]);
            }
            fft.backward();
            auto dScale = 1.f / p.nPoints;
            for (uint32_t i = 0; i < p.nPoints; ++i)
                dst[2 * i + 1] = fft.b[i] * dScale;

            // wrap point, so the Hermite pair at the last index needs no test
            dst[2 * p.nPoints] = dst[0];
            dst[2 * p.nPoints + 1] = dst[1];
        }
    }

    return res;
}

std::unique_ptr<Wavetable> buildWavetableFromSource(const SourceFrames &src, uint64_t hash,
                                                    const std::string &name,
                                                    WavetableBandLimit mode, bool mipChain)
{
    if (mode == WavetableBandLimit::DIRECT || mode == WavetableBandLimit::DIRECT_ZOH)
    {
        // frame axis first, so Direct honours the same cap as the transform path
        if (src.nFrames > wt::maxFrames)
        {
            SourceFrames cut;
            cut.cycleLength = src.cycleLength;
            cut.nFrames = wt::maxFrames;
            cut.name = src.name;
            cut.samples.resize((size_t)wt::maxFrames * src.cycleLength);
            for (uint32_t f = 0; f < wt::maxFrames; ++f)
            {
                double pos = 1.0 * f / (wt::maxFrames - 1) * (src.nFrames - 1);
                auto lo = (uint32_t)pos;
                auto hi = std::min(lo + 1u, src.nFrames - 1u);
                auto fr = (float)(pos - lo);
                for (uint32_t i = 0; i < src.cycleLength; ++i)
                {
                    auto a = src.samples[(size_t)lo * src.cycleLength + i];
                    auto b = src.samples[(size_t)hi * src.cycleLength + i];
                    cut.samples[(size_t)f * src.cycleLength + i] = a + (b - a) * fr;
                }
            }
            if (auto d = buildDirect(cut, hash, name))
                return d;
        }
        else if (auto d = buildDirect(src, hash, name))
        {
            return d;
        }
        // an odd cycle length cannot be read by bit mask; fall through to the transform
    }

    auto hf = analyzeSource(src);
    if (!hf)
        return nullptr;
    if (hf->nFrames > wt::maxFrames)
        resampleFrameAxis(*hf, wt::maxFrames);
    return buildWavetable(*hf, hash, name, mode, mipChain);
}

std::vector<double> analyzeLevelForTest(const Wavetable &twt, uint8_t level, uint32_t frame)
{
    if (level >= twt.nLevels || frame >= twt.nFrames)
        return {};
    const auto &l = twt.levels[level];

    RealFFT fft(l.nPoints);
    if (!fft.ok())
        return {};

    const auto *src = l.data + static_cast<size_t>(frame) * l.frameStride;
    for (uint32_t i = 0; i < l.nPoints; ++i)
        fft.a[i] = src[2 * i];
    fft.forward();

    std::vector<double> mag(l.nPoints / 2, 0.0);
    auto scale = 1.0 / l.nPoints;
    for (uint32_t k = 0; k < mag.size(); ++k)
    {
        auto re = fft.b[2 * k] * scale;
        auto im = fft.b[2 * k + 1] * scale;
        mag[k] = 2.0 * std::sqrt(re * re + im * im);
    }
    return mag;
}
} // namespace baconpaul::six_sines
