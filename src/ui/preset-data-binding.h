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

#ifndef BACONPAUL_SIX_SINES_UI_PRESET_DATA_BINDING_H
#define BACONPAUL_SIX_SINES_UI_PRESET_DATA_BINDING_H

#include "sst/jucegui/data/Discrete.h"
#include "presets/preset-manager.h"
#include "synth/patch.h"
#include "synth/synth.h"

namespace baconpaul::six_sines::ui
{

struct PresetDataBinding : sst::jucegui::data::Discrete
{
    presets::PresetManager &pm;
    Patch &patch;
    Synth::mainToAudioQueue_T &mainToAudio;
    PresetDataBinding(presets::PresetManager &p, Patch &pat, Synth::mainToAudioQueue_T &m)
        : pm(p), patch(pat), mainToAudio(m)
    {
    }

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
            pm.loadInit(patch, mainToAudio);
            return;
        }
        auto fp = f - 1;
        if (fp < pm.factoryPatchVector.size())
        {
            pm.loadFactoryPreset(patch, mainToAudio, pm.factoryPatchVector[fp].first,
                                 pm.factoryPatchVector[fp].second);
        }
        fp -= pm.factoryPatchVector.size();
        if (fp < pm.userPatches.size())
        {
            auto pt = pm.userPatches[fp];
            pm.loadUserPresetDirect(patch, mainToAudio, pm.userPatchesPath / pt);
        }
    };
    void setValueFromModel(const int &f) override { curr = f; }
    int getMin() const override { return hasExtra ? -1 : 0; }
    int getMax() const override
    {
        return 1 + pm.factoryPatchVector.size() + pm.userPatches.size() - 1 + (hasExtra ? 1 : 0);
    } // last -1 is because inclusive

    void setDirtyState(bool b) { isDirty = b; }

    void setStateForDisplayName(const std::string &s)
    {
        auto q = getValueAsString();
        auto sp = q.find("/");
        if (sp != std::string::npos)
        {
            q = q.substr(sp + 1);
        }

        if (s == "Init")
        {
            setValueFromModel(0);
        }
        else
        {
            bool found{false};
            int idx{1};
            for (const auto &[c, pp] : pm.factoryPatchVector)
            {
                auto p = pp.substr(0, pp.find(".sxsnp"));
                if (p == s)
                {
                    setValueFromModel(idx);
                    found = true;
                    break;
                }
                idx++;
            }
            if (!found)
            {
                for (auto &p : pm.userPatches)
                {
                    auto pn = p.filename().replace_extension("").u8string();
                    if (s == pn)
                    {
                        setValueFromModel(idx);
                        found = true;
                        break;
                    }
                    idx++;
                }
            }
            if (!found)
            {
                setExtra(s);
                setValueFromModel(-1);
            }
        }
    }
};
} // namespace baconpaul::six_sines::ui

#endif
