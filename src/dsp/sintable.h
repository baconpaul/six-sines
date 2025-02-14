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

#ifndef BACONPAUL_SIX_SINES_DSP_SINTABLE_H
#define BACONPAUL_SIX_SINES_DSP_SINTABLE_H

#include <cassert>
#include <cstring>
#include <functional>
#include <utility>

#include "configuration.h"
#include <sst/basic-blocks/simd/setup.h>

namespace baconpaul::six_sines
{
struct SinTable
{
    enum WaveForm
    {
        SIN = 0, // these stream so you know....
        SIN_FIFTH,
        SQUARISH,
        SAWISH,
        TRIANGLE,
        SIN_OF_CUBED,

        TX2,
        TX3,
        TX4,
        TX5,
        TX6,
        TX7,
        TX8,

        SPIKY_TX2,
        SPIKY_TX4,
        SPIKY_TX6,
        SPIKY_TX8,

        HANN_WINDOW,
        BLACKMAN_HARRIS_WINDOW,
        HALF_BLACKMAN_HARRIS_WINDOW,
        TUKEY_WINDOW,

        NUM_WAVEFORMS
    };

    static constexpr size_t nPoints{1 << 12}, nQuadrants{4};
    static double xTable[nQuadrants][nPoints + 1];
    static float quadrantTable[NUM_WAVEFORMS][nQuadrants][nPoints + 1];
    static float dQuadrantTable[NUM_WAVEFORMS][nQuadrants][nPoints + 1];

    static float cubicHermiteCoefficients[nQuadrants][nPoints];
    static float linterpCoefficients[2][nPoints];

    static SIMD_M128 simdFullQuad alignas(
        16)[NUM_WAVEFORMS][nQuadrants * nPoints];    // for each quad it is q, q+1, dq + 1
    static SIMD_M128 simdCubic alignas(16)[nPoints]; // it is cq, cq+1, cdq, cd1+1
    static bool staticsInitialized;

    SIMD_M128 *simdQuad;

    SinTable()
    {
        initializeStatics();
        simdQuad = simdFullQuad[0];
    }

    void setSampleRate(double sr) { frToPhase = (1 << 26) / sr; }
    static void fillTable(int WF, std::function<std::pair<double, double>(double x, int Q)> der);
    static void initializeStatics();

    void setWaveForm(WaveForm wf)
    {
        auto stwf = size_t(wf);
        if (stwf >= NUM_WAVEFORMS) // mostly remove ine during dev
            stwf = 0;
        simdQuad = simdFullQuad[stwf];
    }

    double frToPhase{0};
    inline int32_t dPhase(float fr) const
    {
        auto dph = (int32_t)(fr * frToPhase);
        return dph;
    }

    // phase is 26 bits, 12 of fractional, 12 of position in the table and 2 of quadrant
    inline float at(const uint32_t ph) const
    {
        static constexpr uint32_t mask{(1 << 12) - 1};
        static constexpr uint32_t umask{(1 << 14) - 1};

        auto lb = ph & mask;
        auto ub = (ph >> 12) & umask;

        auto q = simdQuad[ub];
        auto c = simdCubic[lb];
        auto r = SIMD_MM(mul_ps(q, c));

        auto h = SIMD_MM(hadd_ps)(r, r);
        auto v = SIMD_MM(hadd_ps)(h, h);
        return SIMD_MM(cvtss_f32)(v);
    }
};
} // namespace baconpaul::six_sines
#endif // SINTABLE_H
