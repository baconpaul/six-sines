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
 * Bulk data reaching the audio thread is the one genuinely new piece of architecture here.
 * The contract these pin:
 *   - a table is built once and shared, keyed on the bytes it came from
 *   - the patch carries the source bytes, once per distinct table, not once per operator
 *   - nothing is ever freed while a voice could still be rendering it
 *   - a patch naming a wavetable whose blob is gone still loads
 */

#include "catch2/catch2.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "configuration.h"
#include "dsp/sintable.h"
#include "dsp/wavetable_io.h"
#include "synth/matrix_index.h"
#include "synth/patch.h"
#include "synth/wavetable_handoff.h"

using namespace baconpaul::six_sines;

namespace
{
// a minimal valid Surge .wt so the tests exercise the real parse path
std::vector<uint8_t> wtBytes(uint32_t nSamples, uint16_t nTables, int harmonicOffset = 0)
{
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
    for (char c : std::string("vawt"))
        v.push_back((uint8_t)c);
    put32(nSamples);
    put16(nTables);
    put16(0);
    for (int t = 0; t < nTables; ++t)
        for (uint32_t i = 0; i < nSamples; ++i)
        {
            float s = (float)std::sin(2 * M_PI * (t + 1 + harmonicOffset) * i / nSamples);
            uint32_t bits;
            std::memcpy(&bits, &s, 4);
            put32(bits);
        }
    return v;
}
} // namespace

namespace
{
std::shared_ptr<const Wavetable> build(const std::vector<uint8_t> &b)
{
    return buildWavetableFromBytes(b.data(), b.size(), "t", hashBytes(b.data(), b.size()));
}
} // namespace

TEST_CASE("Publishing stages a table for the audio thread", "[wavetable][store]")
{
    WavetableHandoff h;
    auto t = build(wtBytes(512, 2));
    REQUIRE(t);

    uint32_t slot{0};
    REQUIRE(h.publish(0, t, slot));
    REQUIRE(h.opForSlot(slot) == 0);

    // the audio side consumes by slot and gets the same table
    auto got = h.consume(slot);
    REQUIRE(got.get() == t.get());
}

TEST_CASE("Publication refuses to overwrite a slot the audio thread has not taken",
          "[wavetable][store]")
{
    WavetableHandoff h;
    auto t = build(wtBytes(512, 2));

    // fill every staging slot without consuming any
    uint32_t slot{0};
    for (uint32_t i = 0; i < WavetableHandoff::stageSlots; ++i)
        REQUIRE(h.publish(0, t, slot));

    // the next one has nowhere safe to go, so it is deferred rather than racing
    REQUIRE(!h.publish(0, t, slot));

    // drain one and it fits again
    h.consume(0);
    REQUIRE(h.publish(0, t, slot));
}

TEST_CASE("A retired table survives until nothing holds it", "[wavetable][store]")
{
    WavetableHandoff h;
    auto bytes = wtBytes(512, 2);
    auto t = build(bytes);
    REQUIRE(t);
    auto *raw = t.get();

    h.retire(t);
    // parking it twice must not double count
    h.retire(t);
    REQUIRE(h.retiringCount() == 1);

    // a "voice" still holding it after the patch let go
    auto voiceHeld = t;
    t.reset();

    h.collect();
    REQUIRE(h.retiringCount() == 1); // voiceHeld keeps it alive
    REQUIRE(voiceHeld.get() == raw);
    REQUIRE(h.retiredWithHash(voiceHeld->hash).get() == raw);

    voiceHeld.reset();
    h.collect();
    REQUIRE(h.retiringCount() == 0);
}

