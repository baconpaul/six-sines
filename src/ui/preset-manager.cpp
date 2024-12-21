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

#include <fstream>
#include "sst/plugininfra/paths.h"
#include "preset-manager.h"

namespace baconpaul::six_sines::ui
{
PresetManager::PresetManager()
{
    try
    {
        auto userPath = sst::plugininfra::paths::bestDocumentsFolderPathFor("SixSines");
        fs::create_directories(userPath);
        userPatchesPath = userPath / "Patches";
        fs::create_directories(userPatchesPath);
    }
    catch (fs::filesystem_error &e)
    {
        SXSNLOG("Unable to create user dir " << e.what());
    }
}

void PresetManager::rescanUserPresets() {}
void PresetManager::saveUserPreset(const fs::path &category, const fs::path &name, Patch &patch)
{
    try
    {
        auto dir = userPatchPath / category;
        fs::create_directories(dir);
        auto pt = (dir / name).replace_extension(".sxsnp");
        std::ofstream ofs(pt);
        if (ofs.is_open())
        {
            ofs << patch.toState();
        }
    }
    catch (fs::filesystem_error &e)
    {
        SXSNLOG(e.what());
    }
}
void PresetManager::loadPreset(const Preset &p, Patch &synth)
{

    if (p.isFactory)
    {
    }
    else
    {
    }
}
} // namespace baconpaul::six_sines::ui