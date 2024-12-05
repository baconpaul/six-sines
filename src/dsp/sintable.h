/*
 * BaconPaul's FM Atrocity
 *
 * A mess, with FM.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#ifndef BACONPAUL_FMTHING_DSP_SINTABLE_H
#define BACONPAUL_FMTHING_DSP_SINTABLE_H

#include "configuration.h"

namespace baconpaul::fm
{
struct SinTable
{
    static constexpr size_t nPoints{1 << 12};
    float quadrantTable[4][nPoints + 1];
    float dQuadrantTable[4][nPoints + 1];

    float cubicHermiteCoefficients[4][nPoints];

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
        auto lb = ph & mask;
        auto ub = (ph >> 12) & mask;
        auto pit = (ph >> 24);
        auto pitQ = pit & 0x3;

        auto y1 = quadrantTable[pitQ][ub];
        auto y2 = quadrantTable[pitQ][ub + 1];
        auto dy1 = dQuadrantTable[pitQ][ub];
        auto dy2 = dQuadrantTable[pitQ][ub + 1];
        ;

        auto c0 = cubicHermiteCoefficients[0][lb];
        auto c1 = cubicHermiteCoefficients[1][lb];
        auto c2 = cubicHermiteCoefficients[2][lb];
        auto c3 = cubicHermiteCoefficients[3][lb];

        auto res = c0 * y1 + c1 * dy1 + c2 * y2 + c3 * dy2;

        return res;
    }
};
} // namespace baconpaul::fm
#endif // SINTABLE_H
