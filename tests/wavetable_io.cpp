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
 * Format parsing. The bytes are synthesised here rather than checked in, so these tests
 * pin the spec rather than one file someone happened to have. Real Surge and Serum files
 * are a separate obligation - see the plan - since the interesting failures live in what
 * exporters actually emit.
 *
 * The .wt header and the .wav chunk priority chain both follow Surge:
 *   surge/src/common/dsp/Wavetable.h        wt_header and its flags
 *   surge/src/common/WAVFileSupport.cpp     clm / uhWT / cue / srge / smpl
 */

#include "catch2/catch2.hpp"

#include <algorithm>
#include <filesystem>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "dsp/wavetable.h"
#include "dsp/wavetable_build.h"
#include "dsp/wavetable_io.h"

using namespace baconpaul::six_sines;

namespace
{
void put32(std::vector<uint8_t> &v, uint32_t x)
{
    for (int i = 0; i < 4; ++i)
        v.push_back((x >> (8 * i)) & 0xFF);
}
void put16(std::vector<uint8_t> &v, uint16_t x)
{
    v.push_back(x & 0xFF);
    v.push_back((x >> 8) & 0xFF);
}
void putTag(std::vector<uint8_t> &v, const char *t)
{
    for (int i = 0; i < 4; ++i)
        v.push_back((uint8_t)t[i]);
}
void putF32(std::vector<uint8_t> &v, float f)
{
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    put32(v, bits);
}

float frameSample(int frame, uint32_t i, uint32_t n)
{
    return static_cast<float>(std::sin(2 * M_PI * (frame + 1) * i / n));
}

// A Surge .wt. flags: 4 = int16, 8 = int16 full range, 1 = is_sample
std::vector<uint8_t> makeWT(uint32_t nSamples, uint16_t nTables, uint16_t flags)
{
    std::vector<uint8_t> v;
    putTag(v, "vawt");
    put32(v, nSamples);
    put16(v, nTables);
    put16(v, flags);
    for (int t = 0; t < nTables; ++t)
        for (uint32_t i = 0; i < nSamples; ++i)
        {
            auto s = frameSample(t, i, nSamples);
            if (flags & 4)
            {
                // scales per surge/src/common/dsp/vembertech/basic_dsp.h: i15 is 1/16384,
                // i16 is 1/32768. Clamp like a real encoder - 32768 is not an int16.
                auto q = std::lround(s * ((flags & 8) ? 32768.0 : 16384.0));
                q = std::max<long>(-32768, std::min<long>(32767, q));
                put16(v, (uint16_t)(int16_t)q);
            }
            else
                putF32(v, s);
        }
    return v;
}

struct WavSpec
{
    uint32_t cycleLength{2048};
    uint32_t nFrames{4};
    uint16_t channels{1};
    uint16_t bits{32};
    bool isFloat{true};
    std::string hintChunk; // "clm ", "uhWT", "srge", "smpl", or empty
    uint32_t hintValue{2048};
};

std::vector<uint8_t> makeWav(const WavSpec &sp)
{
    std::vector<uint8_t> extra;
    if (sp.hintChunk == "clm ")
    {
        auto text = "<!>" + std::to_string(sp.hintValue) + " 10000000 wavetable (Serum)";
        putTag(extra, "clm ");
        put32(extra, (uint32_t)text.size());
        for (char c : text)
            extra.push_back((uint8_t)c);
        if (text.size() & 1)
            extra.push_back(0);
    }
    else if (sp.hintChunk == "uhWT")
    {
        putTag(extra, "uhWT");
        put32(extra, 4);
        put32(extra, 0);
    }
    else if (sp.hintChunk == "srge")
    {
        putTag(extra, "srge");
        put32(extra, 8);
        put32(extra, 1); // version
        put32(extra, sp.hintValue);
    }

    std::vector<uint8_t> data;
    auto total = sp.cycleLength * sp.nFrames;
    for (uint32_t i = 0; i < total; ++i)
    {
        auto s = frameSample((int)(i / sp.cycleLength), i % sp.cycleLength, sp.cycleLength);
        for (uint16_t c = 0; c < sp.channels; ++c)
        {
            // put something different in channel 1 so "takes channel 0" is testable
            auto v = (c == 0) ? s : -0.5f * s;
            if (sp.isFloat)
                putF32(data, v);
            else
                put16(data, (uint16_t)(int16_t)std::lround(v * 32767.0));
        }
    }

    std::vector<uint8_t> fmt;
    put16(fmt, sp.isFloat ? 3 : 1); // 3 = IEEE float, 1 = PCM
    put16(fmt, sp.channels);
    put32(fmt, 44100);
    put32(fmt, 44100 * sp.channels * sp.bits / 8);
    put16(fmt, sp.channels * sp.bits / 8);
    put16(fmt, sp.bits);

    std::vector<uint8_t> body;
    putTag(body, "WAVE");
    putTag(body, "fmt ");
    put32(body, (uint32_t)fmt.size());
    body.insert(body.end(), fmt.begin(), fmt.end());
    body.insert(body.end(), extra.begin(), extra.end());
    putTag(body, "data");
    put32(body, (uint32_t)data.size());
    body.insert(body.end(), data.begin(), data.end());

    std::vector<uint8_t> v;
    putTag(v, "RIFF");
    put32(v, (uint32_t)body.size());
    v.insert(v.end(), body.begin(), body.end());
    return v;
}

std::vector<uint8_t> bytesOf(const std::string &s)
{
    return std::vector<uint8_t>(s.begin(), s.end());
}

ParsedWavetable parse(const std::vector<uint8_t> &v)
{
    return parseWavetable(v.data(), v.size(), "test");
}
} // namespace

