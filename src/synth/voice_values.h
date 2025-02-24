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

#ifndef BACONPAUL_SIX_SINES_SYNTH_VOICE_VALUES_H
#define BACONPAUL_SIX_SINES_SYNTH_VOICE_VALUES_H

#include <sst/basic-blocks/tables/EqualTuningProvider.h>
#include <sst/basic-blocks/tables/TwoToTheXProvider.h>
#include "sst/basic-blocks/dsp/Lag.h"

struct MTSClient;

namespace baconpaul::six_sines
{
struct VoiceValues
{
    VoiceValues() : gated(gatedV), key(keyV) {}

    const bool &gated;
    float gatedFloat, ungatedFloat;
    void setGated(bool g)
    {
        gatedV = g;
        gatedFloat = g ? 1.f : 0.f;
        ungatedFloat = 1.0 - gatedFloat;
    }
    void setKey(int k)
    {
        keyV = k;
        keytrackFrom60 = (k - 60) / 12.0;
    }
    const int &key;
    float keytrackFrom60;
    int channel{0};
    float velocity{0}, releaseVelocity{0};

    float polyAt{0};

    float portaDiff{0}, dPorta{0}, portaFrac{0}, dPortaFrac{0};
    int portaSign{0};

    float mpeBendInSemis{0}, mpeBendNormalized{0}, mpeTimbre{0}, mpePressure{0};

    float noteExpressionTuningInSemis{0}, noteExpressionPanBipolar{0};

    float uniRatioMul{1.0};
    float uniPanShift{0.0};
    int uniIndex{0};
    int uniCount{1};
    float uniPMScale{0.f}; // -1 to 1 for unison field
    bool hasCenterVoice{false}, isCenterVoice{false};
    bool phaseRandom{false}, rephaseOnRetrigger{false};

    sst::basic_blocks::dsp::OnePoleLag<float, false> velocityLag;

  private:
    bool gatedV{false};
    int keyV{0};
};
};     // namespace baconpaul::six_sines
#endif // MONO_VALUES_H
