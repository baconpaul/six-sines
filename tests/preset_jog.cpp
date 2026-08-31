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

// Preset selection / jog ordering. The selected preset is an index into
// {Init} + factoryPatchVector + userPatches, and a display name is NOT a unique key for it:
// a user preset can share its bare filename with a factory one, or with another user preset in
// a different folder. These pin the selection to the actual file rather than to the name.

#include "catch2/catch2.hpp"

#include <fstream>
#include <memory>
#include <string>

#include <cmrc/cmrc.hpp>

#include "clap/clap.h"
#include "presets/preset-manager.h"
#include "synth/patch.h"
#include "synth/synth.h"
#include "ui/preset-data-binding.h"

#include "tinyxml/tinyxml.h"

CMRC_DECLARE(sixsines_patches);

using namespace baconpaul::six_sines;

namespace baconpaul::six_sines
{
extern const clap_plugin *makePlugin(const clap_host *h, bool multiOut);
}

namespace
{
// Patch construction touches DSP tables populated as a side effect of plugin init
void ensureDspTables()
{
    static bool done{false};
    if (done)
        return;
    clap_host_t host{};
    host.clap_version = CLAP_VERSION;
    host.name = "Six Sines Preset Jog Test";
    host.vendor = "Test";
    host.url = "";
    host.version = "0";
    host.get_extension = [](const clap_host_t *, const char *) -> const void * { return nullptr; };
    host.request_restart = [](const clap_host_t *) {};
    host.request_process = [](const clap_host_t *) {};
    host.request_callback = [](const clap_host_t *) {};
    auto *plugin = makePlugin(&host, false);
    REQUIRE(plugin != nullptr);
    plugin->init(plugin);
    done = true;
}

// A PresetManager rooted at a scratch directory rather than the real user Documents tree.
// Null host == read-only: nothing is created under the user's own patch folder.
struct ScratchPresets
{
    fs::path root;
    presets::PresetManager pm;

    explicit ScratchPresets(const std::string &tag)
        : root(fs::temp_directory_path() / ("six-sines-jog-" + tag)), pm(nullptr)
    {
        fs::remove_all(root);
        fs::create_directories(root);
        pm.userPatchesPath = root;
        pm.rescanUserPresets();
    }
    ~ScratchPresets() { fs::remove_all(root); }

    // writes a copy of a factory patch into the scratch user tree, exactly as "load a factory
    // preset, tweak it, save it under your own name" would
    void addUserPatch(const std::string &rel, const std::string &factorySource)
    {
        auto p = root / fs::path(rel);
        fs::create_directories(p.parent_path());

        auto fsys = cmrc::sixsines_patches::get_filesystem();
        auto f = fsys.open(std::string(presets::PresetManager::factoryPath) + "/" + factorySource);
        std::ofstream o(p);
        o << std::string(f.begin(), f.end());
        o.close();

        pm.rescanUserPresets();
    }
};

struct Engine
{
    std::unique_ptr<Patch> patch{std::make_unique<Patch>()};
    std::unique_ptr<Synth::mainToAudioQueue_T> queue{std::make_unique<Synth::mainToAudioQueue_T>()};
    Synth::MainDawState des{};
};

int factoryIndexOf(const presets::PresetManager &pm, const std::string &cat,
                   const std::string &file)
{
    for (size_t i = 0; i < pm.factoryPatchVector.size(); ++i)
        if (pm.factoryPatchVector[i].first == cat && pm.factoryPatchVector[i].second == file)
            return 1 + (int)i;
    return -1;
}

int userIndexOf(const presets::PresetManager &pm, const std::string &rel)
{
    for (size_t i = 0; i < pm.userPatches.size(); ++i)
        if (pm.userPatches[i].generic_string() == rel)
            return 1 + (int)pm.factoryPatchVector.size() + (int)i;
    return -1;
}

// Display labels are built with fs::path, so the separator is the platform's own.
std::string label(const std::string &folder, const std::string &name)
{
    return (fs::path(folder) / name).u8string();
}

// Mirrors the editor: PresetManager tells whoever is listening what it just loaded, and the
// binding moves its selection there. Without this wiring the round trip under test is absent.
void wireBinding(presets::PresetManager &pm, ui::PresetDataBinding &b)
{
    pm.onPresetLoaded = [&b](const auto &loaded) { b.setStateForLoadedPreset(loaded); };
}
} // namespace

