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
#include "synth/matrix_index.h"
#include "synth/patch.h"

namespace baconpaul::fm
{

namespace scpu = sst::cpputils;

Voice::Voice(const Patch &p)
    : out(p.output, mixerNode), output{out.output[0], out.output[1]},
      src(scpu::make_array_lambda<OpSource, numOps>([this, &p](auto i)
                                                    { return OpSource(p.sourceNodes[i]); })),
      mixerNode(scpu::make_array_lambda<MixerNode, numOps>(
          [this, &p](auto i) { return MixerNode(p.mixerNodes[i], this->src[i]); })),
      selfNode(scpu::make_array_lambda<MatrixNodeSelf, numOps>(
          [this, &p](auto i) { return MatrixNodeSelf(p.selfNodes[i], this->src[i]); })),
      matrixNode(scpu::make_array_lambda<MatrixNodeFrom, matrixSize>(
          [&p, this](auto i) {
              return MatrixNodeFrom(p.matrixNodes[i], this->targetAtMatrix(i),
                                    this->sourceAtMatrix(i));
          }))
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
    for (auto &n : matrixNode)
        n.attack();

    gated = true;
}

void Voice::renderBlock()
{
    for (auto &s : src)
        s.zeroInputs();

    auto baseFreq = 440.0 * pow(2.0, (key - 69) / 12.0);

    for (int i = 0; i < numOps; ++i)
    {
        src[i].setBaseFrequency(baseFreq);
        src[i].renderBlock();
        mixerNode[i].renderBlock(gated);
    }

    out.renderBlock(gated);
}

/*
 * Indexing works as follows.
 * n-1 sources onto n oscillators
 * upper diagonal
 *
 *   SRC
 * T 0 1  2  3  4  5
 * 0 x 0  1  2  3  4
 * 1   x  5  6  7  8
 * 2      x 10 11 12
 * 3         x 13 14
 * 4           x  15
 * 5               x
 *
 * for 6 operators 6 * 5 / 2 = 15 so this tracks
 *
 * OK so how do you get hat lookup quickly? Well I think
 * you just make a static table and look up to be honest
 */

static_assert(numOps == 6, "Rebuild this table if not");

OpSource &Voice::sourceAtMatrix(size_t pos) { return src[MatrixIndex::sourceIndexAt(pos)]; }
OpSource &Voice::targetAtMatrix(size_t pos) { return src[MatrixIndex::targetIndexAt(pos)]; }

} // namespace baconpaul::fm