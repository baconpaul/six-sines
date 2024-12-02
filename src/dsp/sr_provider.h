//
// Created by Paul Walker on 12/2/24.
//

#ifndef SR_PROVIDER_H
#define SR_PROVIDER_H

#include "configuration.h"

namespace baconpaul::fm
{
struct SRProvider
{
  float envelope_rate_linear_nowrap(float f) { return (blockSize / gSampleRate) * pow(2.0, -f);}
  static constexpr  float samplerate{gSampleRate};
};
}
#endif //SR_PROVIDER_H
