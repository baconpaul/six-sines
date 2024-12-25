/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
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

        NUM_WAVEFORMS
    };

    static constexpr size_t nPoints{1 << 12};
    float quadrantTable[NUM_WAVEFORMS][4][nPoints + 1];
    float dQuadrantTable[NUM_WAVEFORMS][4][nPoints + 1];

    float cubicHermiteCoefficients[4][nPoints];
    float linterpCoefficients[2][nPoints];

    SIMD_M128
        simdFullQuad alignas(16)[NUM_WAVEFORMS][4 * nPoints]; // for each quad it is q, q+1, dq + 1
    SIMD_M128 *simdQuad;
    SIMD_M128 simdCubic alignas(16)[nPoints]; // it is cq, cq+1, cdq, cd1+1

    SinTable()
    {
        simdQuad = simdFullQuad[0];
        memset(quadrantTable, 0, sizeof(quadrantTable));
        memset(dQuadrantTable, 0, sizeof(dQuadrantTable));

        // Waveform 0: Pure sine
        for (int i = 0; i < nPoints + 1; ++i)
        {
            for (int Q = 0; Q < 4; ++Q)
            {
                auto s = sin((1.0 * i / (nPoints - 1) + Q) * M_PI / 2.0);
                auto c = cos((1.0 * i / (nPoints - 1) + Q) * M_PI / 2.0);
                quadrantTable[0][Q][i] = s;
                dQuadrantTable[0][Q][i] = c / (nPoints - 1);
            }
        }

        // Waveform 1: sin(x) ^ 5. Derivative is 5 sin(x)^4 cos(x)
        for (int i = 0; i < nPoints + 1; ++i)
        {
            for (int Q = 0; Q < 4; ++Q)
            {
                auto s = sin((1.0 * i / (nPoints - 1) + Q) * M_PI / 2.0);
                auto c = cos((1.0 * i / (nPoints - 1) + Q) * M_PI / 2.0);
                quadrantTable[1][Q][i] = s * s * s * s * s;
                dQuadrantTable[1][Q][i] = 5 * s * s * s * s * c / (nPoints - 1);
            }
        }

        for (int i = 0; i < nPoints; ++i)
        {
            auto t = 1.f * i / (1 << 12);

            auto c0 = 2 * t * t * t - 3 * t * t + 1;
            auto c1 = t * t * t - 2 * t * t + t;
            auto c2 = -2 * t * t * t + 3 * t * t;
            auto c3 = t * t * t - t * t;

            cubicHermiteCoefficients[0][i] = c0;
            cubicHermiteCoefficients[1][i] = c1;
            cubicHermiteCoefficients[2][i] = c2;
            cubicHermiteCoefficients[3][i] = c3;

            linterpCoefficients[0][i] = 1.f - t;
            linterpCoefficients[1][i] = t;
        }

        // Load the SIMD buffers
        for (int i = 0; i < nPoints; ++i)
        {
            for (int WF = 0; WF < NUM_WAVEFORMS; ++WF)
            {
                for (int Q = 0; Q < 4; ++Q)
                {
                    float r alignas(16)[4];
                    r[0] = quadrantTable[WF][Q][i];
                    r[1] = dQuadrantTable[WF][Q][i];
                    r[2] = quadrantTable[WF][Q][i + 1];
                    r[3] = dQuadrantTable[WF][Q][i + 1];
                    simdFullQuad[WF][nPoints * Q + i] = SIMD_MM(load_ps)(r);
                }
            }

            {
                float r alignas(16)[4];
                for (int j = 0; j < 4; ++j)
                {
                    r[j] = cubicHermiteCoefficients[j][i];
                }
                simdCubic[i] = SIMD_MM(load_ps)(r);
            }
        }
    }

    void setWaveForm(WaveForm wf)
    {
        auto stwf = size_t(wf);
        if (stwf >= NUM_WAVEFORMS) // mostly remove ine during dev
            stwf = 0;
        simdQuad = simdFullQuad[stwf];
    }

    static constexpr double frToPhase{(1 << 26) / gSampleRate};
    uint32_t dPhase(float fr)
    {
        auto dph = (uint32_t)(fr * frToPhase);
        return dph;
    }

    // phase is 26 bits, 12 of fractional, 12 of position in the table and 2 of quadrant
    float at(uint32_t ph)
    {
        static constexpr uint32_t mask{(1 << 12) - 1};
        static constexpr uint32_t umask{(1 << 14) - 1};

        auto lb = ph & mask;
        auto ub = (ph >> 12) & umask;

#define CUBIC_HERMITE 1
#if CUBIC_HERMITE
        auto q = simdQuad[ub];
        auto c = simdCubic[lb];
        auto r = SIMD_MM(mul_ps(q, c));
        auto h = SIMD_MM(hadd_ps)(r, r);
        auto v = SIMD_MM(hadd_ps)(h, h);
        return SIMD_MM(cvtss_f32)(v);
        ;
#endif

#if LINEAR
        auto lb = ph & mask;
        auto ub = (ph >> 12) & mask;
        auto pitQ = (ph >> 24) & 0x3;
        auto y1 = quadrantTable[pitQ][ub];
        auto y2 = quadrantTable[pitQ][ub + 1];

        auto c0 = linterpCoefficients[0][lb];
        auto c1 = linterpCoefficients[1][lb];

        return c0 * y1 + c1 * y2;
#endif
    }
};
} // namespace baconpaul::six_sines
#endif // SINTABLE_H
