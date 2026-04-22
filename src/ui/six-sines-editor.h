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

#ifndef BACONPAUL_SIX_SINES_UI_SIX_SINES_EDITOR_H
#define BACONPAUL_SIX_SINES_UI_SIX_SINES_EDITOR_H

#include <concepts>
#include <functional>
#include <utility>
#include <juce_gui_basics/juce_gui_basics.h>
#include "sst/jucegui/style/JUCELookAndFeelAdapter.h"

#include <sst/jucegui/components/NamedPanel.h>
#include <sst/jucegui/components/WindowPanel.h>
#include <sst/jucegui/components/Knob.h>
#include <sst/jucegui/components/ToolTip.h>
#include <sst/jucegui/components/ToggleButton.h>
#include <sst/jucegui/components/MenuButton.h>
#include <sst/jucegui/components/JogUpDownButton.h>
#include <sst/jucegui/components/VUMeter.h>
#include <sst/jucegui/accessibility/FocusDebugger.h>
#include <sst/jucegui/data/Continuous.h>
#include <sst/jucegui/screens/ColorEditor.h>

#include "synth/synth.h"
#include "presets/preset-manager.h"
#include "presets/ui-theme-manager.h"
#include "ui-defaults.h"
#include "six-sines-skin.h"

namespace jcmp = sst::jucegui::components;
namespace jdat = sst::jucegui::data;

namespace baconpaul::six_sines::ui
{
template <typename T>
concept HasContinuous = std::derived_from<T, juce::Component> && requires(T &t) {
    { t.continuous() } -> std::same_as<sst::jucegui::data::Continuous *>;
    t.onBeginEdit();
    t.onEndEdit();
    t.notifyAccessibleChange();
};

struct MainPanel;
struct MainSubPanel;
struct MatrixPanel;
struct MatrixSubPanel;
struct SelfSubPanel;
struct MixerPanel;
struct MixerSubPanel;
struct MainPanSubPanel;
struct FineTuneSubPanel;
struct PlayModeSubPanel;
struct SourcePanel;
struct SourceSubPanel;
struct MacroPanel;
struct SettingsPanel;
struct Clipboard;
struct PresetDataBinding;

struct SixSinesEditor : jcmp::WindowPanel
{
    Patch patchCopy;
    ModMatrixConfig modMatrixConfig;

    Synth::audioToUIQueue_t &audioToUI;
    Synth::mainToAudioQueue_T &mainToAudio;
    const clap_host_t *clapHost{nullptr};

    SixSinesEditor(Synth::audioToUIQueue_t &atou, Synth::mainToAudioQueue_T &utoa,
                   const clap_host_t *ch);
    virtual ~SixSinesEditor();

    std::unique_ptr<sst::jucegui::style::LookAndFeelManager> lnf;
    void onStyleChanged() override;

    void paint(juce::Graphics &g) override;
    void resized() override;

    void idle();
    std::unique_ptr<juce::Timer> idleTimer;

    std::unique_ptr<Clipboard> clipboard;

    std::unique_ptr<jcmp::NamedPanel> singlePanel;
    void doSinglePanelHamburger();
    void activateHamburger(bool b);

    std::unique_ptr<MainPanel> mainPanel;
    std::unique_ptr<MainSubPanel> mainSubPanel;
    std::unique_ptr<MainPanSubPanel> mainPanSubPanel;
    std::unique_ptr<FineTuneSubPanel> fineTuneSubPanel;
    std::unique_ptr<PlayModeSubPanel> playModeSubPanel;

    std::unique_ptr<MatrixPanel> matrixPanel;
    std::unique_ptr<MatrixSubPanel> matrixSubPanel;
    std::unique_ptr<SelfSubPanel> selfSubPanel;

    std::unique_ptr<MixerPanel> mixerPanel;
    std::unique_ptr<MixerSubPanel> mixerSubPanel;

    std::unique_ptr<MacroPanel> macroPanel;
    std::unique_ptr<SettingsPanel> settingsPanel;

    std::unique_ptr<SourcePanel> sourcePanel;
    std::unique_ptr<SourceSubPanel> sourceSubPanel;

    std::unique_ptr<presets::PresetManager> presetManager;
    std::unique_ptr<presets::UIThemeManager> uiThemeManager;
    std::unique_ptr<PresetDataBinding> presetDataBinding;
    std::unique_ptr<jcmp::JogUpDownButton> presetButton;
    void showPresetPopup();
    void doLoadPatch();
    void doSavePatch();
    void postPatchChange(const std::string &displayName);
    void resetToDefault();
    void setPatchNameDisplay();
    void setPatchNameTo(const std::string &);
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::DocumentWindow> colorEditorWindow;
    std::unique_ptr<juce::FileChooser> colorThemeFileChooser;

