#include <iostream>
#include "sst/basic-blocks/tables/SincTableProvider.h"
#include <cstdint>

int main(int, char **)
{
    static constexpr size_t nPoints{1 << 12};
    float quadrantTable[4][nPoints + 1];
    float dQuadrantTable[4][nPoints + 1];

    float cubicHermiteCoefficients[4][nPoints];

    for (int i = 0; i < nPoints + 1; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            auto s = sin((1.0 * i / (nPoints - 1) + j) * M_PI / 2.0);
            auto c = cos((1.0 * i / (nPoints - 1) + j) * M_PI / 2.0);
            quadrantTable[j][i] = s;
            dQuadrantTable[j][i] = c / (nPoints-1);
        }
    }

    for (int i=0; i<nPoints; ++i)
    {
        auto t = 1.f * i / (1<<12);

        auto c0 = 2 * t * t * t - 3 * t * t + 1;
        auto c1 = t * t * t - 2 * t * t + t;
        auto c2 = -2 * t * t * t + 3 * t * t;
        auto c3 = t * t * t - t * t;

        cubicHermiteCoefficients[0][i] = c0;
        cubicHermiteCoefficients[1][i] = c1;
        cubicHermiteCoefficients[2][i] = c2;
        cubicHermiteCoefficients[3][i] = c3;

    }

    uint32_t phase;

    auto sr = 96000.0;
    auto fr = 31.7;

    auto dph = (uint32_t)(fr / sr * (1 << 24));

    uint32_t mask = (1 << 12) - 1;
    uint32_t ph = 0;

    for (int i = 0; i < 5000; ++i)
    {
        auto lb = ph & mask;
        auto ub = (ph >> 12) & mask;
        auto pit = (ph >> 24);
        auto pitQ = pit & 0x3;

        auto y1 = quadrantTable[pitQ][ub];
        auto y2 = quadrantTable[pitQ][ub+1];
        auto dy1 = dQuadrantTable[pitQ][ub];
        auto dy2 = dQuadrantTable[pitQ][ub+1];;

        auto c0 = cubicHermiteCoefficients[0][lb];
        auto c1 = cubicHermiteCoefficients[1][lb];
        auto c2 = cubicHermiteCoefficients[2][lb];
        auto c3 = cubicHermiteCoefficients[3][lb];

        auto res = c0 * y1 + c1 * dy1 + c2 * y2 + c3 * dy2;

        auto ans = sin(1.0 * ph / (1 << 26) * 2.0 * M_PI);
        if (i % 100 == 0)
        {
            std::cout << "i=" << i << " ub=" << ub << " lb=" << lb << " pit=" << pit << std::hex
                      << " pitQ=" << pitQ << std::dec
                      << " sf=" << res << " sv=" << ans << " diff=" << 1e5 * (ans - res) << " x1e-5"
            << std::endl;
        }

        ph += dph;
    }

    std::cout << "hw" << std::endl;
}
