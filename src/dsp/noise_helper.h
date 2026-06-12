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

#ifndef BACONPAUL_SIX_SINES_DSP_NOISE_HELPER_H
#define BACONPAUL_SIX_SINES_DSP_NOISE_HELPER_H

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "sst/basic-blocks/dsp/PinkNoise.h"
#include "sst/basic-blocks/dsp/RNG.h"
#include "sst/basic-blocks/tables/DbToLinearProvider.h"
#include "sst/filters/FastTiltNoiseFilter.h"
#include "sst/filters/ButterworthLPHP.h"

#include "synth/patch.h"

namespace baconpaul::six_sines
{
struct NoiseHelper
{
    using NoiseType = Patch::SourceNode::NoiseType;
    using LFSRMode = Patch::SourceNode::LFSRMode;

    // This wierd constant is roughly the frequency at which the thing would turn over at 48khz in
    // a CPU running at 1.789 mhz and a cycle length of 96 cpu cycles.
    static constexpr float lfsrFreeReferenceHz{1.789 * 1000000 / 48000 * 96};

    static constexpr float lfsrTuningRange{12.f};

    // Tilt filter takes ±tiltMaxDb across the bipolar N range.
    static constexpr float tiltMaxDb{6.f};

    struct Config
    {
        const sst::basic_blocks::tables::DbToLinearProvider *db{nullptr};
        double sr{48000.0};
        double sri{1.0 / 48000.0};
        float dbToLinear(float dbVal) { return db->dbToLinear(dbVal); }
        float getSampleRateInv() { return static_cast<float>(sri); }
    } config;

    sst::basic_blocks::dsp::PinkNoise pinkNoise;
    sst::filters::FastTiltNoiseFilter<Config> tiltFilter;
    sst::basic_blocks::dsp::RNG &rng;

    // Anti-alias LP applied to the continuous noise colors (white/pink/tilt) ahead of
    // the global bit-rate ZOH, so the ZOH has nothing above f_z/2 to fold (which would
    // whiten the tilt). Cutoff tracks MonoValues::noiseBandLimitHz; 0 = no limit.
    sst::filters::ButterworthLP<8> bandLimitFilter;
    float lastBandLimitHz{-1.f};

    // 15-bit Galois LFSR shared by both chip modes. Seeded non-zero per helper
    // so simultaneous voices don't lock-step.
    uint16_t lfsrReg{0x0001};
    double lfsrPhaseAcc{0.0};

    NoiseHelper(sst::basic_blocks::dsp::RNG &r,
                const sst::basic_blocks::tables::DbToLinearProvider &dbProv)
        : pinkNoise(r.unifU32()), tiltFilter(config), rng(r)
    {
        config.db = &dbProv;
        lfsrReg = static_cast<uint16_t>((r.unifU32() & 0x7FFFu) | 0x0001u);
    }

    void setSampleRate(double sr)
    {
        config.sr = sr;
        config.sri = 1.0 / sr;
    }

    // Map the patch's unipolar [0,1] N value onto a bipolar tilt gain in dB.
    static float nToTiltDb(float n) { return (n * 2.f - 1.f) * tiltMaxDb; }

    // Anti-alias the 16-sample noise block to cutoffHz (== crusher Nyquist). 0 = no limit.
    // Coeffs only recompute when the cutoff changes (rare); reset on change for stability.
    void bandLimit16(float buf[16], float cutoffHz)
    {
        if (cutoffHz <= 0.f)
            return;
        // Empirically at 48khz ZOH the re-sample to audio output still reflects so scootch down a
        // bit
        cutoffHz = std::min(cutoffHz, 20000.f);
        if (cutoffHz != lastBandLimitHz)
        {
            bandLimitFilter.setCutoffAndSampleRate(cutoffHz, static_cast<float>(config.sr));
            bandLimitFilter.reset();
            lastBandLimitHz = cutoffHz;
        }
        bandLimitFilter.processBlock(buf, 16);
    }

    // Push 11 white samples through the tilt filter to prime its history.
    void warmup()
    {
        float w[11];
        for (int i = 0; i < 11; ++i)
            w[i] = rng.unifPM1();
        tiltFilter.init(w, nToTiltDb(0.5f));
        bandLimitFilter.reset();
        lastBandLimitHz = -1.f;
    }

    // Refill 16 samples of the chosen noise color into buf, normalized to ~±1.
    // baseFreq drives the LFSR shift clock when the mode is keytracked; otherwise
    // a fixed reference (lfsrFreeReferenceHz) is used.
    void fill16(float buf[16], NoiseType type, float nValue, float baseFreq, LFSRMode lfsrMode,
                float bandLimitHz)
    {
        switch (type)
        {
        case NoiseType::WHITE:
            for (int i = 0; i < 16; ++i)
                buf[i] = rng.unifPM1();
            bandLimit16(buf, bandLimitHz);
            break;
        case NoiseType::PINK:
            pinkNoise.generate16(buf);
            bandLimit16(buf, bandLimitHz);
            break;
        case NoiseType::TILT:
        {
            // Slope is half the user-visible tilt; positive tilt also gets a linear
            // -4 dB/tiltDb attenuation to compensate for the high-shelf gain stack.
            auto tiltDb = std::clamp(nToTiltDb(nValue), -tiltMaxDb, tiltMaxDb);
            tiltFilter.setCoeff(tiltDb * 0.5f);
            auto atten = (tiltDb > 0.f) ? config.dbToLinear(-4.f * tiltDb) : 1.f;
            for (int i = 0; i < 16; ++i)
            {
                buf[i] = rng.unifPM1();
                sst::filters::FastTiltNoiseFilter<Config>::step(tiltFilter, buf[i]);
                buf[i] *= atten;
            }
            bandLimit16(buf, bandLimitHz);
            break;
        }
        case NoiseType::CHIP_LFSR:
        {
            const bool isShort =
                (lfsrMode == LFSRMode::SHORT || lfsrMode == LFSRMode::SHORT_KEYTRACK);
            const bool keytrack =
                (lfsrMode == LFSRMode::SHORT_KEYTRACK || lfsrMode == LFSRMode::LONG_KEYTRACK);
            const int xorBit = isShort ? 6 : 1;
            const float refFreq = (keytrack ? baseFreq : 261.62) * lfsrFreeReferenceHz / 261.62;
            const float shiftFreq = refFreq * std::exp2((nValue - 0.5f) * lfsrTuningRange);
            const double dPhase = static_cast<double>(shiftFreq) * config.sri;
            for (int i = 0; i < 16; ++i)
            {
                lfsrPhaseAcc += dPhase;
                while (lfsrPhaseAcc >= 1.0)
                {
                    uint16_t fb = ((lfsrReg >> 0) ^ (lfsrReg >> xorBit)) & 1u;
                    lfsrReg = static_cast<uint16_t>((lfsrReg >> 1) | (fb << 14));
                    lfsrPhaseAcc -= 1.0;
                }
                buf[i] = (lfsrReg & 1u) ? 1.f : -1.f;
            }
            break;
        }
        }
    }
};
} // namespace baconpaul::six_sines

#endif
