#include "catch2/catch2.hpp"
#include "synth/patch.h"
#include "presets/preset-manager.h"
#include "clap/clap.h"
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(sixsines_patches);

using namespace baconpaul::six_sines;

namespace baconpaul::six_sines
{
extern const clap_plugin *makePlugin(const clap_host *h, bool multiOut);
}

namespace
{
clap_host_t makeFactoryTestHost()
{
    clap_host_t host{};
    host.clap_version = CLAP_VERSION;
    host.name = "Six Sines Factory Patch Test";
    host.vendor = "Test";
    host.url = "";
    host.version = "0";
    host.get_extension = [](const clap_host_t *, const char *) -> const void * { return nullptr; };
    host.request_restart = [](const clap_host_t *) {};
    host.request_process = [](const clap_host_t *) {};
    host.request_callback = [](const clap_host_t *) {};
    return host;
}
} // namespace

TEST_CASE("All factory patches load", "[factory]")
{
    // Patch construction touches DSP tables that are populated as a side-effect
    // of plugin init. Bring up a plugin to initialize them.
    auto host = makeFactoryTestHost();
    auto *plugin = baconpaul::six_sines::makePlugin(&host, false);
    REQUIRE(plugin != nullptr);
    plugin->init(plugin);

    auto fs = cmrc::sixsines_patches::get_filesystem();
    const auto factoryPath = std::string(presets::PresetManager::factoryPath);

    size_t count{0};
    for (const auto &cat : fs.iterate_directory(factoryPath))
    {
        if (!cat.is_directory())
            continue;
        auto catPath = factoryPath + "/" + cat.filename();
        for (const auto &p : fs.iterate_directory(catPath))
        {
            auto fname = p.filename();
            auto fullPath = catPath + "/" + fname;

            auto file = fs.open(fullPath);
            auto data = std::string(file.begin(), file.end());

            Patch patch;
            INFO("Loading " << fullPath);
            REQUIRE(patch.fromState(data));
            ++count;
        }
    }

    REQUIRE(count > 0);

    plugin->destroy(plugin);
}
