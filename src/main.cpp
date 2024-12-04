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

#include <iostream>
#include "sst/basic-blocks/tables/SincTableProvider.h"
#include <cstdint>

#include "dsp/matrix_node.h"
#include "dsp/op_source.h"
#include "dsp/sintable.h"
#include "infra/RIFFWavWriter.h"
#include <cassert>

int main(int, char **)
{
    baconpaul::fm::OpSource osrc, osrc2;
    baconpaul::fm::MatrixNodeFrom node(osrc, osrc2);
    baconpaul::fm::MatrixNodeSelf snode(osrc);
    baconpaul::fm::MixerNode outnode(osrc);

    baconpaul::fm::RIFFWavWriter writer("test.wav", 2);
    if (!writer.openFile())
    {
        std::cout << "Cannot open test wav" << std::endl;
        exit(2);
    }
    assert(writer.isOpen());
    writer.writeRIFFHeader();
    writer.writeFMTChunk(baconpaul::fm::gSampleRate);
    writer.startDataChunk();

    auto fr = 55.0;
    osrc.setFrequency(fr);
    osrc2.setFrequency(fr * 2.1);
    node.attack();
    snode.attack();

    int every{7000};
    int gate{2500};
    for (int i = 0; i < 80000; ++i)
    {
        if (i % every == 0)
        {
            fr *= pow(2.0, 4.0 / 12.0);
            osrc.setFrequency(fr);
            osrc2.setFrequency(fr * 2.1);
            node.attack();
            snode.attack();
            outnode.attack();
            if (snode.fbBase > 0)
            {
                snode.fbBase = 0;
                node.fmLevel = 0.5;
            }
            else
            {
                snode.fbBase = 1.0;
                node.fmLevel = 0;
            }
        }

        osrc2.renderBlock();
        node.applyBlock(((i + 1) % every) < gate);

        snode.applyBlock(((i + 1) % every) < gate);
        osrc.renderBlock();

        outnode.renderBlock(((i + 1) % every) < gate);

        for (int j = 0; j < baconpaul::fm::blockSize; ++j)
        {
            float smp[2]{outnode.output[0][j], outnode.output[1][j]};
            writer.pushSamples(smp);
        }
    }

    if (!writer.closeFile())
    {
        std::cout << "Cannot close file" << std::endl;
        exit(1);
    }
    /*
    baconpaul::fm::SinTable st;

    uint32_t phase;

    auto sr = 96000.0;
    auto fr = 55.0;

    auto frac = 3.7;
    auto dph = st.dPhase(fr, sr);
    auto dph2 = st.dPhase(fr * frac, sr);
    uint32_t ph = 4 << 27;
    uint32_t cph = 4 << 27;

    float smp[2];

    auto tri = [](auto f)
    {
        if (f < 0.5)
            return f * 2;
        else
            return (1-f) * 2;
    };

    float fb{0.f};
    bool doFB{false};
    for (int i = 0; i < 800000; ++i)
    {
        if ((i+1) % 50000 == 0)
        {
            fr *= pow(2.0, 4.0 / 12.0);
            dph = st.dPhase(fr, sr);
            dph2 = st.dPhase(fr * frac, sr);
            doFB = !doFB;
        }

        auto p1 = st.at(cph) * tri(1.f * (i % 50000) / 50000) * doFB;
        cph += dph2;

        auto res = st.at(ph + (int32_t) (p1 * (1<<26)) + (int32_t)(fb * (1<<24)));
        fb = res *  tri(1.f * (i % 50000) / 50000) * (!doFB);
        smp[0] = res;
        smp[1] = res;
        writer.pushSamples(smp);

        ph += dph;
    }

    writer.closeFile();
    std::cout << "hw" << std::endl;
    */
}
