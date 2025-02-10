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

#include "sintable.h"

namespace baconpaul::six_sines
{
double SinTable::xTable[nQuadrants][nPoints + 1];
float SinTable::quadrantTable[NUM_WAVEFORMS][nQuadrants][nPoints + 1];
float SinTable::dQuadrantTable[NUM_WAVEFORMS][nQuadrants][nPoints + 1];

float SinTable::cubicHermiteCoefficients[nQuadrants][nPoints];
float SinTable::linterpCoefficients[2][nPoints];

SIMD_M128 SinTable::simdFullQuad alignas(
    16)[NUM_WAVEFORMS][nQuadrants * nPoints];       // for each quad it is q, q+1, dq + 1
SIMD_M128 SinTable::simdCubic alignas(16)[nPoints]; // it is cq, cq+1, cdq, cd1+1

bool SinTable::staticsInitialized{false};

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
    // ALl in x < 0 < 1
    // a = 1 - 2 x
    // b = sin 6pi x
    // c = sin(pi (32 (x - 0.5)^6 + 0.5))
    // res = b + c * (a-b)
    // dres = db + dc (a-b) + c (da - db)

    static constexpr double sqrFreq{4};
    static constexpr double twoPiS{sqrFreq * twoPi};
    auto osp = 1.0 / (twoPiS);
    auto co = osp * acos(-2 * osp) / (twoPi /* rad->0.1 units */);

    fillTable(WaveForm::SAWISH,
              [co](double x, int Q)
              {
                  auto a = 1.0 - 2 * x;
                  auto da = -2;

                  auto b = sin(6 * M_PI * x);
                  auto db = 6 * M_PI * cos(6 * M_PI * x);

                  auto c = sin(M_PI * (32 * pow((x - 0.5), 6) + 0.5));
                  // gross
                  auto eps = 0.00001;
                  auto cp = sin(M_PI * (32 * pow((x + eps - 0.5), 6) + 0.5));
                  auto cm = sin(M_PI * (32 * pow((x - eps - 0.5), 6) + 0.5));
                  auto dc = (cp - cm) / 2 * eps;

                  auto v = b + c * (a - b);
                  auto dv = db + dc * (a - b) + c * (da - db);

                  // Convert to upward saw

                  return std::make_pair(-v, -dv);
              });