TEST_CASE("A patch carries one blob for two operators sharing a table",
          "[wavetable][store][stream]")
{
    MatrixIndex::initialize();

    auto p = std::make_unique<Patch>();
    auto bytes = wtBytes(1024, 4);
    auto idx = p->addWavetableBlob(bytes, "shared");
    REQUIRE(idx >= 0);
    // the same bytes again must reuse the blob rather than append a second
    REQUIRE(p->addWavetableBlob(bytes, "shared") == idx);
    REQUIRE(p->wavetableBlobs.size() == 1);

    p->sourceNodes[0].wavetableBlobIndex = idx;
    p->sourceNodes[3].wavetableBlobIndex = idx;
    p->sourceNodes[0].waveForm.value = (float)SinTable::USER_TABLE;
    p->sourceNodes[3].waveForm.value = (float)SinTable::USER_TABLE;

    auto xml = p->toState();

    auto q = std::make_unique<Patch>();
    REQUIRE(q->fromState(xml));
    REQUIRE(q->wavetableBlobs.size() == 1);
    REQUIRE(q->sourceNodes[0].wavetableBlobIndex == 0);
    REQUIRE(q->sourceNodes[3].wavetableBlobIndex == 0);
    REQUIRE(q->wavetableBlobs[0].sourceBytes == bytes);
    REQUIRE(q->wavetableBlobs[0].name == "shared");
}

TEST_CASE("Operators with no wavetable round trip as none", "[wavetable][store][stream]")
{
    MatrixIndex::initialize();

    auto p = std::make_unique<Patch>();
    auto xml = p->toState();

    auto q = std::make_unique<Patch>();
    for (int i = 0; i < (int)numOps; ++i)
        q->sourceNodes[i].wavetableBlobIndex = 0; // dirty it first
    REQUIRE(q->fromState(xml));
    REQUIRE(q->wavetableBlobs.empty());
    for (int i = 0; i < (int)numOps; ++i)
        REQUIRE(q->sourceNodes[i].wavetableBlobIndex == -1);
}

TEST_CASE("A patch whose blob is missing still loads", "[wavetable][store][stream]")
{
    MatrixIndex::initialize();

    auto p = std::make_unique<Patch>();
    p->sourceNodes[1].wavetableBlobIndex = 7; // an index into a blob list that will not exist
    p->sourceNodes[1].waveForm.value = (float)SinTable::USER_TABLE;
    auto xml = p->toState();

    auto q = std::make_unique<Patch>();
    REQUIRE(q->fromState(xml));
    // the reference is dropped rather than left dangling; the operator falls back to a sine
    REQUIRE(q->sourceNodes[1].wavetableBlobIndex == -1);
}

TEST_CASE("A blob survives deflate and base64", "[wavetable][store][stream]")
{
    auto bytes = wtBytes(2048, 8);
    auto packed = deflateBytes(bytes.data(), bytes.size());
    REQUIRE(!packed.empty());
    std::vector<uint8_t> out;
    REQUIRE(inflateBytes(packed.data(), packed.size(), bytes.size(), out));
    REQUIRE(out == bytes);

    auto b64 = base64Encode(packed.data(), packed.size());
    std::vector<uint8_t> dec;
    REQUIRE(base64Decode(b64, dec));
    REQUIRE(dec == packed);
}

TEST_CASE("Deflate actually shrinks a wavetable", "[wavetable][store][stream]")
{
    // float32 audio does not compress well, but it should not grow either
    auto bytes = wtBytes(2048, 8);
    auto packed = deflateBytes(bytes.data(), bytes.size());
    INFO("raw " << bytes.size() << " packed " << packed.size());
    REQUIRE(packed.size() < bytes.size());
}

/*
 * The reconcile is the main-thread step that turns patch blobs into live tables and tells
 * the audio thread about them. It is idempotent so it can run on every idle.
 */
#include "synth/synth.h"
#include "synth/voice.h"

