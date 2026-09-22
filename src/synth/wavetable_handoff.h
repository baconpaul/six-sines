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

#ifndef BACONPAUL_SIX_SINES_SYNTH_WAVETABLE_HANDOFF_H
#define BACONPAUL_SIX_SINES_SYNTH_WAVETABLE_HANDOFF_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "configuration.h"
#include "synth/patch.h"
#include "dsp/wavetable.h"
#include "dsp/wavetable_io.h"

namespace baconpaul::six_sines
{
/*
 * Getting a built wavetable from the main thread to the audio thread, and knowing when the
 * old one is safe to free. Nothing else - the tables themselves belong to the patch, one
 * shared_ptr per SourceNode on each side, the same mirroring every parameter gets.
 *
 * A built table is immutable, so any thread may read one freely and we only ever swap
 * pointers. That buys us no read synchronisation cost at all. It does not buy lifetime, and
 * these two pieces are what cover lifetime:
 *
 * Staging. The ring buffer between the threads carries POD, so it cannot carry a shared_ptr,
 * and main cannot write a shared_ptr object that audio reads. So main parks a reference in a
 * slot and sends the slot index; audio moves it out. Two counters gate the ring, so main can
 * never overwrite a slot audio has not taken - publish() refuses instead, and the caller
 * retries on its next idle.
 *
 * Retiring. A table displaced from patchMain waits here until use_count says nothing holds it
 * any more: not patchMain, not patch, and not any voice that latched it at attack. Because
 * this list always holds a reference until then, a refcount can never reach zero on the audio
 * thread and no destructor ever runs there.
 */
struct WavetableHandoff
{
    static constexpr uint32_t stageSlots{16};

    /*
     * [main] Park `table` for operator `op` and report the slot. False means every slot is
     * still in flight; defer rather than race one.
     */
    bool publish(int op, const std::shared_ptr<const Wavetable> &table, uint32_t &slotOut)
    {
        auto written = stageWritten.load(std::memory_order_relaxed);
        auto read = stageRead.load(std::memory_order_acquire);
        if (written - read >= stageSlots)
            return false;

        auto slot = written % stageSlots;
        stage[slot] = table;
        stageOp[slot] = op;
        stageWritten.store(written + 1, std::memory_order_release);
        slotOut = slot;
        return true;
    }

    /*
     * [audio] Take the staged reference. Moved rather than copied, so the slot stops holding
     * it - otherwise the ring would pin the last stageSlots tables for the life of the engine
     * and collect() could never reclaim them. Moving costs no refcount traffic either.
     */
    std::shared_ptr<const Wavetable> consume(uint32_t slot)
    {
        if (slot >= stageSlots)
            return nullptr;
        auto t = std::move(stage[slot]);
        stageRead.fetch_add(1, std::memory_order_release);
        return t;
    }

    int opForSlot(uint32_t slot) const { return slot < stageSlots ? stageOp[slot] : -1; }

    /*
     * [main] Hold onto an operator's table while it is switched away from a wavetable but
     * still has a blob selected. Rebuilding an 8 bit era table is cheap, rebuilding a 64 frame
     * Serum one is not, and flipping the waveform back and forth is a normal thing to do while
     * auditioning. At most one per operator, dropped as soon as the operator wants something
     * else, so it cannot grow.
     */
    void keepWarm(int op, const std::shared_ptr<const Wavetable> &t)
    {
        if (op >= 0 && op < (int)numOps)
            warm[op] = t;
    }
    void dropWarm(int op)
    {
        if (op >= 0 && op < (int)numOps)
            warm[op].reset();
    }
    std::shared_ptr<const Wavetable> warmWithHash(uint64_t hash) const
    {
        for (const auto &t : warm)
            if (t && t->hash == hash)
                return t;
        return nullptr;
    }

    // Whether THIS operator's warm table is the one asked for. warmWithHash answers "does
    // anyone hold this", which is the right question for reuse and the wrong one for deciding
    // whether an operator's own warm copy has gone stale.
    bool warmMatches(int op, uint64_t hash) const
    {
        return op >= 0 && op < (int)numOps && warm[op] && warm[op]->hash == hash;
    }