TEST_CASE("A user preset sharing a factory name keeps its own slot", "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("factory-clash");
    sp.addUserPatch("MyFolder/EP 3.sxsnp", "Keys/EP 3.sxsnp");

    Engine e;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);

    auto userIdx = userIndexOf(sp.pm, "MyFolder/EP 3.sxsnp");
    REQUIRE(userIdx > 0);

    b.setValueFromGUI(userIdx);

    REQUIRE(b.getValueAsString() == label("MyFolder", "EP 3"));
    REQUIRE(b.getValue() == userIdx);
}

TEST_CASE("Jogging off a name-clashing user preset stays in the user list", "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("jog-past-clash");
    sp.addUserPatch("MyFolder/EP 3.sxsnp", "Keys/EP 3.sxsnp");
    sp.addUserPatch("MyFolder/Zulu.sxsnp", "Keys/Organ.sxsnp");

    Engine e;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);

    b.setValueFromGUI(userIndexOf(sp.pm, "MyFolder/EP 3.sxsnp"));
    b.jog(1);

    REQUIRE(b.getValueAsString() == label("MyFolder", "Zulu"));
}

TEST_CASE("Two user presets with the same bare name are distinct slots", "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("user-clash");
    sp.addUserPatch("Alpha/Dup.sxsnp", "Keys/Organ.sxsnp");
    sp.addUserPatch("Beta/Dup.sxsnp", "Keys/EP 3.sxsnp");

    Engine e;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);

    b.setValueFromGUI(userIndexOf(sp.pm, "Beta/Dup.sxsnp"));

    REQUIRE(b.getValueAsString() == label("Beta", "Dup"));
}

TEST_CASE("A user preset named Init does not select the Init slot", "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("user-init");
    sp.addUserPatch("Init.sxsnp", "Keys/EP 3.sxsnp");

    Engine e;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);

    auto userIdx = userIndexOf(sp.pm, "Init.sxsnp");
    b.setValueFromGUI(userIdx);

    REQUIRE(b.getValue() == userIdx);
}

TEST_CASE("A factory preset shadowed by a user file stays on the factory slot", "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("factory-stays");
    sp.addUserPatch("EP 3.sxsnp", "Keys/EP 3.sxsnp");

    Engine e;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);

    auto facIdx = factoryIndexOf(sp.pm, "Keys", "EP 3.sxsnp");
    REQUIRE(facIdx > 0);

    b.setValueFromGUI(facIdx);

    REQUIRE(b.getValueAsString() == label("Keys", "EP 3"));
    REQUIRE(b.getValue() == facIdx);
}

TEST_CASE("The selection follows its file across a user rescan", "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("rescan");
    sp.addUserPatch("Zulu.sxsnp", "Keys/Organ.sxsnp");

    Engine e;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);

    b.setValueFromGUI(userIndexOf(sp.pm, "Zulu.sxsnp"));
    REQUIRE(b.getValueAsString() == "Zulu");

    // sorts ahead of Zulu, so every later index shifts by one
    sp.addUserPatch("Alpha.sxsnp", "Keys/EP 3.sxsnp");

    REQUIRE(b.getValueAsString() == "Zulu");
    REQUIRE(b.getValue() == userIndexOf(sp.pm, "Zulu.sxsnp"));
}

TEST_CASE("An unidentified patch adds no phantom slot past the last preset", "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("phantom-max");
    sp.addUserPatch("Mine.sxsnp", "Keys/EP 3.sxsnp");

    Engine e;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);

    b.setStateForDisplayName("Never Saved Anywhere");
    REQUIRE(b.getValueAsString() == "Never Saved Anywhere");
    REQUIRE(b.getMin() == -1);

    auto lastReal = (int)(sp.pm.factoryPatchVector.size() + sp.pm.userPatches.size());
    REQUIRE(b.getMax() == lastReal);
    REQUIRE(b.getValueAsStringFor(b.getMax()) == "Mine");
}