TEST_CASE("Reconcile builds and publishes a patch's wavetable", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto bytes = wtBytes(1024, 4);
    auto idx = s->patchMain.addWavetableBlob(bytes, "reconciled");
    s->patchMain.sourceNodes[2].wavetableBlobIndex = idx;
    s->patchMain.sourceNodes[2].waveForm.value = (float)SinTable::USER_TABLE;

    REQUIRE(s->reconcileWavetables());
    REQUIRE(s->patchMain.sourceNodes[2].wavetable != nullptr);

    // it queued a SET_WAVETABLE rather than touching monoValues directly
    REQUIRE(s->patch.sourceNodes[2].wavetable == nullptr);
    s->processUIQueue(nullptr);
    REQUIRE(s->patch.sourceNodes[2].wavetable != nullptr);
    REQUIRE(s->patch.sourceNodes[2].wavetable->nFrames == 4);
    REQUIRE(s->wavetableError[2].empty());
}

TEST_CASE("Reconcile is idempotent", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto idx = s->patchMain.addWavetableBlob(wtBytes(512, 2), "t");
    s->patchMain.sourceNodes[0].wavetableBlobIndex = idx;
    s->patchMain.sourceNodes[0].waveForm.value = (float)SinTable::USER_TABLE;

    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    auto *first = s->patch.sourceNodes[0].wavetable.get();

    for (int i = 0; i < 5; ++i)
    {
        REQUIRE(s->reconcileWavetables());
        s->processUIQueue(nullptr);
    }
    // same table, and no pile of retiring copies
    REQUIRE(s->patch.sourceNodes[0].wavetable.get() == first);
    REQUIRE(s->patchMain.sourceNodes[0].wavetable.get() == first);
    REQUIRE(s->wavetableHandoff.retiringCount() == 0);
}

TEST_CASE("Reconcile drops a table no operator wants any more", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto idx = s->patchMain.addWavetableBlob(wtBytes(512, 2), "t");
    s->patchMain.sourceNodes[0].wavetableBlobIndex = idx;
    s->patchMain.sourceNodes[0].waveForm.value = (float)SinTable::USER_TABLE;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    REQUIRE(s->patchMain.sourceNodes[0].wavetable != nullptr);

    // Drop the selection outright rather than just switching waveform. Switching waveform
    // alone parks the table warm so flipping back is instant - covered separately - so this
    // is the path that actually retires one.
    s->patchMain.sourceNodes[0].waveForm.value = (float)SinTable::SIN;
    s->patchMain.sourceNodes[0].wavetableBlobIndex = -1;
    REQUIRE(s->reconcileWavetables());
    REQUIRE(s->patchMain.sourceNodes[0].wavetable == nullptr);
    // main has let go but the audio patch has not yet seen the message, so it is parked
    REQUIRE(s->wavetableHandoff.retiringCount() == 1);

    s->processUIQueue(nullptr);
    REQUIRE(s->patch.sourceNodes[0].wavetable == nullptr);
    s->wavetableHandoff.collect();
    REQUIRE(s->wavetableHandoff.retiringCount() == 0);
}

TEST_CASE("Reconcile reports a bad blob instead of going quiet", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    std::vector<uint8_t> junk{'v', 'a', 'w', 't', 0, 0, 0, 0, 0, 0, 0, 0};
    auto idx = s->patchMain.addWavetableBlob(junk, "broken");
    s->patchMain.sourceNodes[4].wavetableBlobIndex = idx;
    s->patchMain.sourceNodes[4].waveForm.value = (float)SinTable::USER_TABLE;

    REQUIRE(s->reconcileWavetables());
    REQUIRE(!s->wavetableError[4].empty());
    REQUIRE(s->patchMain.sourceNodes[4].wavetable == nullptr);
}

