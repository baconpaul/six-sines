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
    baconpaul::fm::MatrixNode node(osrc, osrc2);

    baconpaul::fm::RIFFWavWriter writer("test.wav", 2);
    writer.openFile();
    assert(writer.isOpen());
    writer.writeRIFFHeader();
    writer.writeFMTChunk(baconpaul::fm::gSampleRate);
    writer.startDataChunk();

    auto fr = 55.0;
    osrc.setFrequency(fr);
    osrc2.setFrequency(fr * 2.1);
    node.fmLevel = 0.4;
    node.attack();

    for (int i = 0; i < 80000; ++i)
    {
        if ((i + 1) % 7000 == 0)
        {
            fr *= pow(2.0, 4.0 / 12.0);
            osrc.setFrequency(fr);
            osrc2.setFrequency(fr * 2.1);
            node.attack();
        }

        osrc2.renderBlock();
        node.applyBlock(((i+1)%7000) < 2500);
        osrc.renderBlock();

        for (int j = 0; j < baconpaul::fm::blockSize; ++j)
        {
            float smp[2]{osrc.output[0][j], osrc.output[1][j]};
            writer.pushSamples(smp);
        }
    }

    writer.closeFile();
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