    // [main] Hold a displaced table until its last reader is done with it.
    void retire(const std::shared_ptr<const Wavetable> &table)
    {
        if (!table)
            return;
        for (const auto &t : retiring)
            if (t.get() == table.get())
                return; // already parked; never let it in twice
        retiring.push_back(table);
    }

    // [main] A table already built for these bytes, whether live or on its way out. Lets an
    // off-and-on-again reuse an 8MB table instead of rebuilding it.
    std::shared_ptr<const Wavetable> retiredWithHash(uint64_t hash) const
    {
        for (const auto &t : retiring)
            if (t->hash == hash)
                return t;
        return nullptr;
    }

    /*
     * [main] Remember that a key would not build, and why. Keys are content derived, so a
     * failure is permanent for those bytes - and without this the reconcile retries the parse
     * on every idle, which for a large corrupt blob is real work at idle rate, forever.
     */
    void rememberFailure(uint64_t key, const std::string &why) { failures[key] = why; }
    const std::string *failureFor(uint64_t key) const
    {
        auto it = failures.find(key);
        return it == failures.end() ? nullptr : &it->second;
    }

    // [main] Drop anything nothing else references. From idle, never from audio.
    void collect()
    {
        retiring.erase(std::remove_if(retiring.begin(), retiring.end(),
                                      [](const std::shared_ptr<const Wavetable> &t)
                                      { return t.use_count() == 1; }),
                       retiring.end());
    }

    size_t retiringCount() const { return retiring.size(); }

  private:
    std::map<uint64_t, std::string> failures;
    std::vector<std::shared_ptr<const Wavetable>> retiring;
    std::array<std::shared_ptr<const Wavetable>, numOps> warm{};

