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
                                                           { return MixerNode(this->src[i]); }))
{
}

void Voice::attack()
{
    out.attack();
    for (auto &n : mixerNode)
        n.attack();
    for (auto &n : src)
    {
        n.reset();
    }

    gated = true;
}

void Voice::renderBlock()
{
    src[0].setFrequency(440.0 * pow(2.0, (key - 69) / 12.0));
    src[0].renderBlock();
    mixerNode[0].renderBlock(gated);
    out.renderBlock(gated);
}

} // namespace baconpaul::fm