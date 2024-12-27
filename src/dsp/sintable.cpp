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

#include "sintable.h"

namespace baconpaul::six_sines
{
thread_local double SinTable::xTable[nQuadrants][nPoints + 1];
thread_local float SinTable::quadrantTable[NUM_WAVEFORMS][nQuadrants][nPoints + 1];
thread_local float SinTable::dQuadrantTable[NUM_WAVEFORMS][nQuadrants][nPoints + 1];

thread_local float SinTable::cubicHermiteCoefficients[nQuadrants][nPoints];
thread_local float SinTable::linterpCoefficients[2][nPoints];

thread_local SIMD_M128 SinTable::simdFullQuad alignas(
    16)[NUM_WAVEFORMS][nQuadrants * nPoints]; // for each quad it is q, q+1, dq + 1
thread_local SIMD_M128 SinTable::simdCubic alignas(16)[nPoints]; // it is cq, cq+1, cdq, cd1+1

thread_local bool SinTable::staticsInitialized{false};

void SinTable::fillTable(int WF, std::function<std::pair<double, double>(double x, int Q)> der)
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

void SinTable::initializeStatics()
{
    if (staticsInitialized)
        return;

    SXSNLOG("Iniitalizing Sintable");

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

    fillTable(WaveForm::SIN_OF_CUBED,
              [](double x, int Q)
              {
                  auto z = x * 2 - 1;
                  auto dzdx = 2;
                  auto v = sin(twoPi * z * z * z);
                  auto dvdz = 3 * twoPi * z * z * cos(twoPi * z * z * z);
                  auto dv = dvdz * dzdx;

                  // Above is a downward saw and we want upward
                  return std::make_pair(v, dv);
              });

    fillTable(SinTable::WaveForm::TX2,
              [](double x, int Q)
              {
                  double v, dv;
                  auto s = sin(twoPi * x);
                  auto c = cos(twoPi * x);

                  switch (Q)
                  {
                  case 0:
                      v = 1 - c;
                      dv = twoPi * s;
                      break;
                  case 1:
                      v = 1 + c;
                      dv = -twoPi * s;
                      break;
                  case 2:
                      v = -1 - c;
                      dv = twoPi * s;
                      break;
                  case 3:
                      v = c - 1;
                      dv = -twoPi * s;
                      break;
                  }

                  return std::make_pair(v, dv);
              });

    fillTable(SinTable::WaveForm::TX3,
              [](double x, int Q)
              {
                  double v, dv;
                  auto s = sin(twoPi * x);
                  auto c = cos(twoPi * x);

                  switch (Q)
                  {
                  case 0:
                  case 1:
                      v = s;
                      dv = twoPi * c;
                      break;
                  case 2:
                  case 3:
                      v = 0;
                      dv = 0;
                      break;
                  }

                  return std::make_pair(v, dv);
              });

    fillTable(SinTable::WaveForm::TX4,
              [](double x, int Q)
              {
                  double v, dv;
                  auto s = sin(twoPi * x);
                  auto c = cos(twoPi * x);

                  switch (Q)
                  {
                  case 0:
                      v = 1 - c;
                      dv = twoPi * s;
                      break;
                  case 1:
                      v = 1 + c;
                      dv = -twoPi * s;
                      break;
                  case 2:
                  case 3:
                      v = 0;
                      dv = 0;
                      break;
                  }

                  return std::make_pair(v, dv);
              });

    fillTable(SinTable::WaveForm::TX5,
              [](double x, int Q)
              {
                  double v, dv;
                  auto s = sin(2 * twoPi * x);
                  auto c = cos(2 * twoPi * x);

                  switch (Q)
                  {
                  case 0:
                  case 1:
                      v = s;
                      dv = 2 * twoPi * c;
                      break;
                  case 2:
                  case 3:
                      v = 0;
                      dv = 0;
                      break;
                  }

                  return std::make_pair(v, dv);
              });

    fillTable(SinTable::WaveForm::TX6,
              [](double x, int Q)
              {
                  double v{0}, dv{0};
                  auto s = sin(2 * twoPi * x);
                  auto c = cos(2 * twoPi * x);

                  auto OCT = Q * 2;
                  if (x > .125 && Q == 0)
                      OCT++;
                  else if (x > .375 && Q == 1)
                      OCT++;

                  switch (OCT)
                  {
                  case 0:
                      v = 1 - c;
                      dv = 2 * twoPi * s;
                      break;
                  case 1:
                      v = 1 + c;
                      dv = -2 * twoPi * s;
                      break;
                  case 2:
                      v = -1 - c;
                      dv = 2 * twoPi * s;
                      break;
                  case 3:
                      v = c - 1;
                      dv = -2 * twoPi * s;
                      break;
                  default:
                      break;
                  }

                  return std::make_pair(v, dv);
              });

    fillTable(SinTable::WaveForm::TX7,
              [](double x, int Q)
              {
                  double v, dv;
                  auto s = sin(2 * twoPi * x);
                  auto c = cos(2 * twoPi * x);

                  switch (Q)
                  {
                  case 0:
                      v = s;
                      dv = 2 * twoPi * c;
                      break;
                  case 1:
                      v = -s;
                      dv = -2 * twoPi * c;
                      break;
                  case 2:
                  case 3:
                      v = 0;
                      dv = 0;
                      break;
                  }

                  return std::make_pair(v, dv);
              });

    fillTable(SinTable::WaveForm::TX8,
              [](double x, int Q)
              {
                  double v{0}, dv{0};
                  auto s = sin(2 * twoPi * x);
                  auto c = cos(2 * twoPi * x);

                  auto OCT = Q * 2;
                  if (x > .125 && Q == 0)
                      OCT++;
                  else if (x > .375 && Q == 1)
                      OCT++;

                  switch (OCT)
                  {
                  case 0:
                      v = 1 - c;
                      dv = 2 * twoPi * s;
                      break;
                  case 1:
                      v = 1 + c;
                      dv = -2 * twoPi * s;
                      break;
                  case 2:
                      v = 1 + c;
                      dv = -2 * twoPi * s;
                      break;
                  case 3:
                      v = -c + 1;
                      dv = 2 * twoPi * s;
                      break;
                  default:
                      break;
                  }

                  return std::make_pair(v, dv);
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
    staticsInitialized = true;
}

} // namespace baconpaul::six_sines