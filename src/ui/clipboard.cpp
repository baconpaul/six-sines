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

#include "clipboard.h"
#include "configuration.h"

namespace baconpaul::six_sines::ui
{
Clipboard::Clipboard() { SXSNLOG("Clipboard created"); }
} // namespace baconpaul::six_sines::ui