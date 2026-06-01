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

#ifndef BACONPAUL_SIX_SINES_SYNTH_MONO_VALUES_H
#define BACONPAUL_SIX_SINES_SYNTH_MONO_VALUES_H

#include <sst/basic-blocks/tables/DbToLinearProvider.h>
#include <sst/basic-blocks/tables/EqualTuningProvider.h>
#include <sst/basic-blocks/tables/TwoToTheXProvider.h>
#include <sst/basic-blocks/dsp/RNG.h>

#include "mod_matrix.h"

struct MTSClient;

namespace baconpaul::six_sines
{
struct MonoValues;
struct SRProvider
{
    const sst::basic_blocks::tables::TwoToTheXProvider &ttx;
    SRProvider(const sst::basic_blocks::tables::TwoToTheXProvider &t) : ttx(t) {}
    float envelope_rate_linear_nowrap(float f) const
    {
        return (blockSize * sampleRateInv) * ttx.twoToThe(-f);
    }

    void setSampleRate(double sr)
    {
        samplerate = sr;
        sampleRate = sr;
        sampleRateInv = 1.0 / sr;
    }
    double samplerate{1};
    double sampleRate{1};
    double sampleRateInv{1};
};

struct MonoValues
{
    MonoValues() : sr(twoToTheX)
    {
        tuningProvider.init();
        twoToTheX.init();
        dbToLinear.init();
        std::fill(macroPtr.begin(), macroPtr.end(), nullptr);
    }

    float tempoSyncRatio{1};

    // Transport state, refreshed each host buffer from the CLAP transport. songPosSeconds
    // re-anchors to hostSongPosSeconds at the top of each buffer (songPosNeedsResync) and
    // then advances by blockSize/hostSampleRate per engine block, so it stays sample-locked
    // to the host across wide buffers. When the host is stopped, gives no seconds timeline,
    // or supplies no transport at all, isPlayingAndHasSecondsTimeline is false and the clock
    // simply free-runs by the same per-block advance, a monotonic clock for the life of the
    // engine.
    bool isPlayingAndHasSecondsTimeline{false};
    bool songPosNeedsResync{false};
    double hostSongPosSeconds{0.0};
    double songPosSeconds{0.0};

    float pitchBend{0.f};
    std::array<int8_t, 128> midiCC;
    std::array<float, 128> midiCCFloat; // 0...1 normed
    float channelAT{0.f};

    bool attackFloorOnRetrig{true};
    bool designModeRunAll{false};

    std::array<float *, numMacros> macroPtr;

    MTSClient *mtsClient{nullptr};

    sst::basic_blocks::tables::EqualTuningProvider tuningProvider;
    sst::basic_blocks::tables::TwoToTheXProvider twoToTheX;
    sst::basic_blocks::tables::DbToLinearProvider dbToLinear;

    sst::basic_blocks::dsp::RNG rng;

    ModMatrixConfig modMatrixConfig;

    SRProvider sr;

    // Per-engine-block hoists of mono unison params, refreshed by Synth::processInternal
    // so each Voice::renderBlock can derive uniRatioMul / uniPanShift without per-voice
    // table lookups. unisonSpreadFactorMinus1 = 2^unisonSpread.value - 1.
    float unisonSpreadFactorMinus1{0.f};
    float unisonPanScalar{0.f};

    float audioInBlock alignas(16)[blockSize]{}; // engine-rate audio in, mono mix

    // Anti-alias ceiling for the per-voice noise source = active bit-rate-crusher
    // Nyquist (f_z/2), or 0 when the crusher is off (no limit). Derived in
    // Synth::reapplyControlSettings so it can never drift from bitRateZOH's rate.
    float noiseBandLimitHz{0.f};

    // Instance-scoped MPE config — lives on the engine, NOT in the patch. Persisted
    // via Synth::DawExtraState so DAW sessions round-trip without polluting patches.
    bool mpeActive{false};
    int mpeBendRange{24};

    // Engine-wide smoothing times, in milliseconds. Mirrored from Synth::DawExtraState.
    // midiCCSmoothingTimeMs governs MIDI CC, MPE (bend/pressure/timbre), and per-voice
    // note-expression lags. paramAutomationSmoothingTimeMs governs the per-param lag
    // applied to host parameter automation.
    float midiCCSmoothingTimeMs{25.f};
    float paramAutomationSmoothingTimeMs{2.f};
};
}; // namespace baconpaul::six_sines
#endif // MONO_VALUES_H