    std::array<std::shared_ptr<const Wavetable>, stageSlots> stage{};
    std::array<int, stageSlots> stageOp{};
    std::atomic<uint32_t> stageWritten{0};
    std::atomic<uint32_t> stageRead{0};
};
/*
 * [main] Bring a patch's nodes and the audio thread in line with its wavetable blobs: build
 * anything newly referenced, hand it across, and let go of anything nothing points at.
 *
 * A free function rather than a Synth method because both callers need it and neither knows
 * about the other - the engine's load funnel, and the editor after a load gesture. Both run
 * on the main thread, so there is no ordering to arrange between them.
 *
 * Idempotent, so it is safe on every idle. False means a staging slot was unavailable and the
 * caller should come back; errors[op] carries a message when a blob would not build.
 */
template <typename QueueT, typename MsgT>
bool reconcileWavetables(Patch &patchMain, WavetableHandoff &handoff, QueueT &toAudio,
                         std::array<std::string, numOps> &errors)
{
    bool complete{true};

    /*
     * Dedup without a registry: with six operators, "have I built this already" is a scan of
     * the nodes and the retiring list for a matching hash. A Wavetable carries its own hash,
     * so a second operator on the same blob - or one switched off and straight back on -
     * reuses the 8MB instead of rebuilding it.
     */
    auto alreadyBuilt = [&](uint64_t hash) -> std::shared_ptr<const Wavetable>
    {
        for (const auto &sn : patchMain.sourceNodes)
            if (sn.wavetable && sn.wavetable->hash == hash)
                return sn.wavetable;
        if (auto w = handoff.warmWithHash(hash))
            return w;
        return handoff.retiredWithHash(hash);
    };

    for (int op = 0; op < (int)numOps; ++op)
    {
        errors[op].clear();
        auto &sn = patchMain.sourceNodes[op];

        auto wf = (int)std::round(sn.waveForm.value);
        auto blob = sn.wavetableBlobIndex;
        std::shared_ptr<const Wavetable> want;

        // Whatever this operator is set to now, anything warm for it that no longer matches
        // its selection is dead weight.
        if (blob < 0 || blob >= (int)patchMain.wavetableBlobs.size())
        {
            handoff.dropWarm(op);
        }
        else
        {
            auto liveKey = saltHashWithMode(
                patchMain.wavetableBlobs[blob].hash,
                static_cast<WavetableBandLimit>((uint32_t)std::round(sn.wavetableBandLimit.value)),
                sn.wavetableMipChain.value > 0.5f);
            if (!handoff.warmMatches(op, liveKey))
                handoff.dropWarm(op);
        }

        if (wf == SinTable::USER_TABLE && blob < 0)
        {
            /*
             * Switched to a wavetable with nothing loaded. Rather than falling back to a sine
             * - which makes the waveform a mode you cannot hear until you have been to a file
             * dialog - give it the built in table. It needs no blob, so a patch using it is
             * self contained for free, and loading a file over it is a normal load.
             */
            const auto &d = defaultWavetableBytes();
            auto key = saltHashWithMode(
                defaultWavetableHash(),
                static_cast<WavetableBandLimit>((uint32_t)std::round(sn.wavetableBandLimit.value)),
                sn.wavetableMipChain.value > 0.5f);
            want = alreadyBuilt(key);
            if (!want)
            {
                auto built =
                    buildWavetableFromBytes(d.data(), d.size(), "Sine to Saw", key,
                                            static_cast<WavetableBandLimit>(
                                                (uint32_t)std::round(sn.wavetableBandLimit.value)),
                                            sn.wavetableMipChain.value > 0.5f);
                if (built)
                    want = std::shared_ptr<const Wavetable>{std::move(built)};
            }
        }
        else if (wf == SinTable::USER_TABLE && blob >= 0 &&
                 blob < (int)patchMain.wavetableBlobs.size())
        {
            const auto &b = patchMain.wavetableBlobs[blob];
            if (b.sourceBytes.empty())
            {
                // the blob was in the patch and did not survive being read back
                errors[op] = "the wavetable stored in this patch could not be read";
            }
            else
            {
                // Playback mode is part of a table's identity - the same file Direct and Band
                // Limited are two different tables - so it has to be in the key.
                auto mode = static_cast<WavetableBandLimit>(
                    (uint32_t)std::round(sn.wavetableBandLimit.value));
                auto mips = sn.wavetableMipChain.value > 0.5f;
                auto key = saltHashWithMode(b.hash, mode, mips);
                want = alreadyBuilt(key);
                if (!want)
                {
                    if (auto *why = handoff.failureFor(key))
                    {
                        // known bad: do not parse it again every idle
                        errors[op] = *why;
                    }
                    else
                    {
                        auto built = buildWavetableFromBytes(
                            b.sourceBytes.data(), b.sourceBytes.size(), b.name, key, mode, mips);
                        if (built)
                        {
                            want = std::shared_ptr<const Wavetable>{std::move(built)};
                        }
                        else
                        {
                            // the operator falls back to a sine; say why rather than going quiet
                            auto parsed =
                                parseWavetable(b.sourceBytes.data(), b.sourceBytes.size(), b.name);
                            auto why2 =
                                parsed.error.empty()
                                    ? std::string("could not build a wavetable from that data")
                                    : parsed.error;
                            handoff.rememberFailure(key, why2);
                            errors[op] = why2;
                        }
                    }
                }
            }
        }

        if (sn.wavetable.get() == want.get())
            continue;

        uint32_t slot{0};
        if (!handoff.publish(op, want, slot))
        {
            // every staging slot is still in flight; leave it for the next idle
            complete = false;
            continue;
        }

        /*
         * Park the displaced table before letting go of it: the audio patch's copy and any
         * voice that latched it are still reading. Where the operator is only switched away
         * from its wavetable rather than off it, park it warm instead - a reference either
         * way, but warm survives collect() so switching back is instant.
         */
        if (!want && sn.wavetable && blob >= 0 && blob < (int)patchMain.wavetableBlobs.size())
            handoff.keepWarm(op, sn.wavetable);
        else
            handoff.retire(sn.wavetable);
        sn.wavetable = want;
        toAudio.push({MsgT::SET_WAVETABLE, slot});
    }

    handoff.collect();
    return complete;
}
} // namespace baconpaul::six_sines
#endif // BACONPAUL_SIX_SINES_SYNTH_WAVETABLE_HANDOFF_H