TEST_CASE("A float32 Surge wt parses", "[wavetable][io]")
{
    auto p = parse(makeWT(1024, 4, 0));
    REQUIRE(p.ok());
    REQUIRE(p.error.empty());
    REQUIRE(p.samples);
    REQUIRE(p.samples->cycleLength == 1024);
    REQUIRE(p.samples->nFrames == 4);
    for (uint32_t f = 0; f < 4; ++f)
        for (uint32_t i : {0u, 1u, 511u, 1023u})
            REQUIRE_THAT(p.samples->samples[f * 1024 + i],
                         Catch::Matchers::WithinAbs(frameSample(f, i, 1024), 1e-6));
}

TEST_CASE("An int16 Surge wt parses and scales", "[wavetable][io]")
{
    auto p = parse(makeWT(512, 2, 4 | 8));
    REQUIRE(p.ok());
    REQUIRE(p.samples->cycleLength == 512);
    REQUIRE(p.samples->nFrames == 2);
    for (uint32_t i : {0u, 63u, 128u, 400u})
        REQUIRE_THAT(p.samples->samples[i],
                     Catch::Matchers::WithinAbs(frameSample(0, i, 512), 1e-3));
}

TEST_CASE("A Surge wt flagged as a sample is refused", "[wavetable][io]")
{
    auto p = parse(makeWT(1024, 4, 1));
    REQUIRE(!p.ok());
    REQUIRE(!p.error.empty());
}

TEST_CASE("A truncated Surge wt is refused", "[wavetable][io]")
{
    auto v = makeWT(1024, 4, 0);
    v.resize(v.size() / 2);
    auto p = parse(v);
    REQUIRE(!p.ok());
}

TEST_CASE("A Surge wt claiming more frames than Surge allows is refused", "[wavetable][io]")
{
    std::vector<uint8_t> v;
    putTag(v, "vawt");
    put32(v, 2048);
    put16(v, 4096); // way past max_subtables
    put16(v, 0);
    auto p = parse(v);
    REQUIRE(!p.ok());
}

TEST_CASE("A clm chunk gives the frame size", "[wavetable][io]")
{
    WavSpec sp;
    sp.hintChunk = "clm ";
    sp.hintValue = 2048;
    auto p = parse(makeWav(sp));
    REQUIRE(p.ok());
    REQUIRE(p.samples->cycleLength == 2048);
    REQUIRE(p.samples->nFrames == 4);
}

TEST_CASE("A clm chunk declaring something other than 2048 is honoured", "[wavetable][io]")
{
    // Surge only recognises a literal 2048 here; we parse the integer generally
    WavSpec sp;
    sp.cycleLength = 1024;
    sp.nFrames = 8;
    sp.hintChunk = "clm ";
    sp.hintValue = 1024;
    auto p = parse(makeWav(sp));
    REQUIRE(p.ok());
    REQUIRE(p.samples->cycleLength == 1024);
    REQUIRE(p.samples->nFrames == 8);
}

TEST_CASE("A uhWT chunk implies 2048", "[wavetable][io]")
{
    WavSpec sp;
    sp.hintChunk = "uhWT";
    auto p = parse(makeWav(sp));
    REQUIRE(p.ok());
    REQUIRE(p.samples->cycleLength == 2048);
}

TEST_CASE("An srge chunk gives the frame size", "[wavetable][io]")
{
    WavSpec sp;
    sp.cycleLength = 512;
    sp.nFrames = 6;
    sp.hintChunk = "srge";
    sp.hintValue = 512;
    auto p = parse(makeWav(sp));
    REQUIRE(p.ok());
    REQUIRE(p.samples->cycleLength == 512);
    REQUIRE(p.samples->nFrames == 6);
}

TEST_CASE("With no hint the frame size is guessed as 2048", "[wavetable][io]")
{
    WavSpec sp;
    sp.hintChunk = "";
    auto p = parse(makeWav(sp));
    REQUIRE(p.ok());
    REQUIRE(p.samples->cycleLength == 2048);
}

