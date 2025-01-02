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

#include "configuration.h"
#include "clap/six-sines-clap-entry-impl.h"
#include "clap/plugin.h"
#include "clapwrapper/vst3.h"
#include "clapwrapper/auv2.h"

#include <iostream>
#include <cstring>
#include <string.h>
#include <clap/clap.h>
#if WIN32
#include <windows.h>
#endif

namespace baconpaul::six_sines
{

extern const clap_plugin *makePlugin(const clap_host *);

/*
 * Clap Factory API
 */
const clap_plugin_descriptor *getDescriptor()
{
    static const char *features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT,
                                     CLAP_PLUGIN_FEATURE_SYNTHESIZER, "Free and Open Source",
                                     "Audio Rate Modulation", nullptr};
    static clap_plugin_descriptor desc = {CLAP_VERSION,
                                          "org.baconpaul.six-sines",
                                          PRODUCT_NAME,
                                          "BaconPaul",
                                          "https://baconpaul.org",
                                          "",
                                          "",
                                          "0.0.1",
                                          "Synth with Audio Rate Modulation or something",
                                          &features[0]};
    return &desc;
}

uint32_t clap_get_plugin_count(const clap_plugin_factory *) { return 1; };
const clap_plugin_descriptor *clap_get_plugin_descriptor(const clap_plugin_factory *f, uint32_t w)
{
    if (w == 0)
    {
        return getDescriptor();
    }

    return nullptr;
}

const clap_plugin *clap_create_plugin(const clap_plugin_factory *f, const clap_host *host,
                                      const char *plugin_id)
{

    if (strcmp(plugin_id, getDescriptor()->id) == 0)
    {
        SXSNLOG("Asked for desc");
        return makePlugin(host);
    }
    return nullptr;
}

/*
 * Clap Wrapper AUV2 Factory API
 */

static bool clap_get_auv2_info(const clap_plugin_factory_as_auv2 *factory, uint32_t index,
                               clap_plugin_info_as_auv2_t *info)
{
    if (index != 0)
        return false;

    strncpy(info->au_type, "aumu", 5); // use the features to determine the type
    strncpy(info->au_subt, "sxSn", 5);

    return true;
}

/*
 * Clap Wrapper VST3 Factory API
 */
static const clap_plugin_info_as_vst3 *clap_get_vst3_info(const clap_plugin_factory_as_vst3 *f,
                                                          uint32_t index)
{
    return nullptr;
}

const void *get_factory(const char *factory_id)
{
    SXSNLOG("Asking for factory [" << factory_id << "]");
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0)
    {
        static const struct clap_plugin_factory six_sines_clap_factory = {
            clap_get_plugin_count,
            clap_get_plugin_descriptor,
            clap_create_plugin,
        };
        return &six_sines_clap_factory;
    }
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_INFO_AUV2) == 0)
    {
        static const struct clap_plugin_factory_as_auv2 six_sines_auv2_factory = {
            "BcPL",      // manu
            "BaconPaul", // manu name
            clap_get_auv2_info};
        return &six_sines_auv2_factory;
    }
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_INFO_VST3) == 0)
    {
        static const struct clap_plugin_factory_as_vst3 six_sines_vst3_factory = {
            "BaconPaul", "https://baconpaul.org", "", clap_get_vst3_info};

        return &six_sines_vst3_factory;
    }
    return nullptr;
}
bool clap_init(const char *p)
{
#if WIN32
#define WIN_DEBUG_CONSOLE_ACTIVE 0
#if WIN_DEBUG_CONSOLE_ACTIVE
    static FILE *confp{nullptr};
    AllocConsole();
    freopen_s(&confp, "CONOUT$", "w", stdout);
    HANDLE hConOut =
        CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
    std::cout.clear();
    std::wcout.clear();
    printf("Hello Console\n");
    std::cout << "Heya" << std::endl;
#endif
#endif
    return true;
}
void clap_deinit() {}
} // namespace baconpaul::six_sines