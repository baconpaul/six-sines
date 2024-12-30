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

#ifndef BACONPAUL_SIX_SINES_UI_PRESET_MANAGER_H
#define BACONPAUL_SIX_SINES_UI_PRESET_MANAGER_H

#include "filesystem/import.h"
#include "synth/patch.h"
#include <map>
#include <set>
#include <string>

namespace baconpaul::six_sines::ui
{
struct PresetManager
{
    fs::path userPatchPath;
    PresetManager();

    void rescanUserPresets();

    void saveUserPresetDirect(const fs::path &p, Patch &);
    void loadUserPresetDirect(const fs::path &p, Patch &);

    void loadFactoryPreset(const std::string &cat, const std::string &pat, Patch &);
    fs::path userPath;
    fs::path userPatchesPath;

    static constexpr const char *factoryPath{"resources/factory_patches"};
    std::map<std::string, std::set<std::string>> factoryPatchNames;
    std::vector<fs::path> userPatches;
};
} // namespace baconpaul::six_sines::ui
#endif // PRESET_MANAGER_H
