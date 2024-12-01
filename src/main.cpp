#include <iostream>
#include "sst/basic-blocks/tables/SincTableProvider.h"
#include <cstdint>

#include "sintable.h"
#include "RIFFWavWriter.h"
#include <cassert>

int main(int, char **)
{
    baconpaul::fm::SinTable st;

    uint32_t phase;

    auto sr = 96000.0;
    auto fr = 440.0;

    auto dph = st.dPhase(fr, sr);
    uint32_t ph = 0;

    baconpaul::fm::RIFFWavWriter writer("test.wav", 2);
    writer.openFile();
    assert(writer.isOpen());
    writer.writeRIFFHeader();
    writer.writeFMTChunk(sr);
    writer.startDataChunk();
    float smp[2];

    for (int i = 0; i < 500000; ++i)
    {
        if ((i+1) % 50000 == 0)
        {
            std::cout << pow(2.0, 1.0/12.0) << std::endl;;
            fr *= pow(2.0, 1.0 / 12.0);
            dph = st.dPhase(fr, sr);
        }

        auto res = st.at(ph);
        auto ans = sin(1.0 * ph / (1 << 26) * 2.0 * M_PI);
        smp[0] = res;
        smp[1] = res;
        writer.pushSamples(smp);

        ph += dph;
    }

    writer.closeFile();
    std::cout << "hw" << std::endl;
}