TEST_CASE("Two operators on one blob share one published table", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto idx = s->patchMain.addWavetableBlob(wtBytes(1024, 4), "shared");
    for (int op : {1, 5})
    {
        s->patchMain.sourceNodes[op].wavetableBlobIndex = idx;
        s->patchMain.sourceNodes[op].waveForm.value = (float)SinTable::USER_TABLE;
    }

    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    // one build, one allocation, both operators pointing at it on both sides
    REQUIRE(s->patch.sourceNodes[1].wavetable != nullptr);
    REQUIRE(s->patch.sourceNodes[1].wavetable.get() == s->patch.sourceNodes[5].wavetable.get());
    REQUIRE(s->patchMain.sourceNodes[1].wavetable.get() ==
            s->patchMain.sourceNodes[5].wavetable.get());
}

TEST_CASE("A wavetable survives a full engine state round trip", "[wavetable][store]")
{
    MatrixIndex::initialize();

    auto bytes = wtBytes(1024, 4);

    std::string saved;
    {
        auto s = std::make_unique<Synth>(false);
        s->setSampleRate(48000.0);
        auto idx = s->patchMain.addWavetableBlob(bytes, "roundtrip");
        s->patchMain.sourceNodes[0].wavetableBlobIndex = idx;
        s->patchMain.sourceNodes[0].waveForm.value = (float)SinTable::USER_TABLE;
        REQUIRE(s->reconcileWavetables());
        saved = s->patchMain.toState();
    }

    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);
    REQUIRE(s->patchMain.fromState(saved));

    // what a restarted plugin has before anything reconciles
    REQUIRE(s->patchMain.wavetableBlobs.size() == 1);
    REQUIRE(s->patchMain.wavetableBlobs[0].sourceBytes == bytes);
    REQUIRE(s->patchMain.sourceNodes[0].wavetableBlobIndex == 0);
    REQUIRE((int)std::round(s->patchMain.sourceNodes[0].waveForm.value) ==
            (int)SinTable::USER_TABLE);

    // and after the idle does its work
    REQUIRE(s->reconcileWavetables());
    REQUIRE(s->patchMain.sourceNodes[0].wavetable != nullptr);
    REQUIRE(s->patchMain.sourceNodes[0].wavetable->nFrames == 4);
    s->processUIQueue(nullptr);
    REQUIRE(s->patch.sourceNodes[0].wavetable != nullptr);
}

TEST_CASE("copyValuesFrom carries the wavetable state", "[wavetable][store]")
{
    MatrixIndex::initialize();

    // This is the path a host stateLoad takes: parse into a temp patch, then copy into
    // patchMain. Blobs are not Params, so unless copyValuesFrom carries them the waveform
    // restores as USER_TABLE with nothing behind it.
    auto bytes = wtBytes(512, 2);
    auto from = std::make_unique<Patch>();
    auto idx = from->addWavetableBlob(bytes, "copied");
    from->sourceNodes[2].wavetableBlobIndex = idx;
    from->sourceNodes[2].waveForm.value = (float)SinTable::USER_TABLE;

    auto to = std::make_unique<Patch>();
    to->copyValuesFrom(*from);

    REQUIRE(to->wavetableBlobs.size() == 1);
    REQUIRE(to->wavetableBlobs[0].sourceBytes == bytes);
    REQUIRE(to->wavetableBlobs[0].name == "copied");
    REQUIRE(to->sourceNodes[2].wavetableBlobIndex == idx);
    for (int i = 0; i < (int)numOps; ++i)
        if (i != 2)
            REQUIRE(to->sourceNodes[i].wavetableBlobIndex == -1);
}

TEST_CASE("copyValuesFrom does not copy the built table", "[wavetable][store]")
{
    MatrixIndex::initialize();

    // The two patches are on different threads and each owns its own shared_ptr. Copying one
    // into the other would put a main-thread write on the audio patch's pointer, which is the
    // one thing the handoff exists to avoid. Reconcile publishes instead.
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);
    auto idx = s->patchMain.addWavetableBlob(wtBytes(512, 2), "t");
    s->patchMain.sourceNodes[0].wavetableBlobIndex = idx;
    s->patchMain.sourceNodes[0].waveForm.value = (float)SinTable::USER_TABLE;
    REQUIRE(s->reconcileWavetables());
    REQUIRE(s->patchMain.sourceNodes[0].wavetable != nullptr);

    auto to = std::make_unique<Patch>();
    to->copyValuesFrom(s->patchMain);
    REQUIRE(to->sourceNodes[0].wavetable == nullptr);
    REQUIRE(to->sourceNodes[0].wavetableBlobIndex == idx);
}

