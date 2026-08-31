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
#include <cstdint>
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
    static constexpr const char *presetExtension{".sxsnp"};

    // Identifies which preset the engine holds. A display name is not a unique key for one: a
    // user preset can share its bare filename with a factory patch, or with another user patch
    // in a different folder. Anything that has to survive a jog keys on this instead.
    struct LoadedPreset
    {
        enum struct Kind
        {
            Unknown, // arbitrary state - a host stateLoad, or a name we cannot place
            Init,
            Factory,
            User
        } kind{Kind::Unknown};

        std::string category{}; // factory category; empty otherwise
        fs::path path{};        // factory: the file name. user: relative to userPatchesPath
        std::string displayName{};

        bool matchesFactory(const std::string &cat, const std::string &file) const
        {
            return kind == Kind::Factory && category == cat && path.u8string() == file;
        }
        bool matchesUser(const fs::path &relative) const
        {
            return kind == Kind::User && path == relative;
        }

        static LoadedPreset init();
        static LoadedPreset factory(const std::string &cat, const std::string &file);
        static LoadedPreset user(const fs::path &relative);
        static LoadedPreset unknown(const std::string &name);

        // Round trip through the <preset> element of the DAW session state. An Unknown preset
        // has no slot to record, so it encodes as an empty kind.
        std::string sessionKind() const;
        std::string sessionPath() const { return path.u8string(); }
        static LoadedPreset fromSession(const std::string &kind, const std::string &cat,
                                        const std::string &path);
    };

    fs::path userPath;
    fs::path userPatchesPath;
    const clap_host_t *clapHost{nullptr};

    // Call with a null host to be read-only
    PresetManager(const clap_host_t *host);
    ~PresetManager();

    void rescanUserPresets();

    void loadInit(Patch &p, Synth::mainToAudioQueue_T &);
    void loadUserPresetDirect(Patch &, Synth::mainToAudioQueue_T &, const fs::path &p);
    void loadFactoryPreset(Patch &, Synth::mainToAudioQueue_T &, const std::string &cat,
                           const std::string &pat);

#if USE_WCHAR_PRESET
    void saveUserPresetDirect(Patch &, const wchar_t *utf8path);
#else
    void saveUserPresetDirect(Patch &, const fs::path &p);
#endif

    // A user preset is identified by its path below userPatchesPath; anything outside that tree
    // (the load/save file chooser can go anywhere) has no slot in the jog list.
    LoadedPreset identifyUserPath(const fs::path &absolute) const;

    static std::string stripPresetExtension(const std::string &fileName);

    std::function<void(const LoadedPreset &)> onPresetLoaded{nullptr};

    static constexpr const char *factoryPath{"resources/factory_patches"};
    std::map<std::string, std::vector<std::string>> factoryPatchNames;
    std::vector<std::pair<std::string, std::string>> factoryPatchVector;
    std::vector<fs::path> userPatches;

    // bumped by rescanUserPresets so a cached index into userPatches knows to re-resolve
    uint32_t userPatchesEpoch{0};
};
} // namespace baconpaul::six_sines::presets
#endif // PRESET_MANAGER_H
