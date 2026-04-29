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

#ifndef BACONPAUL_SIX_SINES_UI_MACRO_SUB_PANEL_H
#define BACONPAUL_SIX_SINES_UI_MACRO_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/components/TextEditor.h>
#include <sst/jucegui/components/GlyphButton.h>
#include "six-sines-editor.h"
#include "dahdsr-components.h"
#include "lfo-components.h"
#include "modulation-components.h"
#include "clipboard.h"
#include "synth/macro_usage.h"

namespace baconpaul::six_sines::ui
{
struct MacroSubPanel : juce::Component,
                       HasEditor,
                       DAHDSRComponents<MacroSubPanel, Patch::MacroNode>,
                       LFOComponents<MacroSubPanel, Patch::MacroNode>,
                       ModulationComponents<MacroSubPanel, Patch::MacroNode>,
                       SupportsClipboard
{
    MacroSubPanel(SixSinesEditor &);
    ~MacroSubPanel();

    size_t index{0};
    void setSelectedIndex(size_t i);

    void resized() override;
    void beginEdit() {}

    std::unique_ptr<jcmp::TextEditor> nameEditor;

    std::unique_ptr<jcmp::RuledLabel> nameTitle;
    std::unique_ptr<jcmp::RuledLabel> usedByTitle;

    // One row per consumer. usedByEmptyState=true uses a single row with only
    // nodeLabel populated ("(macro unused)").
    struct UsedByRow
    {
        std::unique_ptr<jcmp::GlyphButton> jumpButton;
        std::unique_ptr<jcmp::Label> nodeLabel;
        std::unique_ptr<jcmp::Label> variantLabel;
        std::unique_ptr<jcmp::Label> targetLabel;
    };
    std::vector<UsedByRow> usedByRows;
    bool usedByEmptyState{false};

    void jumpTo(const MacroUsedRef &ref);

    // Env-/LFO-→ row above the Name section.
    std::unique_ptr<jcmp::RuledLabel> envSecLab, lfoSecLab;
    std::unique_ptr<jcmp::Knob> envDepthKnob, lfoDepthKnob;
    std::unique_ptr<PatchContinuous::cubic_t> envDepthDA, lfoDepthDA;
    std::unique_ptr<jcmp::Label> envDepthLab, lfoDepthLab;
    std::unique_ptr<jcmp::MultiSwitch> envMul;
    std::unique_ptr<PatchDiscrete> envMulD;

    void setEnabledState();
    void refreshUsedByList();

    // Persist the name editor's text into patchCopy, push to audio thread, and
    // refresh the dependent UI (MacroPanel label + singlePanel title).
    void commitName();

    // Re-reads patchCopy.macroNames[index] into nameEditor and refreshes the
    // singlePanel title. Called when the audio thread tells us the name
    // changed (patch load, etc.) and this sub-panel is the visible one.
    void refreshNameFromPatch();

    // "Foo" if user-named, else "Macro N". Used on the MacroPanel knob label.
    std::string displayShortName() const;
    // "Foo (Macro N)" if user-named, else "Macro N". Used on the singlePanel title.
    std::string displayName() const;

    HAS_CLIPBOARD_SUPPORT;
};
} // namespace baconpaul::six_sines::ui
#endif // BACONPAUL_SIX_SINES_UI_MACRO_SUB_PANEL_H