TEST_CASE("Switching away from a wavetable and back does not rebuild it", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto idx = s->patchMain.addWavetableBlob(wtBytes(1024, 8), "kept");
    auto &sn = s->patchMain.sourceNodes[0];
    sn.wavetableBlobIndex = idx;
    sn.waveForm.value = (float)SinTable::USER_TABLE;

    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    auto *built = sn.wavetable.get();
    REQUIRE(built != nullptr);

    // switch to a plain sine: the operator stops reading it, but the blob is still selected
    sn.waveForm.value = (float)SinTable::SIN;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    REQUIRE(sn.wavetable == nullptr);
    REQUIRE(s->patch.sourceNodes[0].wavetable == nullptr);

    // a few idles go by, each collecting; the warm copy has to survive them
    for (int i = 0; i < 5; ++i)
    {
        REQUIRE(s->reconcileWavetables());
        s->processUIQueue(nullptr);
    }

    // and back: the same object, not an identical rebuild
    sn.waveForm.value = (float)SinTable::USER_TABLE;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    REQUIRE(sn.wavetable.get() == built);
    REQUIRE(s->patch.sourceNodes[0].wavetable.get() == built);
}

TEST_CASE("Clearing the wavetable lets the warm copy go", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto idx = s->patchMain.addWavetableBlob(wtBytes(512, 2), "t");
    auto &sn = s->patchMain.sourceNodes[0];
    sn.wavetableBlobIndex = idx;
    sn.waveForm.value = (float)SinTable::USER_TABLE;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    auto weak = std::weak_ptr<const Wavetable>(sn.wavetable);

    // switching away keeps it warm...
    sn.waveForm.value = (float)SinTable::SIN;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    REQUIRE(!weak.expired());

    // ...but dropping the selection entirely must not keep it forever
    sn.wavetableBlobIndex = -1;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    REQUIRE(s->reconcileWavetables());
    REQUIRE(weak.expired());
}

TEST_CASE("Changing playback mode does not keep the old table warm", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto idx = s->patchMain.addWavetableBlob(wtBytes(512, 2), "t");
    auto &sn = s->patchMain.sourceNodes[0];
    sn.wavetableBlobIndex = idx;
    sn.waveForm.value = (float)SinTable::USER_TABLE;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    auto weak = std::weak_ptr<const Wavetable>(sn.wavetable);

    // a different mode is a different table, so the old one is not something to hold onto
    sn.wavetableBandLimit.value = (float)WavetableBandLimit::DIRECT;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    REQUIRE(s->reconcileWavetables());
    REQUIRE(weak.expired());
    REQUIRE(sn.wavetable != nullptr);
    REQUIRE(sn.wavetable->nLevels == 1); // it really did rebuild as Direct
}

TEST_CASE("Selecting a wavetable with nothing loaded gives the built in one",
          "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto &sn = s->patchMain.sourceNodes[0];
    REQUIRE(sn.wavetableBlobIndex == -1);
    sn.waveForm.value = (float)SinTable::USER_TABLE;

    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);

    REQUIRE(sn.wavetable != nullptr);
    REQUIRE(sn.wavetable->nFrames == 10); // sine through to a ten harmonic saw
    REQUIRE(s->patch.sourceNodes[0].wavetable.get() == sn.wavetable.get());
    REQUIRE(s->wavetableError[0].empty());

    // and it costs the patch nothing, being built in
    REQUIRE(s->patchMain.wavetableBlobs.empty());
    auto xml = s->patchMain.toState();

    auto t = std::make_unique<Synth>(false);
    t->setSampleRate(48000.0);
    REQUIRE(t->patchMain.fromState(xml));
    REQUIRE(t->patchMain.wavetableBlobs.empty());
    REQUIRE(t->reconcileWavetables());
    REQUIRE(t->patchMain.sourceNodes[0].wavetable != nullptr);
    REQUIRE(t->patchMain.sourceNodes[0].wavetable->nFrames == 10);
}