TEST_CASE("An int16 wav parses", "[wavetable][io]")
{
    WavSpec sp;
    sp.isFloat = false;
    sp.bits = 16;
    sp.hintChunk = "clm ";
    auto p = parse(makeWav(sp));
    REQUIRE(p.ok());
    REQUIRE(p.samples->cycleLength == 2048);
    for (uint32_t i : {0u, 100u, 1000u})
        REQUIRE_THAT(p.samples->samples[i],
                     Catch::Matchers::WithinAbs(frameSample(0, i, 2048), 1e-3));
}

TEST_CASE("A stereo wav takes channel zero", "[wavetable][io]")
{
    WavSpec sp;
    sp.channels = 2;
    sp.hintChunk = "clm ";
    auto p = parse(makeWav(sp));
    REQUIRE(p.ok());
    for (uint32_t i : {10u, 500u, 1500u})
        REQUIRE_THAT(p.samples->samples[i],
                     Catch::Matchers::WithinAbs(frameSample(0, i, 2048), 1e-6));
}

TEST_CASE("Garbage is refused with a message", "[wavetable][io]")
{
    auto p = parse(bytesOf("this is not a wavetable at all, not even close"));
    REQUIRE(!p.ok());
    REQUIRE(!p.error.empty());
}

TEST_CASE("An empty buffer is refused", "[wavetable][io]")
{
    std::vector<uint8_t> v;
    auto p = parse(v);
    REQUIRE(!p.ok());
}

TEST_CASE("Base64 round trips arbitrary bytes", "[wavetable][io]")
{
    for (size_t n : {size_t{0}, size_t{1}, size_t{2}, size_t{3}, size_t{255}, size_t{1024}})
    {
        std::vector<uint8_t> in(n);
        for (size_t i = 0; i < n; ++i)
            in[i] = (uint8_t)((i * 37 + 11) & 0xFF);
        auto enc = base64Encode(in.data(), in.size());
        std::vector<uint8_t> out;
        INFO("n = " << n);
        REQUIRE(base64Decode(enc, out));
        REQUIRE(out == in);
    }
}

TEST_CASE("Base64 rejects junk", "[wavetable][io]")
{
    std::vector<uint8_t> out;
    REQUIRE(!base64Decode("!!!not base64!!!", out));
}

TEST_CASE("Bytes go straight to a built wavetable", "[wavetable][io]")
{
    // this is the whole point of routing patch load and file load through one function
    auto v = makeWT(2048, 8, 0);
    auto wt = buildWavetableFromBytes(v.data(), v.size(), "eight", 0x1234);
    REQUIRE(wt);
    REQUIRE(wt->nFrames == 8);
    REQUIRE(wt->hash == 0x1234);
    REQUIRE(wt->nLevels > 1);
}

TEST_CASE("Unparseable bytes give no wavetable", "[wavetable][io]")
{
    auto v = bytesOf("nope");
    REQUIRE(buildWavetableFromBytes(v.data(), v.size(), "nope", 1) == nullptr);
}


TEST_CASE("Factory wavetable discovery is safe with or without the synths installed",
          "[wavetable][io]")
{
    // Whatever it reports has to be real, since the menu will try to walk it. On a machine
    // with none of these synths the contract is simply that it says so quietly.
    auto libs = factoryWavetableLibraries();
    for (const auto &l : libs)
    {
        INFO("found " << l.vendor << " / " << l.label << " at " << l.path);
        REQUIRE(!l.vendor.empty());
        REQUIRE(!l.label.empty());
        REQUIRE(!l.path.empty());
        REQUIRE(std::filesystem::is_directory(l.path));
    }
    if (libs.empty())
        WARN("no wavetable libraries on this machine; the factory menu will not appear");
}

TEST_CASE("The two Surge int16 scalings are distinguished", "[wavetable][io]")
{
    // wtf_int16 alone is i15 at 1/16384; with wtf_int16_is_16 it is i16 at 1/32768. Getting
    // this wrong is a clean factor of two, and only shows on the i15 form - which is why the
    // i16 test alone did not cover it.
    auto i15 = parse(makeWT(512, 1, 4));
    auto i16 = parse(makeWT(512, 1, 4 | 8));
    REQUIRE(i15.ok());
    REQUIRE(i16.ok());

    for (uint32_t i : {5u, 64u, 200u, 400u})
    {
        INFO("sample " << i);
        auto expect = frameSample(0, i, 512);
        REQUIRE_THAT(i15.samples->samples[i], Catch::Matchers::WithinAbs(expect, 2e-3));
        REQUIRE_THAT(i16.samples->samples[i], Catch::Matchers::WithinAbs(expect, 2e-3));
    }
}
