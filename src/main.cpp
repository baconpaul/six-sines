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
    auto fr = 55.0;

    auto frac = 3.7;
    auto dph = st.dPhase(fr, sr);
    auto dph2 = st.dPhase(fr * frac, sr);
    uint32_t ph = 4 << 27;
    uint32_t cph = 4 << 27;

    baconpaul::fm::RIFFWavWriter writer("test.wav", 2);
    writer.openFile();
    assert(writer.isOpen());
    writer.writeRIFFHeader();
    writer.writeFMTChunk(sr);
    writer.startDataChunk();
    float smp[2];

    auto tri = [](auto f)
    {
        if (f < 0.5)
            return f * 2;
        else
            return (1-f) * 2;
    };

    for (int i = 0; i < 800000; ++i)
    {
        if ((i+1) % 50000 == 0)
        {
            fr *= pow(2.0, 4.0 / 12.0);
            dph = st.dPhase(fr, sr);
            dph2 = st.dPhase(fr * frac, sr);
        }

        auto p1 = st.at(cph) * tri(1.f * (i % 50000) / 50000);
        cph += dph2;

        auto res = st.at(ph + (int32_t) (p1 * (1<<26)));
        smp[0] = res;
        smp[1] = res;
        writer.pushSamples(smp);

        ph += dph;
    }

    writer.closeFile();
    std::cout << "hw" << std::endl;
}