TEST_CASE("Two operators on the built in table share one build", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    for (int op : {0, 4})
        s->patchMain.sourceNodes[op].waveForm.value = (float)SinTable::USER_TABLE;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);

    REQUIRE(s->patchMain.sourceNodes[0].wavetable.get() ==
            s->patchMain.sourceNodes[4].wavetable.get());
}

TEST_CASE("Loading a file over the built in table replaces it", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto &sn = s->patchMain.sourceNodes[0];
    sn.waveForm.value = (float)SinTable::USER_TABLE;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    auto *builtIn = sn.wavetable.get();

    sn.wavetableBlobIndex = s->patchMain.addWavetableBlob(wtBytes(1024, 4), "mine");
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    REQUIRE(sn.wavetable.get() != builtIn);
    REQUIRE(sn.wavetable->nFrames == 4);
}

TEST_CASE("A fresh operator defaults to tapered with the chain on", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto p = std::make_unique<Patch>();
    for (int i = 0; i < (int)numOps; ++i)
    {
        INFO("op " << i);
        REQUIRE((int)std::round(p->sourceNodes[i].wavetableBandLimit.value) ==
                (int)WavetableBandLimit::BAND_LIMITED_TAPERED);
        REQUIRE(p->sourceNodes[i].wavetableMipChain.value > 0.5f);
    }
}

TEST_CASE("The default playback mode is what actually gets built", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto &sn = s->patchMain.sourceNodes[0];
    sn.wavetableBlobIndex = s->patchMain.addWavetableBlob(wtBytes(2048, 2), "t");
    sn.waveForm.value = (float)SinTable::USER_TABLE;
    REQUIRE(s->reconcileWavetables());
    REQUIRE(sn.wavetable != nullptr);

    // tapered keeps the chain, so it is not the single level Direct builds
    REQUIRE(sn.wavetable->nLevels > 1);
    REQUIRE(sn.wavetable->hash ==
            saltHashWithMode(hashBytes(wtBytes(2048, 2).data(), wtBytes(2048, 2).size()),
                             WavetableBandLimit::BAND_LIMITED_TAPERED, true));
}

TEST_CASE("One operator's warm table does not keep another's alive", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    auto aBytes = wtBytes(512, 2, 0);
    auto bBytes = wtBytes(512, 2, 5);
    auto ia = s->patchMain.addWavetableBlob(aBytes, "a");
    auto ib = s->patchMain.addWavetableBlob(bBytes, "b");

    auto &op0 = s->patchMain.sourceNodes[0];
    auto &op1 = s->patchMain.sourceNodes[1];
    op0.wavetableBlobIndex = ia;
    op0.waveForm.value = (float)SinTable::USER_TABLE;
    op1.wavetableBlobIndex = ib;
    op1.waveForm.value = (float)SinTable::USER_TABLE;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);

    auto weakB = std::weak_ptr<const Wavetable>(op1.wavetable);

    // both switch away: each parks its own table warm
    op0.waveForm.value = (float)SinTable::SIN;
    op1.waveForm.value = (float)SinTable::SIN;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    REQUIRE(!weakB.expired());

    /*
     * Now point op1 at op0's blob. op1's warm copy is B, which nothing wants any more - but a
     * check that merely asked "is any warm table keyed A" would find op0's and spare B.
     */
    op1.wavetableBlobIndex = ia;
    REQUIRE(s->reconcileWavetables());
    s->processUIQueue(nullptr);
    REQUIRE(s->reconcileWavetables());
    REQUIRE(weakB.expired());
}