    fillTable(WaveForm::TRIANGLE,
              [](double x, int Q)
              {
                  if (Q == 0)
                  {
                      return std::make_pair(4 * x, 4.0);
                  }
                  else if (Q == 3)
                  {
                      return std::make_pair(4 * x - 4, 4.0);
                  }
                  else
                  {
                      return std::make_pair(2.0 - 4.0 * x, -4.0);
                  }
                  return std::make_pair(0.0, 0.0);
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

    fillTable(SinTable::TX2,
              [](double x, int Q) -> std::pair<double, double>
              {
                  auto v = 0.0;
                  auto dv = 0.0;
                  if (Q == 0 || Q == 1)
                  {
                      v = 0.5 * (sin(4.0 * M_PI * (x - 0.125)) + 1);
                      dv = 2.0 * M_PI * cos(4.0 * M_PI * (x - 0.125));
                  }
                  else
                  {
                      v = -0.5 * (sin(4.0 * M_PI * (x - 0.125)) + 1);
                      dv = -2.0 * M_PI * cos(4.0 * M_PI * (x - 0.125));
                  }
                  return {v, dv};
              });

    fillTable(SinTable::WaveForm::SPIKY_TX2,
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

    fillTable(SinTable::TX4,
              [](double x, int Q) -> std::pair<double, double>
              {
                  auto v = 0.0;
                  auto dv = 0.0;
                  if (Q == 0 || Q == 1)
                  {
                      v = 0.5 * (sin(4.0 * M_PI * (x - 0.125)) + 1);
                      dv = 2.0 * M_PI * cos(4.0 * M_PI * (x - 0.125));
                  }

                  return {v, dv};
              });

    fillTable(SinTable::WaveForm::SPIKY_TX4,
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
              [](double x, int Q) -> std::pair<double, double>
              {
                  auto v = 0.0;
                  auto dv = 0.0;
                  if (Q == 0)
                  {
                      v = 0.5 * (sin(8.0 * M_PI * (x - 0.0625)) + 1);
                      dv = 4.0 * M_PI * cos(8.0 * M_PI * (x - 0.0625));
                  }
                  else if (Q == 1)
                  {
                      v = -0.5 * (sin(8.0 * M_PI * (x - 0.0625)) + 1);
                      dv = -4.0 * M_PI * cos(8.0 * M_PI * (x - 0.0625));
                  }
                  return {v, dv};
              });
    fillTable(SinTable::WaveForm::SPIKY_TX6,
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
              [](double x, int Q) -> std::pair<double, double>
              {
                  auto v = 0.0;
                  auto dv = 0.0;
                  if (Q == 0 || Q == 1)
                  {
                      v = 0.5 * (sin(8.0 * M_PI * (x - 0.0625)) + 1);
                      dv = 4.0 * M_PI * cos(8.0 * M_PI * (x - 0.0625));
                  }

                  return {v, dv};
              });
    fillTable(SinTable::WaveForm::SPIKY_TX8,
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

    // Thanks to https://en.wikipedia.org/wiki/Window_function for these
    // HANN: 0.5 * (1-cos 2pix). Derivative is pi sin 2pix
    fillTable(SinTable::WaveForm::HANN_WINDOW,
              [](double x, int Q)
              {
                  auto v = 0.5 * (1.0 - cos(2.0 * M_PI * x));
                  auto dv = M_PI * sin(2.0 * M_PI * x);
                  return std::make_pair(v, dv);
              });

    auto cosSum = [](double x, double a0, double a1, double a2, double a3,
                     double a4) -> std::pair<double, double>
    {
        auto v = a0 - a1 * cos(twoPi * x) + a2 * cos(2 * twoPi * x) - a3 * cos(3 * twoPi * x) +
                 a4 * cos(4 * twoPi * x);
        auto dv = -a1 * twoPi * sin(twoPi * x) + a2 * 2 * twoPi * sin(2 * twoPi * x) -
                  a3 * 3 * twoPi * sin(3 * twoPi * x) + a4 * 4 * twoPi * sin(4 * twoPi * x);
        ;
        return std::make_pair(v, dv);
    };
    fillTable(SinTable::WaveForm::BLACKMAN_HARRIS_WINDOW, [cosSum](double x, int Q)
              { return cosSum(x, 0.35875, 0.48829, 0.14128, 0.01168, 0.00196); });
    fillTable(SinTable::WaveForm::HALF_BLACKMAN_HARRIS_WINDOW,
              [cosSum](double x, int Q)
              {
                  if (Q == 2 || Q == 3)
                  {
                      return std::make_pair(0.0, 0.0);
                  }
                  auto res = cosSum(x * 2, 0.35875, 0.48829, 0.14128, 0.01168, 0.00196);
                  return std::make_pair(res.first, res.second * 2);
              });

    // Tukey with alpha 0.15
    fillTable(SinTable::WaveForm::TUKEY_WINDOW,
              [](double x, int Q)
              {
                  static constexpr float alpha{0.15};
                  auto dSign{1.0};
                  if (Q == 2 || Q == 3)
                  {
                      x = 1.0 - x;
                      dSign = -1.0;
                  }
                  auto v{0.0}, dv{1.0};
                  if (x < alpha / 2)
                  {
                      v = 0.5 * (1 - cos(twoPi * x / alpha));
                      dv = 0.5 * twoPi * sin(twoPi * x / alpha) / alpha;
                  }
                  else
                  {
                      v = 1.0;
                      dv = 0.0;
                  }
                  return std::make_pair(v, dSign * dv);
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