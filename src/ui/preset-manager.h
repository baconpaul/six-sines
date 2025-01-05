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
#include "sst/jucegui/data/Discrete.h"
#include "synth/patch.h"
#include <map>
#include <functional>
#include <set>
#include <string>

namespace baconpaul::six_sines::ui
{
struct PresetDataBinding;
struct PresetManager
{
    fs::path userPath;
    fs::path userPatchesPath;
    Patch &patch;
    PresetManager(Patch &p);
    ~PresetManager();

    void rescanUserPresets();

    void loadInit();

    void saveUserPresetDirect(const fs::path &p);
    void loadUserPresetDirect(const fs::path &p);

    void loadFactoryPreset(const std::string &cat, const std::string &pat);

    std::function<void(const std::string &)> onPresetLoaded{nullptr};

    void setStateForDisplayName(const std::string &s);

    static constexpr const char *factoryPath{"resources/factory_patches"};
    std::map<std::string, std::vector<std::string>> factoryPatchNames;
    std::vector<std::pair<std::string, std::string>> factoryPatchVector;
    std::vector<fs::path> userPatches;

    std::unique_ptr<PresetDataBinding> discreteDataBinding;
    sst::jucegui::data::Discrete *getDiscreteData();

    void setDirtyState(bool b);
};
} // namespace baconpaul::six_sines::ui
#endif // PRESET_MANAGER_H
