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

#ifndef BACONPAUL_SIX_SINES_SYNTH_TUNING_H
#define BACONPAUL_SIX_SINES_SYNTH_TUNING_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>

#include "configuration.h"

struct MTSClient;

namespace baconpaul::six_sines
{
/*
 * An scl/kbm flattened to semitones-from-12-TET per midi key. Built on the main thread,
 * read on the audio thread.
 */
struct RetuneTable
{
    static constexpr int numKeys{128};
    std::array<float, numKeys> semitones{};
    bool valid{false};
};

/*
 * Latest-wins triple buffer carrying a RetuneTable from main to audio. Main fills
 * writeBuffer() and publishes; audio takes the newest on its next block. Neither side waits
 * and a publish can never fail, which a staged ring could when the plugin is inactive.
 */
struct TuningHandoff
{
    // [main]
    RetuneTable &writeBuffer() { return buf[back]; }
    void publish() { back = middle.exchange(back | freshBit, std::memory_order_acq_rel) & idxMask; }

    // [audio] true when a newer table landed in front()
    bool consume()
    {
        if (!(middle.load(std::memory_order_relaxed) & freshBit))
            return false;
        front = middle.exchange(front, std::memory_order_acq_rel) & idxMask;
        return true;
    }
    const RetuneTable &frontBuffer() const { return buf[front]; }

  private:
    static constexpr uint8_t freshBit{4}, idxMask{3};
    std::array<RetuneTable, 3> buf{};
    uint8_t back{0}, front{2};
    std::atomic<uint8_t> middle{1};
};

/*
 * Every voice asks this, and only this, how far to move a key. The mode comes from the
 * patch, the mts client from the engine and the table from the session.
 */
struct Tuning
{
    TuningMode mode{TM_MTS};
    MTSClient *mtsClient{nullptr};
    // asked once per host buffer, not per voice per block
    bool mtsHasMaster{false};
    RetuneTable sclKbm;

    bool retunes() const
    {
        switch (mode)
        {
        case TM_MTS:
            return mtsHasMaster;
        case TM_SCL_KBM:
            return sclKbm.valid;
        case TM_12TET:
            break;
        }
        return false;
    }

    // semitones away from 12-TET for this key; only meaningful when retunes()
    float retuningInSemitones(int key, int channel) const;
};

/*
 * [main] Parse scl and optional kbm text into a table. Exceptions from the tuning library
 * stop here. On failure returns false, fills errorOut and leaves out untouched.
 */
bool buildRetuneTable(const std::string &scl, const std::string &kbm, RetuneTable &out,
                      std::string &errorOut);

/*
 * [main] The one way a session scale reaches the audio thread. Empty scl clears it; a kbm
 * alone means 12-TET under that mapping. On a bad scale nothing is published.
 */
bool publishSclKbm(const std::string &scl, const std::string &kbm, TuningHandoff &handoff,
                   std::string &errorOut);

// seeds for the editor, both no-ops against any scale
std::string twelveTETSclText();
std::string defaultKbmText();

// short name for the status label, empty when there is no scale
std::string sclKbmDisplayName(const std::string &scl, const std::string &kbm);

} // namespace baconpaul::six_sines

#endif // BACONPAUL_SIX_SINES_SYNTH_TUNING_H
