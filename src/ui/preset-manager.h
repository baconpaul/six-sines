/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
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
namespace baconpaul::six_sines::ui
{
struct PresetManager
{
    fs::path userPatchPath;
    PresetManager();

    struct Preset
    {
        bool isFactory{false};
        fs::path category;
        fs::path name;
    };

    std::vector<Preset> factoryPresets;
    std::vector<Preset> userPresets;

    void rescanUserPresets();
    void loadPreset(const Preset &, Patch &);
    void saveUserPreset(const fs::path &category, const fs::path &name, Patch &);

    void saveUserPresetDirect(const fs::path &p, Patch &);
    void loadUserPresetDirect(const fs::path &p, Patch &);

    fs::path userPath;
    fs::path userPatchesPath;
};
} // namespace baconpaul::six_sines::ui
#endif // PRESET_MANAGER_H
