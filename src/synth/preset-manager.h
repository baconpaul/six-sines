/*
 * Six Sines A Sinnin'
 *
 * A mess, with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#ifndef BACONPAUL_SIX_SINES_SYNTH_PRESET_MANAGER_H
#define BACONPAUL_SIX_SINES_SYNTH_PRESET_MANAGER_H

#include "filesystem/import.h"
namespace baconpaul::six_sines
{
struct Synth;
struct PresetManager
{
    fs::path userPatchPath;
    PresetManager(const fs::path &p)
    {
        userPatchPath = p;
        rescanUserPresets();
    }

    struct Preset
    {
        bool isFactory{false};
        fs::path category;
        fs::path name;
    };

    std::vector<Preset> factoryPresets;
    std::vector<Preset> userPresets;

    void rescanUserPresets();
    void loadPreset(const Preset &, Synth &);
    void saveUserPreset(const fs::path &category, const fs::path &name, Synth &);
};
} // namespace baconpaul::six_sines
#endif // PRESET_MANAGER_H
