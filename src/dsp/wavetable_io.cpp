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

#include "dsp/wavetable_io.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <cstdlib>
#include <vector>

#include "miniz.h"

#include "sst/plugininfra/paths.h"
#include "filesystem/import.h"

namespace baconpaul::six_sines
{
namespace
{
ParsedWavetable fail(const std::string &why)
{
    ParsedWavetable r;
    r.error = why;
    return r;
}

// Little endian readers that never walk past `end`.
bool rd32(const uint8_t *&p, const uint8_t *end, uint32_t &out)
{
    if (end - p < 4)
        return false;
    out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    p += 4;
    return true;
}
bool rd16(const uint8_t *&p, const uint8_t *end, uint16_t &out)
{
    if (end - p < 2)
        return false;
    out = (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
    p += 2;
    return true;
}
bool tagIs(const uint8_t *p, const char *t) { return std::memcmp(p, t, 4) == 0; }

float readSample(const uint8_t *p, uint16_t bits, bool isFloat)
{
    if (isFloat)
    {
        if (bits == 64)
        {
            double d;
            std::memcpy(&d, p, 8);
            return (float)d;
        }
        float f;
        std::memcpy(&f, p, 4);
        return f;
    }
    if (bits == 8)
        return ((int)p[0] - 128) / 128.f;
    if (bits == 16)
    {
        auto v = (int16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
        return v / 32768.f;
    }
    if (bits == 24)
    {
        int32_t v = ((int32_t)p[0] << 8) | ((int32_t)p[1] << 16) | ((int32_t)p[2] << 24);
        return (v >> 8) / 8388608.f;
    }
    if (bits == 32)
    {
        int32_t v;
        std::memcpy(&v, p, 4);
        return v / 2147483648.f;
    }
    return 0.f;
}

// Split an interleaved mono/stereo run into frame-major single cycles, channel 0 only.
ParsedWavetable sliceFrames(const std::vector<float> &mono, uint32_t cycleLength,
                            const std::string &name)
{
    if (cycleLength < 2)
        return fail("wavetable frame size is too small");
    auto nFrames = (uint32_t)(mono.size() / cycleLength);
    if (nFrames == 0)
        return fail("wavetable has no complete frame of " + std::to_string(cycleLength) +
                    " samples");
    if (nFrames > wt::maxSourceFrames)
        return fail("wavetable declares " + std::to_string(nFrames) + " frames, more than the " +
                    std::to_string(wt::maxSourceFrames) + " supported");

    ParsedWavetable r;
    r.name = name;
    r.samples = std::make_unique<SourceFrames>();
    r.samples->cycleLength = cycleLength;
    r.samples->nFrames = nFrames;
    r.samples->name = name;
    r.samples->samples.assign(mono.begin(), mono.begin() + (size_t)nFrames * cycleLength);
    return r;
}
} // namespace

bool base64Decode(const std::string &in, std::vector<uint8_t> &out)
{
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int8_t rev[256];
    std::fill(std::begin(rev), std::end(rev), (int8_t)-1);
    for (int i = 0; i < 64; ++i)
        rev[(uint8_t)alphabet[i]] = (int8_t)i;

    out.clear();
    uint32_t acc{0};
    int bits{0};
    for (char c : in)
    {
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t')
            continue;
        if (c == '=')
            break;
        auto d = rev[(uint8_t)c];
        if (d < 0)
            return false;
        acc = (acc << 6) | (uint32_t)d;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back((uint8_t)((acc >> bits) & 0xFF));
        }
    }
    return true;
}

std::string base64Encode(const uint8_t *data, size_t size)
{
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((size + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < size; i += 3)
    {
        uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
        out += alphabet[(v >> 18) & 63];
        out += alphabet[(v >> 12) & 63];
        out += alphabet[(v >> 6) & 63];
        out += alphabet[v & 63];
    }
    if (i < size)
    {
        uint32_t v = (uint32_t)data[i] << 16;
        bool two = (i + 1 < size);
        if (two)
            v |= (uint32_t)data[i + 1] << 8;
        out += alphabet[(v >> 18) & 63];
        out += alphabet[(v >> 12) & 63];
        out += two ? alphabet[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

std::vector<uint8_t> deflateBytes(const uint8_t *data, size_t size)
{
    if (!data || size == 0)
        return {};
    auto bound = compressBound((mz_ulong)size);
    std::vector<uint8_t> out(bound);
    mz_ulong got{bound};
    if (compress2(out.data(), &got, data, (mz_ulong)size, MZ_BEST_COMPRESSION) != MZ_OK)
        return {};
    out.resize(got);
    return out;
}

bool inflateBytes(const uint8_t *data, size_t size, size_t expectedSize,
                  std::vector<uint8_t> &out)
{
    if (!data || size == 0 || expectedSize == 0)
        return false;
    out.assign(expectedSize, 0);
    mz_ulong got{(mz_ulong)expectedSize};
    if (uncompress(out.data(), &got, data, (mz_ulong)size) != MZ_OK)
    {
        out.clear();
        return false;
    }
    out.resize(got);
    return true;
}

std::vector<FactoryWavetableLibrary> factoryWavetableLibraries()
{
    std::vector<FactoryWavetableLibrary> res;

    auto addIfDir = [&res](const std::string &vendor, const std::string &label, const fs::path &p)
    {
        try
        {
            if (!p.empty() && fs::is_directory(p))
                res.push_back({vendor, label, p});
        }
        catch (const fs::filesystem_error &)
        {
            // an unreadable or vanished volume is simply not a library
        }
    };

    /*
     * Surge resolves its own data path through sst::plugininfra::paths, so use the same call
     * rather than guessing - it tracks whatever Surge does across platforms. A per-user
     * install wins over the shared one, which is the order Surge itself prefers.
     */
    try
    {
        static const std::string sxt{"Surge XT"};
        for (auto userLocal : {true, false})
        {
            auto root = sst::plugininfra::paths::bestLibrarySharedFolderPathFor(sxt, userLocal);
            if (root.empty() || !fs::is_directory(root))
                continue;
            auto before = res.size();
            addIfDir("Surge XT", "Factory", root / "wavetables");
            addIfDir("Surge XT", "Third Party", root / "wavetables_3rdparty");
            if (res.size() != before)
                break;
        }
    }
    catch (const fs::filesystem_error &)
    {
    }

    /*
     * Serum puts its tables in a fixed place rather than anywhere we can ask a library about,
     * and the vendor folder is spelled differently per platform: "Xfer Records" under the
     * shared presets folder on macOS, "Xfer" under the user's Documents on Windows. Linux is
     * absent rather than guessed.
     */
#if defined(__APPLE__)
    {
        fs::path xfer{"/Library/Audio/Presets/Xfer Records"};
        addIfDir("Serum 2", "Tables", xfer / "Serum 2 Presets" / "Tables");
        addIfDir("Serum", "Tables", xfer / "Serum Presets" / "Tables");
    }
#elif defined(_WIN32)
    {
        // through plugininfra so this is the real Documents folder rather than an assumed
        // one under the user profile. Serum 2 follows the Serum layout; if it ever does not,
        // the folder simply is not there and the entry does not appear.
        namespace paths = sst::plugininfra::paths;
        addIfDir("Serum 2", "Tables",
                 paths::bestDocumentsVendorFolderPathFor("Xfer", "Serum 2 Presets") / "Tables");
        addIfDir("Serum", "Tables",
                 paths::bestDocumentsVendorFolderPathFor("Xfer", "Serum Presets") / "Tables");
    }
#endif

    /*
     * Bitwig ships .wt files - the same format Surge writes - inside its sound packages, laid
     * out as installed-packages/<version>/<vendor>/<package>/Wavetables. The version and
     * package names are not ours to predict, so find the Wavetables folders rather than
     * spelling out a path to one, and prefer the newest version when a package appears in
     * several.
     */
    {
        std::vector<fs::path> roots;
        auto env = [](const char *n) -> fs::path
        {
            auto v = std::getenv(n);
            return v ? fs::path(v) : fs::path{};
        };
        auto home = env("HOME");
#if defined(__APPLE__)
        if (!home.empty())
            roots.push_back(home / "Library" / "Application Support" / "Bitwig" / "Bitwig Studio" /
                            "installed-packages");
#elif defined(_WIN32)
        if (auto lad = env("LOCALAPPDATA"); !lad.empty())
            roots.push_back(lad / "Bitwig Studio" / "installed-packages");
#else
        if (!home.empty())
            roots.push_back(home / ".BitwigStudio" / "installed-packages");
#endif

        std::set<std::string> seenPackage;
        for (const auto &root : roots)
        {
            try
            {
                if (!fs::is_directory(root))
                    continue;

                std::vector<fs::path> versions;
                for (const auto &v : fs::directory_iterator(root))
                    if (v.is_directory())
                        versions.push_back(v.path());
                // newest first, so an older copy of the same package loses
                std::sort(versions.rbegin(), versions.rend());

                for (const auto &ver : versions)
                    for (const auto &vendor : fs::directory_iterator(ver))
                    {
                        if (!vendor.is_directory())
                            continue;
                        for (const auto &pkg : fs::directory_iterator(vendor.path()))
                        {
                            if (!pkg.is_directory())
                                continue;
                            auto wt = pkg.path() / "Wavetables";
                            if (!fs::is_directory(wt))
                                continue;
                            auto name = pkg.path().filename().u8string();
                            if (seenPackage.insert(name).second)
                                addIfDir("Bitwig", name, wt);
                        }
                    }
            }
            catch (const fs::filesystem_error &)
            {
            }
        }
    }

    return res;
}

const std::vector<uint8_t> &defaultWavetableBytes()
{
    static const std::vector<uint8_t> bytes = []()
    {
        static constexpr uint32_t nSamples{2048};
        static constexpr uint16_t nTables{10};
        // wtf_int16 | wtf_int16_is_16: half the size of float32 and it deflates better, and
        // 16 bits is well past what a ten harmonic waveform needs.
        static constexpr uint16_t flags{4 | 8};

        std::vector<uint8_t> v;
        auto put32 = [&v](uint32_t x)
        {
            for (int i = 0; i < 4; ++i)
                v.push_back((x >> (8 * i)) & 0xFF);
        };
        auto put16 = [&v](uint16_t x)
        {
            v.push_back(x & 0xFF);
            v.push_back((x >> 8) & 0xFF);
        };
        for (char ch : std::string("vawt"))
            v.push_back((uint8_t)ch);
        put32(nSamples);
        put16(nTables);
        put16(flags);

        for (uint16_t t = 0; t < nTables; ++t)
        {
            // frame t is the saw series truncated to t+1 harmonics, so frame 0 is a sine
            std::vector<double> f(nSamples, 0.0);
            double peak{0};
            for (uint32_t i = 0; i < nSamples; ++i)
            {
                double x = 1.0 * i / nSamples;
                for (uint32_t n = 1; n <= (uint32_t)t + 1; ++n)
                    f[i] += std::sin(2 * M_PI * n * x) / n;
                peak = std::max(peak, std::abs(f[i]));
            }
            // per frame rather than across the table, so morphing does not also sweep level
            auto norm = peak > 0 ? 0.99 / peak : 1.0;
            for (uint32_t i = 0; i < nSamples; ++i)
            {
                auto q = (long)std::lround(f[i] * norm * 32768.0);
                q = std::max<long>(-32768, std::min<long>(32767, q));
                put16((uint16_t)(int16_t)q);
            }
        }
        return v;
    }();
    return bytes;
}

uint64_t defaultWavetableHash()
{
    static const uint64_t h =
        hashBytes(defaultWavetableBytes().data(), defaultWavetableBytes().size());
    return h;
}

uint64_t hashBytes(const uint8_t *data, size_t size)
{
    // fnv-1a; we need a stable dedup key, not a cryptographic digest
    uint64_t h{0xcbf29ce484222325ull};
    for (size_t i = 0; i < size; ++i)
    {
        h ^= data[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

ParsedWavetable parseSurgeWT(const uint8_t *data, size_t size)
{
    // wt_header, packed: char tag[4]; uint32 n_samples; uint16 n_tables; uint16 flags
    // flags per surge/src/common/dsp/Wavetable.h
    static constexpr uint16_t wtf_is_sample = 1;
    static constexpr uint16_t wtf_int16 = 4;
    static constexpr uint16_t wtf_int16_is_16 = 8;

    auto *p = data;
    auto *end = data + size;
    if (size < 12 || !tagIs(p, "vawt"))
        return fail("not a Surge wavetable");
    p += 4;

    uint32_t nSamples{0};
    uint16_t nTables{0}, flags{0};
    if (!rd32(p, end, nSamples) || !rd16(p, end, nTables) || !rd16(p, end, flags))
        return fail("Surge wavetable header is truncated");

    if (flags & wtf_is_sample)
        return fail("that .wt holds a sample, not a wavetable");
    if (nSamples < 2 || nTables == 0)
        return fail("Surge wavetable declares no content");
    if (nTables > wt::maxSourceFrames)
        return fail("Surge wavetable declares " + std::to_string(nTables) +
                    " frames, more than the " + std::to_string(wt::maxSourceFrames) + " supported");

    auto bytesPer = (flags & wtf_int16) ? 2u : 4u;
    auto need = (size_t)nSamples * nTables * bytesPer;
    if ((size_t)(end - p) < need)
        return fail("Surge wavetable is truncated: wanted " + std::to_string(need) +
                    " bytes of samples, found " + std::to_string(end - p));

    std::vector<float> mono((size_t)nSamples * nTables);
    auto scale = (flags & wtf_int16_is_16) ? 1.f / 32768.f : 1.f / 16384.f;
    for (size_t i = 0; i < mono.size(); ++i)
    {
        if (flags & wtf_int16)
        {
            auto v = (int16_t)((uint32_t)p[2 * i] | ((uint32_t)p[2 * i + 1] << 8));
            mono[i] = v * scale;
        }
        else
        {
            std::memcpy(&mono[i], p + 4 * i, 4);
        }
    }
    return sliceFrames(mono, nSamples, "");
}

ParsedWavetable parseWav(const uint8_t *data, size_t size)
{
    auto *end = data + size;
    if (size < 12 || !tagIs(data, "RIFF") || !tagIs(data + 8, "WAVE"))
        return fail("not a RIFF WAVE file");

    uint16_t format{0}, channels{0}, bits{0};
    const uint8_t *samples{nullptr};
    size_t sampleBytes{0};

    /*
     * Frame size hints, in the priority order Surge settled on after a lot of real files:
     * clm / uhWT beat cue, which beats srge, which beats smpl. See
     * surge/src/common/WAVFileSupport.cpp.
     */
    uint32_t clmLen{0}, cueLen{0}, srgeLen{0}, smplLen{0};

    auto *p = data + 12;
    while (end - p >= 8)
    {
        const auto *tag = p;
        uint32_t cs{0};
        auto *q = p + 4;
        if (!rd32(q, end, cs))
            break;
        if ((size_t)(end - q) < cs)
            cs = (uint32_t)(end - q); // tolerate a short final chunk rather than bail

        if (tagIs(tag, "fmt ") && cs >= 16)
        {
            auto *f = q;
            rd16(f, end, format);
            rd16(f, end, channels);
            f += 4 + 4 + 2; // sample rate, byte rate, block align
            uint16_t b{0};
            rd16(f, end, b);
            bits = b;
        }
        else if (tagIs(tag, "data"))
        {
            samples = q;
            sampleBytes = cs;
        }
        else if (tagIs(tag, "clm ") && cs >= 4)
        {
            // "<!>2048 10000000 ...". Surge only recognises a literal 2048; parse the
            // integer generally so a table authored at another size still loads.
            std::string s((const char *)q, std::min<size_t>(cs, 32));
            auto at = s.find("<!>");
            if (at != std::string::npos)
            {
                auto n = std::strtoul(s.c_str() + at + 3, nullptr, 10);
                if (n >= 2 && n <= 65536)
                    clmLen = (uint32_t)n;
            }
        }
        else if (tagIs(tag, "uhWT"))
        {
            clmLen = 2048; // Hive metadata; treat exactly like clm
        }
        else if ((tagIs(tag, "srge") || tagIs(tag, "srgo")) && cs >= 8)
        {
            auto *f = q + 4; // skip version
            uint32_t n{0};
            if (rd32(f, end, n) && n >= 2 && n <= 65536)
                srgeLen = n;
        }
        else if (tagIs(tag, "cue ") && cs >= 4)
        {
            // trust the chunk size over the count the chunk declares; they disagree in
            // the wild
            auto declared = (uint32_t)((uint32_t)q[0] | ((uint32_t)q[1] << 8) |
                                       ((uint32_t)q[2] << 16) | ((uint32_t)q[3] << 24));
            auto inChunk = (cs - 4) / 24;
            auto nCues = std::min<uint32_t>(declared, inChunk);
            if (nCues >= 2)
            {
                // cue point positions are at offset 4+20 within each 24 byte record
                auto posOf = [&](uint32_t i) -> uint32_t
                {
                    auto *r = q + 4 + (size_t)i * 24 + 20;
                    return (uint32_t)r[0] | ((uint32_t)r[1] << 8) | ((uint32_t)r[2] << 16) |
                           ((uint32_t)r[3] << 24);
                };
                auto d = posOf(1) > posOf(0) ? posOf(1) - posOf(0) : 0;
                if (d >= 2 && d <= 65536)
                    cueLen = d;
            }
        }
        else if (tagIs(tag, "smpl"))
        {
            // an empty smpl block is RAPID's way of saying 2048
            smplLen = 2048;
        }

        p = q + cs + (cs & 1); // chunks are word aligned
    }

    if (!samples || sampleBytes == 0)
        return fail("WAVE file has no data chunk");
    bool isFloat = (format == 3);
    if (format != 1 && format != 3)
        return fail("unsupported WAVE encoding " + std::to_string(format));
    if (channels == 0 || bits == 0 || (bits % 8) != 0)
        return fail("WAVE file has no usable format chunk");

    auto bytesPer = (size_t)bits / 8;
    auto stride = bytesPer * channels;
    auto nSamples = sampleBytes / stride;
    if (nSamples < 2)
        return fail("WAVE file is too short");

    std::vector<float> mono(nSamples);
    for (size_t i = 0; i < nSamples; ++i)
        mono[i] = readSample(samples + i * stride, bits, isFloat);

    uint32_t cycleLength = clmLen    ? clmLen
                           : cueLen  ? cueLen
                           : srgeLen ? srgeLen
                           : smplLen ? smplLen
                                     : 0;
    if (cycleLength == 0)
    {
        // no hint at all: prefer 2048, then other common powers of two that divide evenly
        for (uint32_t c : {2048u, 4096u, 1024u, 512u, 256u, 128u})
            if (nSamples % c == 0)
            {
                cycleLength = c;
                break;
            }
    }
    if (cycleLength == 0)
        return fail("cannot tell the frame size of that WAVE file; no clm, cue, srge or smpl "
                    "chunk and its length is not a multiple of a common frame size");
    if (nSamples < cycleLength)
        return fail("WAVE file is shorter than the frame size it declares");

    return sliceFrames(mono, cycleLength, "");
}

ParsedWavetable parseWavetable(const uint8_t *data, size_t size, const std::string &nameHint)
{
    if (!data || size < 4)
        return fail("that file is empty or too short to be a wavetable");

    ParsedWavetable r;
    if (tagIs(data, "vawt"))
        r = parseSurgeWT(data, size);
    else if (tagIs(data, "RIFF"))
        r = parseWav(data, size);
    else
        r = fail("that file is not a Surge .wt or a WAVE file");

    if (r.ok() && r.name.empty())
    {
        r.name = nameHint;
        if (r.samples)
            r.samples->name = nameHint;
    }
    return r;
}

std::unique_ptr<Wavetable> buildWavetableFromBytes(const uint8_t *data, size_t size,
                                                   const std::string &nameHint, uint64_t hash,
                                                   WavetableBandLimit mode, bool mipChain)
{
    auto p = parseWavetable(data, size, nameHint);
    if (!p.ok())
        return nullptr;

    return buildWavetableFromSource(*p.samples, hash, p.name, mode, mipChain);
}
} // namespace baconpaul::six_sines
