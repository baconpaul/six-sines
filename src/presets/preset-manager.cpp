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

#include "preset-manager.h"
#include "synth/synth.h"
#include <sstream>
#include <fstream>
#include <cstring>

#include "sst/plugininfra/strnatcmp.h"

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(sixsines_patches);

namespace baconpaul::six_sines::presets
{

namespace
{
// Name + dirty are main-thread-only patch state: the audio patch never reads them, and the editor
// reads them straight off patchMain. Set them on the patch (== patchMain) here rather than
// round-tripping through the queues. Author + macro names were already written by fromState /
// resetToInit; the funnel moves only param values into the audio patch.
void nameAndMarkClean(Patch &patch, const std::string &name)
{
    memset(patch.name, 0, sizeof(patch.name));
    strncpy(patch.name, name.c_str(), sizeof(patch.name) - 1);
    patch.dirty = false;
}
} // namespace

std::string PresetManager::stripPresetExtension(const std::string &fileName)
{
    std::string ext{presetExtension};
    if (fileName.size() >= ext.size() &&
        fileName.compare(fileName.size() - ext.size(), ext.size(), ext) == 0)
        return fileName.substr(0, fileName.size() - ext.size());
    return fileName;
}

PresetManager::LoadedPreset PresetManager::LoadedPreset::init()
{
    return {Kind::Init, {}, {}, "Init"};
}

PresetManager::LoadedPreset PresetManager::LoadedPreset::factory(const std::string &cat,
                                                                 const std::string &file)
{
    return {Kind::Factory, cat, fs::path{file}, stripPresetExtension(file)};
}

PresetManager::LoadedPreset PresetManager::LoadedPreset::user(const fs::path &relative)
{
    auto dn = relative.filename();
    return {Kind::User, {}, relative, dn.replace_extension("").u8string()};
}

PresetManager::LoadedPreset PresetManager::LoadedPreset::unknown(const std::string &name)
{
    return {Kind::Unknown, {}, {}, name};
}

std::string PresetManager::LoadedPreset::sessionKind() const
{
    switch (kind)
    {
    case Kind::Init:
        return "init";
    case Kind::Factory:
        return "factory";
    case Kind::User:
        return "user";
    case Kind::Unknown:
        break;
    }
    return "";
}

PresetManager::LoadedPreset PresetManager::LoadedPreset::fromSession(const std::string &kind,
                                                                     const std::string &cat,
                                                                     const std::string &path)
{
    if (kind == "init")
        return init();
    if (kind == "factory")
        return factory(cat, path);
    if (kind == "user")
        return user(fs::u8path(path));
    return unknown("");
}

PresetManager::LoadedPreset PresetManager::identifyUserPath(const fs::path &absolute) const
{
    auto dn = absolute.filename();
    dn = dn.replace_extension("");

    if (userPatchesPath.empty())
        return LoadedPreset::unknown(dn.u8string());

    // purely lexical, to match how rescanUserPresets stores the entries
    auto rel = absolute.lexically_relative(userPatchesPath);
    if (rel.empty() || *rel.begin() == "..")
        return LoadedPreset::unknown(dn.u8string());

    return LoadedPreset::user(rel);
}

PresetManager::PresetManager(const clap_host_t *ch) : clapHost(ch)
{
    try
    {
        userPath = Synth::userDocumentsPath();
        if (clapHost)
            fs::create_directories(userPath);
        userPatchesPath = userPath / "Patches";
        if (clapHost)
            fs::create_directories(userPatchesPath);
    }
    catch (fs::filesystem_error &e)
    {
        SXSNLOG("Unable to create user dir " << e.what());
    }

    try
    {
        auto fs = cmrc::sixsines_patches::get_filesystem();
        for (const auto &d : fs.iterate_directory(factoryPath))
        {
            if (d.is_directory())
            {
                std::vector<std::string> ents;
                for (const auto &p :
                     fs.iterate_directory(std::string() + factoryPath + "/" + d.filename()))
                {
                    ents.push_back(p.filename());
                }

                std::sort(ents.begin(), ents.end(), [](const auto &a, const auto &b)
                          { return strnatcasecmp(a.c_str(), b.c_str()) < 0; });
                factoryPatchNames[d.filename()] = ents;
            }
        }

        factoryPatchVector.clear();
        for (const auto &[c, st] : factoryPatchNames)
        {
            for (const auto &pn : st)
            {
                factoryPatchVector.emplace_back(c, pn);
            }
        }
    }
    catch (const std::exception &e)
    {
        SXSNLOG(e.what());
    }

    rescanUserPresets();
}

PresetManager::~PresetManager() = default;

void PresetManager::rescanUserPresets()
{
    userPatches.clear();
    try
    {
        std::function<void(const fs::path &)> itd;
        itd = [this, &itd](auto &p)
        {
            if (fs::is_directory(p))
            {
                for (auto &el : fs::directory_iterator(p))
                {
                    auto elp = el.path();
                    if (elp.filename() == "." || elp.filename() == "..")
                    {
                        continue;
                    }
                    if (fs::is_directory(elp))
                    {
                        itd(elp);
                    }
                    else if (fs::is_regular_file(elp) && elp.extension() == ".sxsnp")
                    {
                        auto pushP = elp.lexically_relative(userPatchesPath);
                        userPatches.push_back(pushP);
                    }
                }
            }
        };
        itd(userPatchesPath);
        std::sort(userPatches.begin(), userPatches.end(),
                  [](const fs::path &a, const fs::path &b)
                  {
                      auto appe = a.parent_path().empty();
                      auto bppe = b.parent_path().empty();

                      if (appe && bppe)
                      {
                          return strnatcasecmp(a.filename().u8string().c_str(),
                                               b.filename().u8string().c_str()) < 0;
                      }
                      else if (appe)
                      {
                          return true;
                      }
                      else if (bppe)
                      {
                          return false;
                      }
                      else
                      {
                          return a < b;
                      }
                  });
    }
    catch (fs::filesystem_error &)
    {
    }

    userPatchesEpoch++;
}

#if USE_WCHAR_PRESET
void PresetManager::saveUserPresetDirect(Patch &patch, const wchar_t *fname)
{
    std::ofstream ofs(fname);

    if (ofs.is_open())
    {
        ofs << patch.toState();
    }
    ofs.close();
    rescanUserPresets();
}
#else
void PresetManager::saveUserPresetDirect(Patch &patch, const fs::path &pt)
{
    std::ofstream ofs(pt);

    if (ofs.is_open())
    {
        ofs << patch.toState();
    }
    ofs.close();
    rescanUserPresets();
}
#endif

void PresetManager::loadUserPresetDirect(Patch &patch, Synth::mainToAudioQueue_T &mainToAudio,
                                         const fs::path &p)
{
    std::ifstream t(p);
    if (!t.is_open())
        return;
    std::stringstream buffer;
    buffer << t.rdbuf();

    patch.fromState(buffer.str());

    auto loaded = identifyUserPath(p);
    nameAndMarkClean(patch, loaded.displayName);
    Synth::sendEntirePatchToAudio(patch, mainToAudio, clapHost);
    if (onPresetLoaded)
        onPresetLoaded(loaded);
}

void PresetManager::loadFactoryPreset(Patch &patch, Synth::mainToAudioQueue_T &mainToAudio,
                                      const std::string &cat, const std::string &pat)
{
    try
    {
        auto fs = cmrc::sixsines_patches::get_filesystem();
        auto f = fs.open(std::string() + factoryPath + "/" + cat + "/" + pat);
        auto pb = std::string(f.begin(), f.end());
        patch.fromState(pb);

        // can we find this factory preset
        int idx{0};
        for (idx = 0; idx < factoryPatchVector.size(); idx++)
        {
            if (factoryPatchVector[idx].first == cat && factoryPatchVector[idx].second == pat)
                break;
        }

        if (idx == factoryPatchVector.size())
        {
            return;
        }

        auto loaded = LoadedPreset::factory(cat, pat);
        nameAndMarkClean(patch, loaded.displayName);
        Synth::sendEntirePatchToAudio(patch, mainToAudio, clapHost);

        if (onPresetLoaded)
        {
            onPresetLoaded(loaded);
        }
    }
    catch (const std::exception &e)
    {
        SXSNLOG(e.what());
    }
}

void PresetManager::loadInit(Patch &patch, Synth::mainToAudioQueue_T &mainToAudio)
{
    patch.resetToInit();
    auto loaded = LoadedPreset::init();
    nameAndMarkClean(patch, loaded.displayName);
    Synth::sendEntirePatchToAudio(patch, mainToAudio, clapHost);
    if (onPresetLoaded)
        onPresetLoaded(loaded);
}

} // namespace baconpaul::six_sines::presets