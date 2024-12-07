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

#include "voice.h"
#include "sst/cpputils/constructors.h"

namespace baconpaul::fm
{

namespace scpu = sst::cpputils;

Voice::Voice()
    : out(mixerNode), output{out.output[0], out.output[1]},
      mixerNode(scpu::make_array_lambda<MixerNode, numOps>([this](auto i)
                                                           { return MixerNode(this->src[i]); })),
    selfNode(scpu::make_array_lambda<MatrixNodeSelf, numOps>([this](auto i)
                                                           { return MatrixNodeSelf(this->src[i]); }))
{
    std::fill(isKeytrack.begin(), isKeytrack.end(), true);
    std::fill(cmRatio.begin(), cmRatio.end(), 1.0);
}

void Voice::attack()
{
    out.attack();
    for (auto &n : mixerNode)
        n.attack();
    for (auto &n : src)
        n.reset();
    for (auto &n : selfNode)
        n.attack();

    gated = true;
}

void Voice::renderBlock()
{
    auto baseFreq = 440.0 * pow(2.0, (key - 69) / 12.0);
    for (int i=0; i<numOps; ++i)
    {
        selfNode[i].applyBlock(gated);

        src[i].setFrequency(baseFreq * cmRatio[i]);
        src[i].renderBlock();
    }
    mixerNode[0].renderBlock(gated);
    out.renderBlock(gated);
}

} // namespace baconpaul::fm