    std::unique_ptr<defaultsProvder_t> defaultsProvider;

    SixSinesSkin currentSkin;

    // Editor-side mirror of the synth's DawExtraState. Kept in sync via
    // SET_DAW_EXTRA_STATE messages (audio->UI on load/attach, UI->audio on color edits).
    // Stable address so messages UI->audio can safely pass &editorDawExtraState.
    Synth::DawExtraState editorDawExtraState;
    // Debounce timer: restarted on every color edit; when it fires, the latest
    // editorDawExtraState is pushed to the audio thread.
    std::unique_ptr<juce::Timer> dawExtraStatePushTimer;
    void scheduleDawExtraStatePush();
    void pushDawExtraStateToAudio();
    // Apply a newly-received DES (from the audio thread) to the editor: if it carries a
    // colour map, parse it and applyTheme().
    void applyDawExtraStateFromAudio();
    // Install the dark base stylesheet + dark skin.  Called once during construction
    // before lnf exists; setThemeFromPreference() then overlays the user's saved theme.
    void initializeBaseSkin();
    // Sentinel prefix used when storing a factory theme name in the themePath user default.
    // A stored value of e.g. "factory:Dark" means use uiThemeManager->factoryThemes["Dark"].
    static constexpr const char *factoryThemeSentinel{"factory:"};

    // Apply a complete skin: updates currentSkin, projects onto the stylesheet, notifies
    // the LookAndFeel, repaints the main editor, and propagates the style update to the
    // colour editor window if it is open.
    // If preference is non-empty it is written to the themePath user default so the choice
    // persists across sessions.  Pass "" (the default) when applying a theme internally
    // (e.g. from setThemeFromPreference) to avoid overwriting the stored preference.
    void applyTheme(const SixSinesSkin &skin, const std::string &preference = "");

    // Read the themePath user default and apply the corresponding theme.  Falls back to
    // darkDefault() if the factory name is not found or the file path no longer exists.
    void setThemeFromPreference();

    void openColorEditor();
    // Safe pointer to the ColorEditorContent (a WindowPanel subclass) so that
    // onStyleChanged() can propagate style changes to the floating colour editor window.
    juce::Component::SafePointer<jcmp::WindowPanel> colorEditorContent;

    std::unique_ptr<jcmp::ToolTip> toolTip;
    void showTooltipOn(juce::Component *c);
    void updateTooltip(jdat::Continuous *c);
    void updateTooltip(jdat::Discrete *d);
    void hideTooltip();

    template <typename T>
        requires HasContinuous<T>
    void popupMenuForContinuous(T *e);

    void hideAllSubPanels();
    std::unordered_map<uint32_t, juce::Component::SafePointer<juce::Component>> componentByID;
    std::unordered_map<uint32_t, std::function<void()>> componentRefreshByID;

    void setAndSendParamValue(uint32_t id, float value, bool notifyAudio = true,
                              bool includeBeginEnd = true);
    void setAndSendParamValue(const Param &p, float value, bool notifyAudio = true,
                              bool includeBeginEnd = true)
    {
        setAndSendParamValue(p.meta.id, value, notifyAudio, includeBeginEnd);
    }

    bool keyPressed(const juce::KeyPress &key) override;

    void showNavigationMenu();

    void setZoomFactor(float zf);
    float zoomFactor{1.0f};
    std::function<void(float)> onZoomChanged{nullptr};
    bool toggleDebug();

    static constexpr uint32_t edWidth{1048}, edHeight{690};

    std::unique_ptr<jcmp::VUMeter> vuMeter;

    // To turn this on, recompile with it on in six-sines-editor.cpp
    void visibilityChanged() override;
    void parentHierarchyChanged() override;
    std::unique_ptr<sst::jucegui::accessibility::FocusDebugger> focusDebugger;

    std::unordered_map<juce::Component *, std::function<void()>> panelSelectGestureFor;

    float engineSR{0}, hostSR{0};

    bool sessionRunAllNodes{false};
    bool sessionAllSoundsOffOnToggle{false};
    bool isRunAllNodes() const;
    bool isAllSoundsOffOnToggle() const;
    void sendDesignModeToAudio();

    void requestParamsFlush();
    const clap_host_params_t *clapParamsExtension{nullptr};

    // the name tells you about the intent. It just makes startup faster
    void sneakyStartupGrabFrom(Patch &other);
};

struct HasEditor
{
    SixSinesEditor &editor;
    HasEditor(SixSinesEditor &e) : editor(e) {}

    void optionalAllSoundsOffOnToggle()
    {
        if (editor.isAllSoundsOffOnToggle())
            editor.mainToAudio.push({Synth::MainToAudioMsg::PANIC_STOP_VOICES});
    }
};
} // namespace baconpaul::six_sines::ui
#endif
