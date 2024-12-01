#include <iostream>
#include "sst/basic-blocks/tables/SincTableProvider.h"
#include <cstdint>

#include "sintable.h"

int main(int, char **)
{
    baconpaul::fm::SinTable st;

    uint32_t phase;

    auto sr = 96000.0;
    auto fr = 440.0;

    auto dph = st.dPhase(fr, sr);
    uint32_t ph = 0;

    for (int i = 0; i < 500; ++i)
    {
        auto res = st.at(ph);
        auto ans = sin(1.0 * ph / (1 << 26) * 2.0 * M_PI);
        if (i % 10 == 0)
        {
            std::cout << "i=" << i
                      << " sf=" << res << " sv=" << ans << " diff=" << 1e5 * (ans - res) << " x1e-5"
            << std::endl;
        }

        ph += dph;
    }

    std::cout << "hw" << std::endl;
}
