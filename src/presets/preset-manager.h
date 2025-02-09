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

#ifndef BACONPAUL_SIX_SINES_PRESETS_PRESET_MANAGER_H
#define BACONPAUL_SIX_SINES_PRESETS_PRESET_MANAGER_H

#include <clap/clap.h>
#include "filesystem/import.h"
#include "sst/jucegui/data/Discrete.h"
#include "synth/patch.h"
#include "synth/synth.h"
#include <map>
#include <unordered_map>
#include <functional>
#include <set>
#include <string>

namespace baconpaul::six_sines::presets
{
struct PresetDataBinding;
struct PresetManager
{
    fs::path userPath;
    fs::path userPatchesPath;
    const clap_host_t *clapHost{nullptr};
    PresetManager(const clap_host_t *host);
    ~PresetManager();

    void rescanUserPresets();

    void loadInit(Patch &p, Synth::mainToAudioQueue_T &);
    void loadUserPresetDirect(Patch &, Synth::mainToAudioQueue_T &, const fs::path &p);
    void loadFactoryPreset(Patch &, Synth::mainToAudioQueue_T &, const std::string &cat,
                           const std::string &pat);

    void saveUserPresetDirect(Patch &, const fs::path &p);

    std::function<void(const std::string &)> onPresetLoaded{nullptr};

    static constexpr const char *factoryPath{"resources/factory_patches"};
    std::map<std::string, std::vector<std::string>> factoryPatchNames;
    std::vector<std::pair<std::string, std::string>> factoryPatchVector;
    std::vector<fs::path> userPatches;

    const clap_host_params_t *clapHostParams{nullptr};
    void sendEntirePatchToAudio(Patch &, Synth::mainToAudioQueue_T &, const std::string &name);
    static void sendEntirePatchToAudio(Patch &, Synth::mainToAudioQueue_T &,
                                       const std::string &name, const clap_host_t *,
                                       const clap_host_params_t *p = nullptr);
};
} // namespace baconpaul::six_sines::presets
#endif // PRESET_MANAGER_H
