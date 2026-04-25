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

#ifndef BACONPAUL_SIX_SINES_DSP_REMAP_FUNCTIONS_H
#define BACONPAUL_SIX_SINES_DSP_REMAP_FUNCTIONS_H

#include <cstdint>
#include <algorithm>

#include "dsp/sintable.h" // baconpaul::six_sines::phase constants

namespace baconpaul::six_sines::remap
{
using namespace baconpaul::six_sines::phase;

// Uniform clamp range applied to m by every remap function. Keeps slopes finite
// and held segments visible at the extremes; tweak both bounds in one place.
static constexpr float mMin = 0.03f;
static constexpr float mMax = 0.97f;

/*
 * CZ-style two-segment "saw" phase remap.
 * Float reference:
 *     phi_b = 0.5 * (1 - m);                  // m clamped away from 0 and 1
 *     out   = (phi < phi_b) ? phi / (2 * phi_b)
 *                           : 0.5 + (phi - phi_b) / (2 * (1 - phi_b));
 *
 * Implementation notes:
 * - Input phase is assumed already in [0, phaseMax).
 * - The two slopes collapse to a single reciprocal-multiply per call: only the
 *   taken branch's reciprocal is computed, so the hot path is one compare,
 *   one reciprocal, one multiply, one cast.
 * - Final mask keeps the result in 26-bit space against float-rounding overshoot
 *   when phase approaches phaseMax.
 */
inline uint32_t remapSaw(uint32_t phase, float m)
{
    m = std::clamp(m, mMin, mMax);
    const float phi_b_f = 0.5f * (1.0f - m);
    const uint32_t phi_b = static_cast<uint32_t>(phi_b_f * phaseMaxF);

    uint32_t out;
    if (phase < phi_b)
    {
        out = static_cast<uint32_t>(phase * (0.5f / phi_b_f));
    }
    else
    {
        out = halfPhase + static_cast<uint32_t>((phase - phi_b) * (0.5f / (1.0f - phi_b_f)));
    }
    return out & phaseMask;
}

/*
 * Square: linear ramp [0,1) over the leading 1-m fraction of the cycle, then hold at 1.
 * Float ref:
 *     out = min(phi / (1 - m), 1).
 */
inline uint32_t remapSquare(uint32_t phase, float m)
{
    m = std::clamp(m, mMin, mMax);
    const float span = 1.0f - m;
    const uint32_t spanInt = static_cast<uint32_t>(span * phaseMaxF);

    if (phase < spanInt)
        return static_cast<uint32_t>(phase * (1.0f / span)) & phaseMask;
    return phaseMask;
}

/*
 * Pulse: hold at output 1/4 (sine peak) for the leading m fraction of the cycle, then sweep the
 * remaining 1-m fraction through a full output cycle starting from 1/4 and wrapping back to it.
 * Float ref:
 *     if (phi < m) return 0.25;
 *     u = (phi - m) / (1 - m);   // 0..1 across the active span
 *     f = 0.25 + u;              // 1/4 .. 5/4
 *     if (f >= 1) f -= 1;        // wrap into [0, 1)
 *     return f;
 */
inline uint32_t remapPulse(uint32_t phase, float m)
{
    m = std::clamp(m, mMin, mMax);
    const uint32_t mInt = static_cast<uint32_t>(m * phaseMaxF);
    constexpr uint32_t quarterPhase = phaseMax >> 2;

    if (phase < mInt)
        return quarterPhase;

    const float invOneMinusM = 1.0f / (1.0f - m);
    const auto uInt = static_cast<uint32_t>((phase - mInt) * invOneMinusM);
    return (quarterPhase + uInt) & phaseMask;
}

/*
 * Double saw: three-segment piecewise-linear sweep that ramps fast 0 -> 1/4 over [0, a),
 * slow 1/4 -> 3/4 over [a, b), then fast 3/4 -> 1 over [b, 1), with a = 0.25*(1-m), b = 1-a.
 * Float ref:
 *     a = 0.25 * (1 - m);  b = 1 - a;
 *     if (x < a)  return x       / (1 - m);
 *     if (x < b)  return 0.25 + (x - a) / (1 + m);
 *     return            0.75 + (x - b) / (1 - m);
 */
inline uint32_t remapDoubleSaw(uint32_t phase, float m)
{
    m = std::clamp(m, mMin, mMax);
    constexpr uint32_t quarterPhase = phaseMax >> 2;
    constexpr uint32_t threeQuarterPhase = (phaseMax >> 2) * 3;

    const float a = 0.25f * (1.0f - m);
    const float b = 1.0f - a;
    const uint32_t aInt = static_cast<uint32_t>(a * phaseMaxF);
    const uint32_t bInt = static_cast<uint32_t>(b * phaseMaxF);
    const float invSteep = 1.0f / (1.0f - m);   // segments 1 and 3 (slope 1/(1-m))
    const float invShallow = 1.0f / (1.0f + m); // segment 2 (slope 1/(1+m))

    if (phase < aInt)
        return static_cast<uint32_t>(phase * invSteep) & phaseMask;
    if (phase < bInt)
        return (quarterPhase + static_cast<uint32_t>((phase - aInt) * invShallow)) & phaseMask;
    return (threeQuarterPhase + static_cast<uint32_t>((phase - bInt) * invSteep)) & phaseMask;
}

/*
 * Sin-to-square: hold at sine peak (1/4) for time a, ramp down through zero to sine valley (3/4)
 * over time b, hold at the valley for time a, ramp back up to the peak (wrapping at 1) over time b.
 * As m goes 0 -> 1 the held plateaus widen and the ramps steepen, morphing sine into square.
 * Float ref:
 *     a = 0.5*m;  b = 0.5*(1-m);  invTwoB = 1/(1-m);
 *     if (phi < a)              return 0.25;
 *     if (phi < a + b)          return 0.25 + (phi - a) * invTwoB;
 *     if (phi < 2*a + b)        return 0.75;
 *     return 0.75 + (phi - (2*a + b)) * invTwoB;     // wraps past 1 back to 0.25
 */
inline uint32_t remapSinToSquare(uint32_t phase, float m)
{
    m = std::clamp(m, mMin, mMax);
    constexpr uint32_t quarterPhase = phaseMax >> 2;
    constexpr uint32_t threeQuarterPhase = (phaseMax >> 2) * 3;

    const float a = 0.5f * m;
    const float b = 0.5f * (1.0f - m);
    const uint32_t aInt = static_cast<uint32_t>(a * phaseMaxF);
    const uint32_t abInt = static_cast<uint32_t>((a + b) * phaseMaxF);
    const uint32_t aabInt = static_cast<uint32_t>((2.0f * a + b) * phaseMaxF);
    const float invTwoB = 1.0f / (1.0f - m);

    if (phase < aInt)
        return quarterPhase;
    if (phase < abInt)
        return (quarterPhase + static_cast<uint32_t>((phase - aInt) * invTwoB)) & phaseMask;
    if (phase < aabInt)
        return threeQuarterPhase;
    return (threeQuarterPhase + static_cast<uint32_t>((phase - aabInt) * invTwoB)) & phaseMask;
}

/*
 * Double sine: two ramps with silent (held) segments, generating two sine cycles per fundamental.
 * Segments: ramp 0 -> 1/2 over [0, half), hold at 1/2 over [half, 1/2),
 *           ramp 1/2 -> 1 over [1/2, tail), hold at 1 over [tail, 1),
 *           where half = 0.5*(1-m), tail = 1 - 0.5*m.
 */
inline uint32_t remapDoubleSine(uint32_t phase, float m)
{
    m = std::clamp(m, mMin, mMax);
    const float oneMinusM = 1.0f - m;
    const float half = 0.5f * oneMinusM;
    const float tail = 1.0f - 0.5f * m;
    const uint32_t halfInt = static_cast<uint32_t>(half * phaseMaxF);
    const uint32_t tailInt = static_cast<uint32_t>(tail * phaseMaxF);
    const float invOneMinusM = 1.0f / oneMinusM;

    if (phase < halfInt)
        return static_cast<uint32_t>(phase * invOneMinusM) & phaseMask;
    if (phase < halfPhase)
        return halfPhase;
    if (phase < tailInt)
        return (halfPhase + static_cast<uint32_t>((phase - halfPhase) * invOneMinusM)) & phaseMask;
    return phaseMask;
}

} // namespace baconpaul::six_sines::remap
#endif
