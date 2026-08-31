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
    using LoadedPreset = presets::PresetManager::LoadedPreset;

    presets::PresetManager &pm;
    Patch &patch;
    Synth::mainToAudioQueue_T &mainToAudio;
    // The session records which preset is showing; the binding is the only writer.
    Synth::MainDawState &sessionState;
    PresetDataBinding(presets::PresetManager &p, Patch &pat, Synth::mainToAudioQueue_T &m,
                      Synth::MainDawState &des)
        : pm(p), patch(pat), mainToAudio(m), sessionState(des)
    {
        // Seed without publishing: sessionState already holds whatever stateLoad put there, and
        // restoreSelectionFromSession is about to read it.
        selected = LoadedPreset::init();
        selectedIndex = 0;
        selectedEpoch = pm.userPatchesEpoch;
    }

    std::string getLabel() const override { return "Presets"; }

    // What is loaded, keyed on the file rather than on the display name. The index into the jog
    // list is derived from it, so a rescan (a save, a refresh) re-resolves to the same preset
    // instead of leaving a stale slot behind.
    LoadedPreset selected{};
    mutable int selectedIndex{0};
    mutable uint32_t selectedEpoch{0};

    bool isDirty{false};

    // no slot in the list: the extra entry at -1 shows the name and nothing else
    bool hasExtra() const { return selected.kind == LoadedPreset::Kind::Unknown; }

    int getValue() const override
    {
        if (selectedEpoch != pm.userPatchesEpoch)
        {
            selectedIndex = indexOf(selected);
            selectedEpoch = pm.userPatchesEpoch;
        }
        return selectedIndex;
    }
    int getDefaultValue() const override { return 0; };

    std::string getValueAsStringFor(int i) const override
    {
        if (i < 0)
            return hasExtra() ? selected.displayName : "ERR";

        auto lp = presetAt(i);
        if (lp.kind == LoadedPreset::Kind::Unknown)
            return "ERR";

        std::string postfix = isDirty ? " *" : "";
        if (lp.kind == LoadedPreset::Kind::Factory)
        {
            auto p = fs::path{lp.category} / lp.path;
            return p.replace_extension("").u8string() + postfix;
        }
        if (lp.kind == LoadedPreset::Kind::User)
        {
            auto p = lp.path;
            return p.replace_extension("").u8string() + postfix;
        }
        return lp.displayName + postfix;
    }

    void setValueFromGUI(const int &f) override
    {
        auto lp = presetAt(f);
        if (lp.kind == LoadedPreset::Kind::Unknown)
            return;

        isDirty = false;
        setSelection(lp);

        switch (lp.kind)
        {
        case LoadedPreset::Kind::Init:
            pm.loadInit(patch, mainToAudio);
            break;
        case LoadedPreset::Kind::Factory:
            pm.loadFactoryPreset(patch, mainToAudio, lp.category, lp.path.u8string());
            break;
        case LoadedPreset::Kind::User:
            pm.loadUserPresetDirect(patch, mainToAudio, pm.userPatchesPath / lp.path);
            break;
        case LoadedPreset::Kind::Unknown:
            break;
        }
    };
    void setValueFromModel(const int &f) override { setSelection(presetAt(f)); }
    int getMin() const override { return hasExtra() ? -1 : 0; }
    int getMax() const override
    {
        // inclusive: Init plus both lists
        return (int)(pm.factoryPatchVector.size() + pm.userPatches.size());
    }

    void setDirtyState(bool b) { isDirty = b; }

    // The preset manager loaded something and told us exactly what it was.
    void setStateForLoadedPreset(const LoadedPreset &lp) { setSelection(lp); }

    // A save wrote this file and the manager has rescanned, so it now has a slot.
    void setStateForSavedUserPath(const fs::path &absolute)
    {
        setSelection(pm.identifyUserPath(absolute));
    }

    // A session (or a host stateLoad) brought back a patch. Prefer the preset the session
    // recorded, but only when it still resolves and still carries the name the patch has -
    // otherwise something loaded a different preset behind our back and the record is stale.
    void restoreSelectionFromSession(const std::string &name)
    {
        auto lp = LoadedPreset::fromSession(sessionState.presetKind, sessionState.presetCategory,
                                            sessionState.presetPath);
        if (lp.kind != LoadedPreset::Kind::Unknown && lp.displayName == name && indexOf(lp) >= 0)
        {
            setSelection(lp);
            return;
        }
        setStateForDisplayName(name);
    }

    // Last resort, for a patch that arrived without an identity (a host stateLoad): all we have
    // is the name. It is ambiguous, so hold the current selection whenever it still carries that
    // name rather than relocating onto whichever list happens to hold it first.
    void setStateForDisplayName(const std::string &s)
    {
        if (selected.displayName == s && getValue() >= 0)
            return;
        setSelection(identify(s));
    }

  private:
    void setSelection(const LoadedPreset &lp)
    {
        selected = lp;
        selectedIndex = indexOf(lp);
        selectedEpoch = pm.userPatchesEpoch;

        sessionState.recordPreset(lp.sessionKind(), lp.category, lp.sessionPath());
    }

    int indexOf(const LoadedPreset &lp) const
    {
        switch (lp.kind)
        {
        case LoadedPreset::Kind::Init:
            return 0;
        case LoadedPreset::Kind::Factory:
            for (size_t i = 0; i < pm.factoryPatchVector.size(); ++i)
                if (lp.matchesFactory(pm.factoryPatchVector[i].first,
                                      pm.factoryPatchVector[i].second))
                    return 1 + (int)i;
            break;
        case LoadedPreset::Kind::User:
            for (size_t i = 0; i < pm.userPatches.size(); ++i)
                if (lp.matchesUser(pm.userPatches[i]))
                    return 1 + (int)pm.factoryPatchVector.size() + (int)i;
            break;
        case LoadedPreset::Kind::Unknown:
            break;
        }
        return -1;
    }

    LoadedPreset presetAt(int i) const
    {
        if (i == 0)
            return LoadedPreset::init();

        auto fp = i - 1;
        if (fp >= 0 && fp < (int)pm.factoryPatchVector.size())
            return LoadedPreset::factory(pm.factoryPatchVector[fp].first,
                                         pm.factoryPatchVector[fp].second);

        auto up = fp - (int)pm.factoryPatchVector.size();
        if (up >= 0 && up < (int)pm.userPatches.size())
            return LoadedPreset::user(pm.userPatches[up]);

        return LoadedPreset::unknown("");
    }

    LoadedPreset identify(const std::string &s) const
    {
        if (s == "Init")
            return LoadedPreset::init();

        for (const auto &[c, f] : pm.factoryPatchVector)
            if (presets::PresetManager::stripPresetExtension(f) == s)
                return LoadedPreset::factory(c, f);

        for (const auto &p : pm.userPatches)
        {
            auto pn = p.filename();
            if (pn.replace_extension("").u8string() == s)
                return LoadedPreset::user(p);
        }

        return LoadedPreset::unknown(s);
    }
};
} // namespace baconpaul::six_sines::ui

#endif
