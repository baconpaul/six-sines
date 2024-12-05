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

namespace baconpaul::fm
{
const void *get_factory(const char *factory_id)
{
    FMLOG("Asking for factory [" << factory_id << "]");
    return nullptr;
}
bool clap_init(const char *p) { return true; }
void clap_deinit() {}
} // namespace baconpaul::fm