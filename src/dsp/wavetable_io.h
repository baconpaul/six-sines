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

#ifndef BACONPAUL_SIX_SINES_DSP_WAVETABLE_IO_H
#define BACONPAUL_SIX_SINES_DSP_WAVETABLE_IO_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "filesystem/import.h"

#include "dsp/wavetable.h"
#include "dsp/wavetable_build.h"

namespace baconpaul::six_sines
{
// A parsed file, before any band limiting.
struct ParsedWavetable
{
    std::unique_ptr<SourceFrames> samples;
    std::string name;
    std::string error; // human readable; set when there is no payload

    bool ok() const { return samples != nullptr; }
};

/*
 * Sniff the format from the bytes and parse: Surge .wt or a Serum tagged .wav. Never throws
 * and never trusts a declared length over the buffer it was given.
 */
ParsedWavetable parseWavetable(const uint8_t *data, size_t size, const std::string &nameHint);

ParsedWavetable parseSurgeWT(const uint8_t *data, size_t size);
ParsedWavetable parseWav(const uint8_t *data, size_t size);

/*
 * Parse then build. This is the single entry point for both loading a file and rehydrating a
 * patch blob, which is why the patch embeds source bytes rather than anything derived.
 */
std::unique_ptr<Wavetable> buildWavetableFromBytes(
    const uint8_t *data, size_t size, const std::string &nameHint, uint64_t hash,
    WavetableBandLimit mode = WavetableBandLimit::BAND_LIMITED, bool mipChain = true);

// Content hash of a source payload; the dedup key for two operators sharing a table.
uint64_t hashBytes(const uint8_t *data, size_t size);

/*
 * The table an operator gets when it is switched to USER_TABLE with nothing loaded: ten
 * frames walking from a sine to a ten harmonic saw. Real .wt bytes rather than a special
 * case, so it is parsed and built by exactly the same code as a file off disk - and being
 * built in, a patch using it needs no blob, which is why selecting a wavetable on a fresh
 * operator costs the patch nothing.
 */
const std::vector<uint8_t> &defaultWavetableBytes();
uint64_t defaultWavetableHash();

/*
 * Wavetable libraries belonging to other synths installed on this machine, as
 * (menu label, directory) pairs in the order they should be offered. Empty when none are
 * present, and any one of them missing simply means that entry is absent.
 */
struct FactoryWavetableLibrary
{
    std::string vendor; // the submenu this belongs under
    std::string label;  // the entry within it
    fs::path path;
};
std::vector<FactoryWavetableLibrary> factoryWavetableLibraries();

std::string base64Encode(const uint8_t *data, size_t size);
bool base64Decode(const std::string &, std::vector<uint8_t> &out);

// Raw deflate, for embedding source bytes in a patch. The XML container cannot carry
// arbitrary bytes - NUL, "]]>" and invalid UTF-8 all break it - so a blob is deflated and
// then base64'd, which costs a further 4/3.
std::vector<uint8_t> deflateBytes(const uint8_t *data, size_t size);
bool inflateBytes(const uint8_t *data, size_t size, size_t expectedSize,
                  std::vector<uint8_t> &out);
} // namespace baconpaul::six_sines
#endif // BACONPAUL_SIX_SINES_DSP_WAVETABLE_IO_H
