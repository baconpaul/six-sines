/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024-2025, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work will be
 * released under GPL3.
 *
 * The source code and license are at https://github.com/baconpaul/six-sines
 */

#include "catch2/catch2.hpp"

#include <cstring>
#include <string>

#include "clap/preset-discovery-impl.h"

using namespace baconpaul::six_sines;

namespace
{
static constexpr const char *providerId = "org.baconpaul.six-sines.preset-discovery";

struct TestIndexer
{
    clap_preset_discovery_indexer_t indexer{};
    int filetypes{0};
    int locations{0};
    int presets{0};

    TestIndexer(const char *name, const char *vendor)
    {
        indexer.clap_version = CLAP_VERSION;
        indexer.name = name;
        indexer.vendor = vendor;
        indexer.indexer_data = this;
        indexer.declare_filetype = [](auto *i, auto *)
        {
            static_cast<TestIndexer *>(i->indexer_data)->filetypes++;
            return true;
        };
        indexer.declare_location = [](auto *i, auto *)
        {
            static_cast<TestIndexer *>(i->indexer_data)->locations++;
            return true;
        };
        indexer.declare_soundpack = [](auto *, auto *) { return true; };
        indexer.get_extension = [](auto *, auto *) -> const void * { return nullptr; };
    }
};

clap_preset_discovery_metadata_receiver_t makeReceiver(TestIndexer &ti)
{
    clap_preset_discovery_metadata_receiver_t r{};
    r.receiver_data = &ti;
    r.on_error = [](auto *, auto, auto *) {};
    r.begin_preset = [](auto *r, auto *, auto *)
    {
        static_cast<TestIndexer *>(r->receiver_data)->presets++;
        return true;
    };
    r.add_plugin_id = [](auto *, auto *) {};
    r.set_soundpack_id = [](auto *, auto *) {};
    r.set_flags = [](auto *, auto) {};
    r.add_creator = [](auto *, auto *) {};
    r.set_description = [](auto *, auto *) {};
    r.set_timestamps = [](auto *, auto, auto) {};
    r.add_feature = [](auto *, auto *) {};
    r.add_extra_info = [](auto *, auto *, auto *) {};
    return r;
}
} // namespace

TEST_CASE("Preset discovery survives repeated create and destroy")
{
    auto fac = sixSinesPresetDiscoveryFactory();
    REQUIRE(fac);
    REQUIRE(fac->count(fac) == 1);

    int firstPresetCount{-1};
    for (int i = 0; i < 3; ++i)
    {
        INFO("cycle " << i);
        TestIndexer ti("Test Host", "Test Vendor");
        auto prov = fac->create(fac, &ti.indexer, providerId);
        REQUIRE(prov);

        REQUIRE(prov->init(prov));
        REQUIRE(ti.filetypes == 1);
        REQUIRE(ti.locations == 2);

        auto rec = makeReceiver(ti);
        REQUIRE(prov->get_metadata(prov, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, &rec));
        REQUIRE(ti.presets > 0);
        if (firstPresetCount < 0)
            firstPresetCount = ti.presets;
        REQUIRE(ti.presets == firstPresetCount);

        prov->destroy(prov);
    }
}

TEST_CASE("Preset discovery refuses blocked hosts")
{
    auto fac = sixSinesPresetDiscoveryFactory();

    struct Case
    {
        const char *name;
        const char *vendor;
        bool initOk;
    };
    auto c = GENERATE(Case{"Test Host", "Test Vendor", true}, Case{"Test Host", nullptr, true},
                      Case{nullptr, nullptr, true}, Case{"Test Host", "PreSonus", false},
                      Case{"Test Host", "FENDER Musical Instruments", false},
                      Case{"Studio One by presonus", nullptr, false},
                      Case{"Fender Studio", "Test Vendor", false});

    INFO("name '" << (c.name ? c.name : "(null)") << "' vendor '"
                  << (c.vendor ? c.vendor : "(null)") << "'");
    TestIndexer ti(c.name, c.vendor);
    auto prov = fac->create(fac, &ti.indexer, providerId);
    REQUIRE(prov);
    REQUIRE(prov->init(prov) == c.initOk);
    if (!c.initOk)
        REQUIRE(ti.locations == 0);
    prov->destroy(prov);
}
