/*
 * Six Sines A Sinnin'
 *
 * A mess, with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#ifndef BACONPAUL_SIX_SINES_DSP_SINTABLE_H
#define BACONPAUL_SIX_SINES_DSP_SINTABLE_H

#include "configuration.h"
#include <sst/basic-blocks/simd/setup.h>

namespace baconpaul::six_sines
{
struct SinTable
{
    static constexpr size_t nPoints{1 << 12};
    float quadrantTable[4][nPoints + 1];
    float dQuadrantTable[4][nPoints + 1];

    float cubicHermiteCoefficients[4][nPoints];
    float linterpCoefficients[2][nPoints];

    SIMD_M128 simdQuad alignas(16)[4 * nPoints]; // for each quad it is q, q+1, dq + 1
    SIMD_M128 simdCubic alignas(16)[nPoints];    // it is cq, cq+1, cdq, cd1+1

    SinTable()
    {
        for (int i = 0; i < nPoints + 1; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                auto s = sin((1.0 * i / (nPoints - 1) + j) * M_PI / 2.0);
                auto c = cos((1.0 * i / (nPoints - 1) + j) * M_PI / 2.0);
                quadrantTable[j][i] = s;
                dQuadrantTable[j][i] = c / (nPoints - 1);
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

        for (int i = 0; i < nPoints; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                float r alignas(16)[4];
                r[0] = quadrantTable[j][i];
                r[1] = dQuadrantTable[j][i];
                r[2] = quadrantTable[j][i + 1];
                r[3] = dQuadrantTable[j][i + 1];
                simdQuad[nPoints * j + i] = SIMD_MM(load_ps)(r);
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
