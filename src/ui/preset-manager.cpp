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

#include <sstream>
#include <fstream>
#include "sst/plugininfra/paths.h"
#include "preset-manager.h"

#include "sst/plugininfra/strnatcmp.h"

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(sixsines_patches);

namespace baconpaul::six_sines::ui
{

struct PresetDataBinding : sst::jucegui::data::Discrete
{
    PresetManager &pm;
    PresetDataBinding(PresetManager &p) : pm(p) {}

    std::string getLabel() const override { return "Presets"; }

    int curr{0};
    bool hasExtra{false};
    std::string extraName{};
    void setExtra(const std::string &s)
    {
        hasExtra = true;
        extraName = s;
    }

    int getValue() const override { return curr; }
    int getDefaultValue() const override { return 0; };
    bool isDirty{false};

    std::string getValueAsStringFor(int i) const override
    {
        if (hasExtra && i < 0)
            return extraName;

        std::string postfix = isDirty ? " *" : "";

        if (i == 0)
            return "Init" + postfix;
        auto fp = i - 1;
        if (fp < pm.factoryPatchVector.size())
        {
            fs::path p{pm.factoryPatchVector[fp].first};
            p = p / pm.factoryPatchVector[fp].second;
            p = p.replace_extension("");
            return p.u8string() + postfix;
        }
        fp -= pm.factoryPatchVector.size();
        if (fp < pm.userPatches.size())
        {
            auto pt = pm.userPatches[fp];
            pt = pt.replace_extension("");
            return pt.u8string() + postfix;
        }
        return "ERR";
    }
    void setValueFromGUI(const int &f) override
    {
        isDirty = false;
        if (hasExtra)
        {
            hasExtra = false;
        }
        curr = f;
        if (f == 0)
        {
            pm.loadInit();
            return;
        }
        auto fp = f - 1;
        if (fp < pm.factoryPatchVector.size())
        {
            pm.loadFactoryPreset(pm.factoryPatchVector[fp].first, pm.factoryPatchVector[fp].second);
        }
        fp -= pm.factoryPatchVector.size();
        if (fp < pm.userPatches.size())
        {
            auto pt = pm.userPatches[fp];
            pm.loadUserPresetDirect(pm.userPatchesPath / pt);
        }
    };
    void setValueFromModel(const int &f) override { curr = f; }
    int getMin() const override { return hasExtra ? -1 : 0; }
    int getMax() const override
    {
        return 1 + pm.factoryPatchVector.size() + pm.userPatches.size() - 1 + (hasExtra ? 1 : 0);
    } // last -1 is because inclusive
};

PresetManager::PresetManager(Patch &pp) : patch(pp)
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

                std::sort(ents.begin(), ents.end(),
                          [](const auto &a, const auto &b)
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

    discreteDataBinding = std::make_unique<PresetDataBinding>(*this);

    setStateForDisplayName(patch.name);
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
}

void PresetManager::saveUserPresetDirect(const fs::path &pt)
{
    std::ofstream ofs(pt);
    if (ofs.is_open())
    {
        ofs << patch.toState();
    }
    ofs.close();
    rescanUserPresets();
}

void PresetManager::loadUserPresetDirect(const fs::path &p)
{
    std::ifstream t(p);
    if (!t.is_open())
        return;
    std::stringstream buffer;
    buffer << t.rdbuf();

    patch.fromState(buffer.str());

    auto dn = p.filename().replace_extension("").u8string();
    if (onPresetLoaded)
        onPresetLoaded(dn);
}

void PresetManager::loadFactoryPreset(const std::string &cat, const std::string &pat)
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
        if (idx != factoryPatchVector.size())
        {
            discreteDataBinding->setValueFromModel(idx + 1);
        }

        if (onPresetLoaded)
        {
            auto noExt = pat;
            auto ps = noExt.find(".sxsnp");
            if (ps != std::string::npos)
            {
                noExt = noExt.substr(0, ps);
            }
            onPresetLoaded(noExt);
        }
    }
    catch (const std::exception &e)
    {
        SXSNLOG(e.what());
    }
}

void PresetManager::loadInit()
{
    patch.resetToInit();
    if (onPresetLoaded)
        onPresetLoaded("Init");
}

sst::jucegui::data::Discrete *PresetManager::getDiscreteData() { return discreteDataBinding.get(); }

void PresetManager::setStateForDisplayName(const std::string &s)
{
    auto q = discreteDataBinding->getValueAsString();
    auto sp = q.find("/");
    if (sp != std::string::npos)
    {
        q = q.substr(sp + 1);
    }
    if (q != s)
    {
        if (s == "Init")
        {
            discreteDataBinding->setValueFromModel(0);
        }
        else
        {
            bool found{false};
            int idx{1};
            for (const auto &[c, pp] : factoryPatchVector)
            {
                auto p = pp.substr(0, pp.find(".sxsnp"));
                if (p == s)
                {
                    discreteDataBinding->setValueFromModel(idx);
                    found = true;
                    break;
                }
                idx++;
            }
            if (!found)
            {
                for (auto &p : userPatches)
                {
                    auto pn = p.filename().replace_extension("").u8string();
                    if (s == pn)
                    {
                        discreteDataBinding->setValueFromModel(idx);
                        found = true;
                        break;
                    }
                    idx++;
                }
            }
            if (!found)
            {
                discreteDataBinding->setExtra(s);
                discreteDataBinding->setValueFromModel(-1);
            }
        }
    }
}

void PresetManager::setDirtyState(bool b) { discreteDataBinding->isDirty = b; }

} // namespace baconpaul::six_sines::ui