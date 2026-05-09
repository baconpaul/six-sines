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

#include "synth/patch.h"

namespace baconpaul::six_sines
{
struct NoiseHelper
{
    using NoiseType = Patch::SourceNode::NoiseType;

    // Tilt filter takes ±tiltMaxDb across the bipolar N range.
    static constexpr float tiltMaxDb{6.f};

    struct Host
    {
        const sst::basic_blocks::tables::DbToLinearProvider *db{nullptr};
        double sri{1.0 / 48000.0};
        float dbToLinear(float dbVal) { return db->dbToLinear(dbVal); }
        float getSampleRateInv() { return static_cast<float>(sri); }
    } host;

    sst::basic_blocks::dsp::PinkNoise pinkNoise;
    sst::filters::FastTiltNoiseFilter<Host> tiltFilter;
    sst::basic_blocks::dsp::RNG &rng;

    NoiseHelper(sst::basic_blocks::dsp::RNG &r,
                const sst::basic_blocks::tables::DbToLinearProvider &dbProv)
        : pinkNoise(r.unifU32()), tiltFilter(host), rng(r)
    {
        host.db = &dbProv;
    }

    void setSampleRate(double sr) { host.sri = 1.0 / sr; }

    // Map the patch's unipolar [0,1] N value onto a bipolar tilt gain in dB.
    static float nToTiltDb(float n) { return (n * 2.f - 1.f) * tiltMaxDb; }

    // Push 11 white samples through the tilt filter to prime its history.
    void warmup()
    {
        float w[11];
        for (int i = 0; i < 11; ++i)
            w[i] = rng.unifPM1();
        tiltFilter.init(w, nToTiltDb(0.5f));
    }

    // Refill 16 samples of the chosen noise color into buf, normalized to ~±1.
    // CHIP_LFSR is silent for now until the LFSR generator lands.
    void fill16(float buf[16], NoiseType type, float nValue)
    {
        switch (type)
        {
        case NoiseType::WHITE:
            for (int i = 0; i < 16; ++i)
                buf[i] = rng.unifPM1();
            break;
        case NoiseType::PINK:
            pinkNoise.generate16(buf);
            break;
        case NoiseType::TILT:
        {
            // Slope is half the user-visible tilt; positive tilt also gets a linear
            // -4 dB/tiltDb attenuation to compensate for the high-shelf gain stack.
            auto tiltDb = std::clamp(nToTiltDb(nValue), -tiltMaxDb, tiltMaxDb);
            tiltFilter.setCoeff(tiltDb * 0.5f);
            auto atten = (tiltDb > 0.f) ? host.dbToLinear(-4.f * tiltDb) : 1.f;
            for (int i = 0; i < 16; ++i)
            {
                buf[i] = rng.unifPM1();
                sst::filters::FastTiltNoiseFilter<Host>::step(tiltFilter, buf[i]);
                buf[i] *= atten;
            }
            break;
        }
        case NoiseType::CHIP_LFSR:
            for (int i = 0; i < 16; ++i)
                buf[i] = 0.f;
            break;
        }
    }
};
} // namespace baconpaul::six_sines

#endif
