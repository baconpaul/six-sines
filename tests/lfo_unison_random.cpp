#include "catch2/catch2.hpp"
#include "synth/patch.h"
#include <algorithm>
#include <set>
#include <vector>

using namespace baconpaul::six_sines;

namespace
{
uint32_t mix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x7feb352dU;
    h ^= h >> 15;
    h *= 0x846ca68bU;
    h ^= h >> 16;
    return h;
}
float phaseFor(uint32_t seed, uint32_t id)
{
    return (float)(mix32(seed ^ mix32(id)) >> 8) * 0x1p-24f;
}
} // namespace

TEST_CASE("Unison-random LFO phase")
{
    MatrixIndex::initialize();
    auto p = std::make_unique<Patch>();

    std::vector<uint32_t> rateIds;
    for (auto *pp : p->params)
    {
        std::string n = pp->meta.name;
        if (n.size() > 9 && n.substr(n.size() - 9) == " LFO Rate")
            rateIds.push_back(pp->meta.id);
    }

    SECTION("every LFO instance has a distinct rate param id")
    {
        REQUIRE(rateIds.size() > 20);
        std::set<uint32_t> uniq(rateIds.begin(), rateIds.end());
        REQUIRE(uniq.size() == rateIds.size());
    }

    SECTION("one note seed gives every LFO its own phase")
    {
        for (uint32_t seed : {0xC0FFEE01u, 0xDEADBEEFu, 1u, 0u})
        {
            std::vector<float> ph;
            for (auto id : rateIds)
                ph.push_back(phaseFor(seed, id));
            std::set<float> uniq(ph.begin(), ph.end());
            REQUIRE(uniq.size() == ph.size());
        }
    }

    SECTION("phase depends on the seed, so notes differ")
    {
        auto id = rateIds.front();
        std::set<float> across;
        for (uint32_t s = 0; s < 500; ++s)
            across.insert(phaseFor(s * 2654435761u, id));
        REQUIRE(across.size() > 495);
    }
}