TEST_CASE("Jogging to the minimum from an unidentified patch is a no-op", "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("to-min");
    sp.addUserPatch("Mine.sxsnp", "Keys/EP 3.sxsnp");

    Engine e;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);

    b.setStateForDisplayName("Never Saved Anywhere");
    b.setValueFromGUI(b.getMin());

    REQUIRE(b.getValueAsString() == "Never Saved Anywhere");
}

TEST_CASE("An out-of-band rebuild does not relocate onto a same-named factory preset",
          "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("rebuild");
    sp.addUserPatch("MyFolder/EP 3.sxsnp", "Keys/EP 3.sxsnp");

    Engine e;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);

    b.setValueFromGUI(userIndexOf(sp.pm, "MyFolder/EP 3.sxsnp"));

    // what rebuildFromPatchMain does when uiForceRebuild fires: patchMain carries only the name
    b.setStateForDisplayName("EP 3");

    REQUIRE(b.getValueAsString() == label("MyFolder", "EP 3"));
}

TEST_CASE("The selected user preset survives a session save and restore", "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("session");
    sp.addUserPatch("Harmony 4.sxsnp", "Harmony/Harmony 4.sxsnp");

    Engine e;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);
    b.setValueFromGUI(userIndexOf(sp.pm, "Harmony 4.sxsnp"));

    // stateSave / stateLoad: the dawExtraState block round-trips, and the patch itself carries
    // only the bare name "Harmony 4" - which the factory list also has, under Harmony/
    Synth::DawStateMain saved{};
    saved.main = e.des;
    TiXmlElement el("dawExtraState");
    Synth::toDawExtraState(el, saved);

    Synth::DawStateMain restored{};
    Synth::fromDawExtraState(el, restored);

    // a fresh editor binds to the restored session state
    Engine e2;
    e2.des = restored.main;
    ui::PresetDataBinding b2(sp.pm, *e2.patch, *e2.queue, e2.des);
    wireBinding(sp.pm, b2);
    b2.restoreSelectionFromSession("Harmony 4");

    REQUIRE(b2.getValueAsString() == "Harmony 4");
    REQUIRE(b2.getValue() == userIndexOf(sp.pm, "Harmony 4.sxsnp"));
}

TEST_CASE("A stale session identity loses to the name the patch actually carries", "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("stale-session");
    sp.addUserPatch("Harmony 4.sxsnp", "Harmony/Harmony 4.sxsnp");

    Engine e;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);
    b.setValueFromGUI(userIndexOf(sp.pm, "Harmony 4.sxsnp"));

    // something loaded a different preset without going through the binding (the CLAP preset
    // extension does this); patchMain now says Harmony 5 while the session state still says
    // the user's Harmony 4
    b.restoreSelectionFromSession("Harmony 5");

    REQUIRE(b.getValueAsString() == label("Harmony", "Harmony 5"));
}

TEST_CASE("A session with no preset record falls back to matching by name", "[preset][jog]")
{
    ensureDspTables();

    ScratchPresets sp("legacy-session");
    sp.addUserPatch("Harmony 4.sxsnp", "Harmony/Harmony 4.sxsnp");

    // a 1.2-era save: dawExtraState carries colour map / mpe / smoothing but no <preset>
    Synth::DawStateMain legacy{};
    TiXmlElement el("dawExtraState");
    Synth::toDawExtraState(el, legacy);
    REQUIRE(el.FirstChildElement("preset") == nullptr);

    Synth::DawStateMain restored{};
    Synth::fromDawExtraState(el, restored);

    Engine e;
    e.des = restored.main;
    ui::PresetDataBinding b(sp.pm, *e.patch, *e.queue, e.des);
    wireBinding(sp.pm, b);
    b.restoreSelectionFromSession("Harmony 4");

    REQUIRE(b.getValueAsString() == label("Harmony", "Harmony 4"));
}
