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

#ifndef BACONPAUL_SIX_SINES_CLAP_PRESET_DISCOVERY_IMPL_H
#define BACONPAUL_SIX_SINES_CLAP_PRESET_DISCOVERY_IMPL_H

#include <clap/clap.h>
namespace baconpaul::six_sines
{
const clap_preset_discovery_factory_t *sixSinesPresetDiscoveryFactory();
}
#endif // PRESET_DISCOVERY_IMPL_H
