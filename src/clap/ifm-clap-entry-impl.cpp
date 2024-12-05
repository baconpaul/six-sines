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

#include "configuration.h"
#include "clap/ifm-clap-entry-impl.h"
#include <iostream>
#include <clap/clap.h>

namespace baconpaul::fm
{

extern const clap_plugin *makePlugin();

const clap_plugin_descriptor *getDescriptor()
{
    static const char *features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT,
                                     CLAP_PLUGIN_FEATURE_SYNTHESIZER, "Free and Open Source",
                                     nullptr};
    static clap_plugin_descriptor desc = {CLAP_VERSION,
                                          "org.baconpaul.ifm",
                                          "IFM",
                                          "BaconPaul",
                                          "https://baconpaul.org",
                                          "",
                                          "",
                                          "0.0.1",
                                          "FM Madness, including Integers, or something",
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
        FMLOG("Asked for desc");
        return makePlugin();
    }
    return nullptr;
}

const struct clap_plugin_factory ifm_clap_factory = {
    clap_get_plugin_count,
    clap_get_plugin_descriptor,
    clap_create_plugin,
};

const void *get_factory(const char *factory_id)
{
    FMLOG("Asking for factory [" << factory_id << "]");
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0)
    {
        return &ifm_clap_factory;
    }
    return nullptr;
}
bool clap_init(const char *p) { return true; }
void clap_deinit() {}
} // namespace baconpaul::fm