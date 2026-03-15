#include "catch2/catch2.hpp"
#include "synth/patch.h"
#include "clap/clap.h"
#include "clap/ext/params.h"
#include "clapwrapper/auv2.h"
#include <algorithm>
#include <vector>

using namespace baconpaul::six_sines;

namespace baconpaul::six_sines
{
extern const clap_plugin *makePlugin(const clap_host *h, bool multiOut);
extern const Patch *getPatchFromPlugin(const clap_plugin_t *plugin);
}

namespace
{
clap_host_t makeTestHost()
{
    clap_host_t host{};
    host.clap_version = CLAP_VERSION;
    host.host_data = nullptr;
    host.name = "Six Sines Test Host";
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

/*
 * Verify that the AUV2 parameter ordering exposed by the plugin is stable across versions.
 *
 * The ordering contract: params are sorted by (version ascending, clap id ascending).
 * Parameters added in version 1.1.0 (version_110) come first; later-versioned params
 * naturally sort to the end.
 *
 * The hardcoded count below (expected_110_non_uf0_count) was captured at version 1.1.0.
 * If you add new version_110 parameters, update this count and document why.
 * New parameters added after 1.1.0 should use a version constant > version_110.
 */
TEST_CASE("param_order_110", "[structure]")
{
    // Number params as of v1.1.0.
    // Breakdown: OutputNode=54, SourceNode=46×6=276, SelfNode=35×6=210,
    //            MixerNode=37×6=222, MatrixNode=37×15=555, MacroNode=1×6=6,
    //            FineTuneNode=35, MainPanNode=32  =>  total 1390.
    static constexpr size_t expected_110_non_uf0_count = 1390;

    // Instantiate the plugin first — Patch must live in an initialized engine context.
    auto host = makeTestHost();
    auto *plugin = baconpaul::six_sines::makePlugin(&host, false);
    REQUIRE(plugin != nullptr);
    plugin->init(plugin);

    // Get the patch from the live plugin instance.
    auto *patch = baconpaul::six_sines::getPatchFromPlugin(plugin);
    REQUIRE(patch != nullptr);

    // Anchor: if this fails, a version_110 param was added or removed.
    // Update the count and document the change.
    size_t v110_count = 0;
    for (auto *p : patch->params)
        if (p->meta.version == Patch::version_110)
            ++v110_count;
    REQUIRE(v110_count == expected_110_non_uf0_count);

    // Build the expected AUV2 ordering directly from the Patch (the authoritative
    // source of param IDs).  Sort by (version ascending, id ascending) — same
    // algorithm as auv2_get_param_order — so lower-versioned params come first.
    auto sortedParams = patch->params;
    std::sort(sortedParams.begin(), sortedParams.end(), [](const Param *a, const Param *b) {
        if (a->meta.version != b->meta.version)
            return a->meta.version < b->meta.version;
        return a->meta.id < b->meta.id;
    });

    std::vector<clap_id> expected_ids;
    expected_ids.reserve(sortedParams.size());
    for (auto *p : sortedParams)
        expected_ids.push_back(p->meta.id);

    // Spot checks: verify key boundary positions in the 1.1.0 ordering (sorted by id)
    // OutputNode occupies positions 0-53
    REQUIRE(expected_ids[0]    == 500);    // OutputNode: first param (level)
    REQUIRE(expected_ids[53]   == 720);    // OutputNode: last param (lfoDepth)
    // SourceNode ops start at position 54
    REQUIRE(expected_ids[54]   == 1500);   // SourceNode op0: first param (ratio)
    // SelfNode ops start at position 330
    REQUIRE(expected_ids[330]  == 10000);  // SelfNode op0: first param (fbLevel)
    REQUIRE(expected_ids[475]  == 10405);  // SelfNode op4: mid-range param
    // MatrixNode starts at position 762
    REQUIRE(expected_ids[762]  == 30000);  // MatrixNode entry0: first param (level)
    // MacroNode starts at position 1317
    REQUIRE(expected_ids[1317] == 40000);  // MacroNode macro0: first param (level)
    // MainPanNode ends at position 1389
    REQUIRE(expected_ids[1389] == 51401);  // MainPanNode: last param (envDepth)

    // Call the AUV2 param ordering extension and compare against expected.
    auto *auv2Ext = static_cast<const clap_plugin_auv2_param_ordering_t *>(
        plugin->get_extension(plugin, CLAP_PLUGIN_AUV2_PARAM_ORDERING));
    REQUIRE(auv2Ext != nullptr);

    auto *paramsExt = static_cast<const clap_plugin_params_t *>(
        plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    REQUIRE(paramsExt != nullptr);

    auto paramCount = paramsExt->count(plugin);
    REQUIRE(paramCount == patch->params.size());

    std::vector<size_t> ordering(paramCount);
    REQUIRE(auv2Ext->get_param_order(plugin, ordering.data(), paramCount));

    std::vector<clap_id> actual_ids;
    actual_ids.reserve(paramCount);
    clap_param_info_t info{};
    for (size_t i = 0; i < paramCount; ++i)
    {
        REQUIRE(paramsExt->get_info(plugin, static_cast<uint32_t>(ordering[i]), &info));
        actual_ids.push_back(info.id);
    }

    REQUIRE(actual_ids == expected_ids);

    plugin->destroy(plugin);
}
