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

#ifndef BACONPAUL_SIX_SINES_UI_UI_DEFAULTS_H
#define BACONPAUL_SIX_SINES_UI_UI_DEFAULTS_H

#include "configuration.h"
#include <sst/plugininfra/userdefaults.h>

namespace baconpaul::six_sines::ui
{
enum Defaults
{
    useLightSkin,
    zoomLevel,
    useSoftwareRenderer, // only used on windows
    flipSourceAndMatrix,
    numDefaults
};

inline std::string defaultName(Defaults d)
{
    switch (d)
    {
    case useLightSkin:
        return "useLightSkin";
    case zoomLevel:
        return "zoomLevel";
    case useSoftwareRenderer:
        return "useSoftwareRenderer";
    case flipSourceAndMatrix:
        return "flipSourceAndMatrix";
    case numDefaults:
    {
        SXSNLOG("Software Error - defaults found");
        return "";
    }
    }
    return "";
}

using defaultsProvder_t = sst::plugininfra::defaults::Provider<Defaults, Defaults::numDefaults>;
} // namespace baconpaul::six_sines::ui

#endif // UI_DEFAULTS_H
