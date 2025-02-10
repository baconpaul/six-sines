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

#include "preset-discovery-impl.h"
#include "configuration.h"
#include "presets/preset-manager.h"
#include "sst/plugininfra/paths.h"

namespace baconpaul::six_sines
{

namespace prov
{
struct Provider
{
    presets::PresetManager pm{nullptr};
    const clap_preset_discovery_indexer_t *indexer{nullptr};
};
bool init(const struct clap_preset_discovery_provider *provider)
{
    SXSNLOG("Init");

    auto pm = static_cast<Provider *>(provider->provider_data);

    struct clap_preset_discovery_filetype sxsnp
    {
        "Six Sines Preset", "For Six Sines!", "sxsnp"
    };
    pm->indexer->declare_filetype(pm->indexer, &sxsnp);

    struct clap_preset_discovery_location fac
    {
        CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT, "Six Sines Factory Content",
            CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr
    };
    pm->indexer->declare_location(pm->indexer, &fac);

    // User Areas
    auto loc = pm->pm.userPatchesPath.u8string();
    struct clap_preset_discovery_location userLoc
    {
        CLAP_PRESET_DISCOVERY_IS_USER_CONTENT, "Six Sines User Content",
            CLAP_PRESET_DISCOVERY_LOCATION_FILE, loc.c_str()
    };
    pm->indexer->declare_location(pm->indexer, &userLoc);

    return true;
}

// Destroys the preset provider
void destroy(const struct clap_preset_discovery_provider *provider)
{
    SXSNLOG("Destroy");
    auto pm = static_cast<Provider *>(provider->provider_data);
    delete pm;
}

// reads metadata from the given file and passes them to the metadata receiver
// Returns true on success.
bool get_metadata(const struct clap_preset_discovery_provider *provider, uint32_t location_kind,
                  const char *location, const clap_preset_discovery_metadata_receiver_t *mdr)
{
    SXSNLOG("Get Metadata " << location_kind);
    auto pm = static_cast<Provider *>(provider->provider_data);

    if (location_kind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN)
    {
        // factory content
        for (auto &fac : pm->pm.factoryPatchVector)
        {
            SXSNLOG("Begin Preset " << fac.second.c_str());
            if (!mdr->begin_preset(mdr, fac.second.c_str(), fac.first.c_str()))
            {
                SXSNLOG("Begin Preset failed");
                return false;
            }
            std::string desc =
                std::string() + "A Six Sines '" + fac.first.c_str() + "' Factory Preset";
            clap_universal_plugin_id_t clp{"clap", "org.baconpaul.six-sines"};
            mdr->add_plugin_id(mdr, &clp);

            mdr->set_description(mdr, desc.c_str());
        }

        return true;
    }

    fs::path p = fs::path{fs::u8path(location)};
    p = p.replace_extension("");
    p = p.filename();

    SXSNLOG("Begin Preset " << p.u8string());
    if (!mdr->begin_preset(mdr, p.u8string().c_str(), ""))
    {
        SXSNLOG("Begin Preset failed");
        return false;
    }
    clap_universal_plugin_id_t clp{"clap", "org.baconpaul.six-sines"};
    mdr->add_plugin_id(mdr, &clp);
    mdr->set_description(mdr, "A Six Sines User Preset");

    return true;
}

// Query an extension.
// The returned pointer is owned by the provider.
// It is forbidden to call it before provider->init().
// You can call it within provider->init() call, and after.
const void *get_extension(const struct clap_preset_discovery_provider *provider,
                          const char *extension_id)
{
    SXSNLOG("Get Extension " << extension_id);
    return nullptr;
}
} // namespace prov
namespace fac
{
uint32_t count(const clap_preset_discovery_factory_t *factory) { return 1; };
const clap_preset_discovery_provider_descriptor_t *
get_descriptor(const clap_preset_discovery_factory_t *factory, uint32_t index)
{
    static struct clap_preset_discovery_provider_descriptor desc = {
        CLAP_VERSION, "org.baconpaul.six-sines.preset-discovery", "Six Sines Preset Discovery",
        "BaconPaul"};
    return &desc;
}
const clap_preset_discovery_provider_t *create(const clap_preset_discovery_factory_t *factory,
                                               const clap_preset_discovery_indexer_t *indexer,
                                               const char *provider_id)
{
    if (strcmp(provider_id, "org.baconpaul.six-sines.preset-discovery") == 0)
    {
        static auto pm = new prov::Provider{};
        pm->indexer = indexer;
        SXSNLOG("Create " << pm->pm.userPatchesPath.u8string());
        static struct clap_preset_discovery_provider provider = {
            get_descriptor(nullptr, 0), pm, prov::init, prov::destroy, prov::get_metadata,
            prov::get_extension};
        return &provider;
    }
    return nullptr;
}

} // namespace fac
const clap_preset_discovery_factory_t *sixSinesPresetDiscoveryFactory()
{
    SXSNLOG("Factory Create");
    static clap_preset_discovery_factory_t factory = {fac::count, fac::get_descriptor, fac::create};
    return &factory;
}
} // namespace baconpaul::six_sines