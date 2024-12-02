//
// Created by Paul Walker on 12/2/24.
//

#ifndef MATRIX_NODE_H
#define MATRIX_NODE_H

#include "sst/basic-blocks/modulators/DAHDSREnvelope.h"
#include "dsp/op_source.h"
#include "dsp/sr_provider.h"

namespace baconpaul::fm
{
struct MatrixNode
{
  OpSource &onto, &from;
  SRProvider sr;
  MatrixNode(OpSource &on, OpSource &fr) : onto(on), from(fr), env(&sr) {}

  sst::basic_blocks::modulators::DAHDSREnvelope<SRProvider, blockSize> env;

  float fmLevel{0.0};

  void attack()
  {
    env.attack(0.2);
  }

  void applyBlock(bool gated)
  {
    env.processBlock01AD(0.2, 0.4, 0.05, 0.3, 0.3, 0.3, gated);
    for (int j = 0; j < blockSize; ++j)
    {
      onto.phaseInput[0][j] = (int32_t)((2 << 26) * fmLevel * env.outputCache[j] * from.output[0][j]);
      onto.phaseInput[1][j] = (int32_t)((2 << 26) * fmLevel * env.outputCache[j] * from.output[1][j]);
    }
  }
};
}

#endif //MATRIX_NODE_H