TEST_CASE("A blob that will not build is not retried every reconcile", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);

    // a well formed header promising samples that are not there, so it parses far enough to
    // be expensive and still fails
    std::vector<uint8_t> bad{'v', 'a', 'w', 't', 0x00, 0x08, 0, 0, 0x40, 0x00, 0, 0};
    auto &sn = s->patchMain.sourceNodes[0];
    sn.wavetableBlobIndex = s->patchMain.addWavetableBlob(bad, "broken");
    sn.waveForm.value = (float)SinTable::USER_TABLE;

    REQUIRE(s->reconcileWavetables());
    auto first = s->wavetableError[0];
    REQUIRE(!first.empty());
    REQUIRE(sn.wavetable == nullptr);

    // the reconcile runs every idle, so the message has to survive without the work being
    // redone; the cached reason is what proves it took the remembered path
    for (int i = 0; i < 20; ++i)
    {
        REQUIRE(s->reconcileWavetables());
        REQUIRE(s->wavetableError[0] == first);
    }
    REQUIRE(s->wavetableHandoff.failureFor(saltHashWithMode(
                hashBytes(bad.data(), bad.size()),
                (WavetableBandLimit)std::round(sn.wavetableBandLimit.value),
                sn.wavetableMipChain.value > 0.5f)) != nullptr);
}

TEST_CASE("A patch whose stored wavetable will not decode says so", "[wavetable][store][stream]")
{
    MatrixIndex::initialize();

    auto bytes = wtBytes(1024, 4);
    auto p = std::make_unique<Patch>();
    auto idx = p->addWavetableBlob(bytes, "mine");
    p->sourceNodes[0].wavetableBlobIndex = idx;
    p->sourceNodes[0].waveForm.value = (float)SinTable::USER_TABLE;
    auto xml = p->toState();

    // corrupt the payload the way a truncated or hand-edited file would
    auto at = xml.find("encoding=\"deflate-base64\"");
    REQUIRE(at != std::string::npos);
    auto open = xml.find('>', at);
    auto close = xml.find("</table>", open);
    REQUIRE(close != std::string::npos);
    xml = xml.substr(0, open + 1) + "!!!!not base64!!!!" + xml.substr(close);

    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(48000.0);
    REQUIRE(s->patchMain.fromState(xml));

    // the operator keeps pointing at the blob that failed, rather than quietly becoming
    // something else
    REQUIRE(s->patchMain.sourceNodes[0].wavetableBlobIndex == idx);
    REQUIRE(s->patchMain.wavetableBlobs[idx].sourceBytes.empty());

    REQUIRE(s->reconcileWavetables());
    REQUIRE(!s->wavetableError[0].empty());
    REQUIRE(s->patchMain.sourceNodes[0].wavetable == nullptr);
}

TEST_CASE("The encoded size a save would warn about is measurable", "[wavetable][store]")
{
    MatrixIndex::initialize();
    auto p = std::make_unique<Patch>();
    REQUIRE(p->encodedWavetableBytes() == 0);

    auto bytes = wtBytes(2048, 8);
    p->addWavetableBlob(bytes, "big");
    auto enc = p->encodedWavetableBytes();

    /*
     * No bound is asserted on the ratio. Deflate then base64 is roughly break-even on real
     * recorded material, but these frames are synthesised sines and compress to well under
     * half, so a "barely compresses" assertion would be pinning the test's own content rather
     * than the code. What matters is that it measures something and that one table sits well
     * inside the threshold - the warning is for a patch carrying several large ones.
     */
    INFO("raw " << bytes.size() << " encoded " << enc);
    REQUIRE(enc > 0);
    REQUIRE(enc < bytes.size() * 2);
    REQUIRE(enc < Patch::wavetablePayloadWarnBytes);
}
