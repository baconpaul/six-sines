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
 * USER_TABLE was inserted before AUDIO_IN, which moved AUDIO_IN's streamed integer from
 * 21 to 22. structure.cpp pins parameter order but says nothing about enum value
 * remapping, so that migration needs its own coverage or every saved op-1 audio-in patch
 * breaks silently.
 */

#include "catch2/catch2.hpp"

#include <string>
#include <vector>

#include "configuration.h"
#include "dsp/sintable.h"
#include "synth/matrix_index.h"
#include "synth/patch.h"

using namespace baconpaul::six_sines;

namespace
{
// Rewrite the root version attribute so the stream looks like one written by an older
// build, without needing a checked-in fixture per version.
std::string withStreamVersion(std::string xml, uint32_t version)
{
    auto needle = std::string("version=\"") + std::to_string(Patch::patchVersion) + "\"";
    auto at = xml.find(needle);
    REQUIRE(at != std::string::npos);
    xml.replace(at, needle.size(), "version=\"" + std::to_string(version) + "\"");
    return xml;
}

std::string setParamValue(std::string xml, uint32_t id, const std::string &value)
{
    auto needle = "<p id=\"" + std::to_string(id) + "\"";
    auto at = xml.find(needle);
    REQUIRE(at != std::string::npos);
    auto vAt = xml.find("v=\"", at);
    REQUIRE(vAt != std::string::npos);
    auto vEnd = xml.find('"', vAt + 3);
    REQUIRE(vEnd != std::string::npos);
    xml.replace(vAt + 3, vEnd - (vAt + 3), value);
    return xml;
}
} // namespace

TEST_CASE("USER_TABLE sits before AUDIO_IN in the waveform enum", "[wavetable][stream]")
{
    // The whole migration rests on this ordering; if someone appends USER_TABLE after
    // AUDIO_IN instead, the value fixups below become wrong rather than merely unnecessary.
    REQUIRE(static_cast<int>(SinTable::USER_TABLE) == 21);
    REQUIRE(static_cast<int>(SinTable::AUDIO_IN) == 22);
    REQUIRE(static_cast<int>(SinTable::NUM_WAVEFORMS) == 23);
}

TEST_CASE("The patch version was bumped for the waveform renumbering", "[wavetable][stream]")
{
    REQUIRE(Patch::patchVersion >= 13);
}

TEST_CASE("A pre-13 stream reads waveform 21 as audio in", "[wavetable][stream]")
{
    MatrixIndex::initialize();

    auto written = std::make_unique<Patch>();
    auto wfId = written->sourceNodes[0].waveForm.meta.id;
    auto xml = withStreamVersion(setParamValue(written->toState(), wfId, "21"), 12);

    auto read = std::make_unique<Patch>();
    read->sourceNodes[0].waveForm.value = 0.f;
    REQUIRE(read->fromState(xml));
    REQUIRE((int)std::round(read->sourceNodes[0].waveForm.value) == (int)SinTable::AUDIO_IN);
}

TEST_CASE("A pre-13 stream leaves waveforms below the insert point alone", "[wavetable][stream]")
{
    MatrixIndex::initialize();

    for (int wf : {(int)SinTable::SIN, (int)SinTable::TX5, (int)SinTable::TUKEY_WINDOW})
    {
        auto written = std::make_unique<Patch>();
        auto wfId = written->sourceNodes[1].waveForm.meta.id;
        auto xml =
            withStreamVersion(setParamValue(written->toState(), wfId, std::to_string(wf)), 12);

        auto read = std::make_unique<Patch>();
        REQUIRE(read->fromState(xml));
        INFO("waveform " << wf);
        REQUIRE((int)std::round(read->sourceNodes[1].waveForm.value) == wf);
    }
}

TEST_CASE("A version 13 stream keeps waveform 21 as the user table", "[wavetable][stream]")
{
    MatrixIndex::initialize();

    auto written = std::make_unique<Patch>();
    written->sourceNodes[2].waveForm.value = (float)SinTable::USER_TABLE;
    auto xml = written->toState();

    auto read = std::make_unique<Patch>();
    REQUIRE(read->fromState(xml));
    REQUIRE((int)std::round(read->sourceNodes[2].waveForm.value) == (int)SinTable::USER_TABLE);
}

TEST_CASE("The waveform parameter range covers the user table", "[wavetable][stream]")
{
    auto p = std::make_unique<Patch>();
    for (int i = 0; i < (int)numOps; ++i)
    {
        INFO("op " << i);
        REQUIRE(p->sourceNodes[i].waveForm.meta.maxVal >= (float)SinTable::USER_TABLE);
    }
}

TEST_CASE("The user table has a display name", "[wavetable][stream]")
{
    auto p = std::make_unique<Patch>();
    auto s = p->sourceNodes[0].waveForm.meta.valueToString((float)SinTable::USER_TABLE);
    REQUIRE(s.has_value());
    REQUIRE(!s->empty());
}

/*
 * Jogging. USER_TABLE is not a plain value pick - you reach it by loading a file - but once a
 * file is loaded it has to be reachable by the jog that left it, or a loaded table becomes
 * invisible to the control that selects everything else.
 */
#include "ui/waveform-display.h"

TEST_CASE("Jog skips the user table when nothing is loaded", "[wavetable][stream]")
{
    using namespace baconpaul::six_sines::ui;
    // walking the whole ring must never land on it
    int v = (int)SinTable::SIN;
    for (int i = 0; i < 64; ++i)
    {
        v = nextWaveformValue(v, 1, false, false);
        REQUIRE(v != (int)SinTable::USER_TABLE);
    }
}

TEST_CASE("Jog includes the user table once one is loaded", "[wavetable][stream]")
{
    using namespace baconpaul::six_sines::ui;
    bool seen{false};
    int v = (int)SinTable::SIN;
    for (int i = 0; i < 64 && !seen; ++i)
    {
        v = nextWaveformValue(v, 1, false, true);
        seen = (v == (int)SinTable::USER_TABLE);
    }
    REQUIRE(seen);
}

TEST_CASE("Jogging off the user table and back returns to it", "[wavetable][stream]")
{
    using namespace baconpaul::six_sines::ui;
    // this was the bug: leaving USER_TABLE found no current position, so it fell to index 0
    // and coming back landed on Tukey instead
    auto off = nextWaveformValue((int)SinTable::USER_TABLE, -1, false, true);
    REQUIRE(off != (int)SinTable::USER_TABLE);
    REQUIRE(nextWaveformValue(off, 1, false, true) == (int)SinTable::USER_TABLE);

    auto fwd = nextWaveformValue((int)SinTable::USER_TABLE, 1, false, true);
    REQUIRE(nextWaveformValue(fwd, -1, false, true) == (int)SinTable::USER_TABLE);
}

TEST_CASE("Jog can always leave the user table even with nothing loaded", "[wavetable][stream]")
{
    using namespace baconpaul::six_sines::ui;
    // a patch can arrive on USER_TABLE with its blob missing; the control must not trap there
    auto n = nextWaveformValue((int)SinTable::USER_TABLE, 1, false, false);
    REQUIRE(n != (int)SinTable::USER_TABLE);
}
