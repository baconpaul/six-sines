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

#ifndef BACONPAUL_FMTHING_CLAP_IFM_CLAP_ENTRY_IMPL_H
#define BACONPAUL_FMTHING_CLAP_IFM_CLAP_ENTRY_IMPL_H

namespace baconpaul::fm
{
const void *get_factory(const char *factory_id);
bool clap_init(const char *p);
void clap_deinit();
} // namespace baconpaul::fm

#endif // IFM_CLAP_ENTRY_IMPL_H
