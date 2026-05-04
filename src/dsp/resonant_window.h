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

#ifndef BACONPAUL_SIX_SINES_DSP_RESONANT_WINDOW_H
#define BACONPAUL_SIX_SINES_DSP_RESONANT_WINDOW_H

#include <cstdint>
#include <cmath>

#include "dsp/sintable.h" // baconpaul::six_sines::phase constants

/*
 * Closed-form window functions for the CZ-style resonant sweep mode.
 *
 * Each window is multiplied by sin(2*pi * k(m) * inner_phase) where
 * inner_phase resets to 0 each time the fundamental phase wraps. The
 * window itself is a function of the fundamental phase and provides
 * the spectral body of the waveform.
 *
 * Float reference forms (phi in [0, 1)):
 *     windowSaw(phi)            = 1 - phi
 *     windowTriangle(phi)       = 1 - |2*phi - 1|
 *     windowTrapezoid(phi)      = phi < 1/2 ? 1 : 2*(1 - phi)
 *     windowFullTrapezoid(phi)  = phi < 1/4 ? 4*phi
 *                               : phi < 3/4 ? 1
 *                               :             4*(1 - phi)
 *
 * Integer-phase signature: phase is a uint32_t in [0, phaseMax),
 * matching the rest of the engine. Output is a float window amplitude
 * in [0, 1]. The caller multiplies this by the inner-phase sine.
 *
 * Hann / Blackman-Harris / Tukey windows are pulled from the SinTable
 * (`OpSource::stWindow`) rather than implemented here.
 */

namespace baconpaul::six_sines::resonant_window
{
using namespace baconpaul::six_sines::phase;

static constexpr float invPhaseMaxF = 1.0f / phaseMaxF;

// Form 5 -- Resonant Saw window
// Descending ramp: 1 at phase = 0, 0 at phase = phaseMax.
// Hard restart at the wrap is the source of the saw-like spectral envelope.
inline float windowSaw(uint32_t phase) { return 1.0f - static_cast<float>(phase) * invPhaseMaxF; }

// Form 6 -- Resonant Triangle window
// Triangle: 0 at phase = 0, peaks at 1 at phase = halfPhase, back to 0 at phase = phaseMax.
// No discontinuity at the wrap; smoother, more vowel-like resonance.
inline float windowTriangle(uint32_t phase)
{
    const float p = static_cast<float>(phase) * invPhaseMaxF;
    return 1.0f - std::fabs(2.0f * p - 1.0f);
}

// Form 7 -- Resonant Trapezoid window
// Flat at 1 for phase in [0, halfPhase), then linear decay to 0 at phase = phaseMax.
// Pulse-like body with a strong formant from the hard restart at phase = 0.
inline float windowTrapezoid(uint32_t phase)
{
    if (phase < halfPhase)
        return 1.0f;
    return 2.0f * (1.0f - static_cast<float>(phase) * invPhaseMaxF);
}

// Symmetric full trapezoid: 0 -> 1 over [0, 1/4), held at 1 over [1/4, 3/4),
// 1 -> 0 over [3/4, 1). No discontinuity at the wrap; flat-topped pulse body.
inline float windowFullTrapezoid(uint32_t phase)
{
    constexpr uint32_t quarterPhase = phaseMax >> 2;
    constexpr uint32_t threeQuarterPhase = quarterPhase * 3;
    if (phase < quarterPhase)
        return 4.0f * static_cast<float>(phase) * invPhaseMaxF;
    if (phase < threeQuarterPhase)
        return 1.0f;
    return 4.0f * (1.0f - static_cast<float>(phase) * invPhaseMaxF);
}

} // namespace baconpaul::six_sines::resonant_window
#endif
