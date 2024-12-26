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

        NUM_WAVEFORMS
    };

    static constexpr size_t nPoints{1 << 12}, nQuadrants{4};
    double xTable[nQuadrants][nPoints + 1];
    float quadrantTable[NUM_WAVEFORMS][nQuadrants][nPoints + 1];
    float dQuadrantTable[NUM_WAVEFORMS][nQuadrants][nPoints + 1];

    float cubicHermiteCoefficients[nQuadrants][nPoints];
    float linterpCoefficients[2][nPoints];

    SIMD_M128
    simdFullQuad alignas(
        16)[NUM_WAVEFORMS][nQuadrants * nPoints]; // for each quad it is q, q+1, dq + 1
    SIMD_M128 *simdQuad;
    SIMD_M128 simdCubic alignas(16)[nPoints]; // it is cq, cq+1, cdq, cd1+1

    void fillTable(int WF, std::function<std::pair<double, double>(double x, int Q)> der)
    {
        static constexpr double dxdPhase = 1.0 / (nQuadrants * (nPoints - 1));
        for (int Q = 0; Q < nQuadrants; ++Q)
        {
            for (int i = 0; i < nPoints + 1; ++i)
            {
                auto [v, dvdx] = der(xTable[Q][i], Q);
                quadrantTable[WF][Q][i] = static_cast<float>(v);
                dQuadrantTable[WF][Q][i] = static_cast<float>(dvdx * dxdPhase);
            }
        }
    }

    SinTable()
    {
        simdQuad = simdFullQuad[0];
        memset(quadrantTable, 0, sizeof(quadrantTable));
        memset(dQuadrantTable, 0, sizeof(dQuadrantTable));

        for (int i = 0; i < nPoints + 1; ++i)
        {
            for (int Q = 0; Q < nQuadrants; ++Q)
            {
                xTable[Q][i] = (1.0 * i / (nPoints - 1) + Q) * 0.25;
            }
        }

        static constexpr double twoPi{2.0 * M_PI};
        // Waveform 0: sin(2pix);
        fillTable(WaveForm::SIN, [](double x, int Q)
                  { return std::make_pair(sin(twoPi * x), twoPi * cos(twoPi * x)); });

        // Waveform 1: sin(2pix)^4. Deriv is 5 2pix sin(2pix)^4 cos(2pix)
        fillTable(WaveForm::SIN_FIFTH,
                  [](double x, int Q)
                  {
                      auto s = sin(twoPi * x);
                      auto c = cos(twoPi * x);
                      auto v = s * s * s * s * s;
                      auto dv = 5 * twoPi * s * s * s * s * c;
                      return std::make_pair(v, dv);
                  });

        // Waveform 2: Square-ish with sin 8 transitions
        fillTable(WaveForm::SQUARISH,
                  [](double x, int Q)
                  {
                      static constexpr double winFreq{8.0};
                      static constexpr double dFr{1.0 / (4 * winFreq)};
                      static constexpr double twoPiF{twoPi * winFreq};
                      float v{0}, dv{0};
                      if (x <= dFr || x > 1.0 - dFr)
                      {
                          v = sin(twoPiF * x);
                          dv = twoPiF * cos(2.0 * M_PI * 4 * x);
                      }
                      else if (x <= 0.5 - dFr)
                      {
                          v = 1.0;
                          dv = 0.0;
                      }
                      else if (x < 0.5 + dFr)
                      {
                          v = -sin(twoPiF * x);
                          dv = twoPiF * cos(2.0 * M_PI * winFreq * x);
                      }
                      else
                      {
                          v = -1.0;
                          dv = 0.0;
                      }
                      return std::make_pair(v, dv);
                  });

        // Waveform 3: Saw-ish with sin 4 transitions
        // What we need is the point where the derivatie of sin n 2pi x = deriv -2x
        // or n 2pi cos(n 2pix) = -2
        // or x = 1/n 2pix * acos(-2/n 2pi)
        static constexpr double sqrFreq{4};
        static constexpr double twoPiS{sqrFreq * twoPi};
        auto osp = 1.0 / (twoPiS);
        auto co = osp * acos(-2 * osp) / (twoPi /* rad->0.1 units */);

        fillTable(WaveForm::SAWISH,
                  [co](double x, int Q)
                  {
                      float v{0}, dv{0};
                      if (x <= co || x > 1.0 - co)
                      {
                          v = sin(twoPiS * x);
                          dv = twoPiS * cos(twoPiS * x);
                      }
                      else
                      {
                          v = 1.0 - 2 * x;
                          dv = -2.0;
                      }

                      // Above is a downward saw and we want upward
                      return std::make_pair(-v, -dv);
                  });

        // Fill up interp buffers
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
