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

#ifndef BACONPAUL_SIX_SINES_UI_SEGMENTED_RATIO_EDITOR_H
#define BACONPAUL_SIX_SINES_UI_SEGMENTED_RATIO_EDITOR_H

#include <array>
#include <memory>
#include <sst/jucegui/components/ContinuousParamEditor.h>
#include <sst/jucegui/components/DraggableTextEditableValue.h>
#include <sst/jucegui/components/GlyphButton.h>
#include <sst/jucegui/data/Continuous.h>

namespace baconpaul::six_sines::ui
{
struct SegmentedRatioEditor : sst::jucegui::components::ContinuousParamEditor
{
    static constexpr int numSlots = 3;

    SegmentedRatioEditor();
    ~SegmentedRatioEditor() override;

    void paint(juce::Graphics &g) override {}
    void resized() override;

    // we delegate all real input to children, but right-click on the editor's
    // own empty area should still raise the param popup menu.
    void mouseDown(const juce::MouseEvent &e) override
    {
        if (e.mods.isPopupMenu() && onPopupMenu)
            onPopupMenu(e.mods);
    }
    void mouseDrag(const juce::MouseEvent &) override {}
    void mouseUp(const juce::MouseEvent &) override {}
    void mouseDoubleClick(const juce::MouseEvent &) override {}

    // invalidate cached digits and repaint slot widgets — call when the underlying
    // value changed (automation, preset load, knob edit). PatchContinuous doesn't
    // fan its data listeners on writes, so dataChanged() can't carry this for us
    // and the SourcePanel routes external updates through this method directly.
    void refreshFromExternal();

    // call after createComponent has set the source
    void finalizeSetup();

    // decomposition: given underlying log2 value, get t1/t2/t3
    static void decompose(float val, int &t1, int &t2, int &t3);
    // recomposition: given t1/t2/t3, return clamped underlying log2 value
    float recompose(int t1, int t2, int t3) const;

    // read the current digits — returns cached ints when the UI authored the
    // current float; otherwise decomposes from the float.
    void readDigits(int &t1, int &t2, int &t3) const;
    // write digits back through the main source (with begin/end edit)
    void writeDigits(int t1, int t2, int t3);

    void jogSlot(int slot, bool up);

    // cached digits — UI-owned representation that survives float roundtrip
    mutable int cachedT1{1}, cachedT2{0}, cachedT3{0};
    mutable bool digitsValid{false};

    // Set by the slot widgets' begin/end-edit hooks while a drag gesture is open.
    // While true, writeDigits skips its own BEGIN/END pair so we don't nest
    // gestures within the slot widget's outer one.
    bool inOuterGesture{false};

    struct DigitSource;
    std::array<std::unique_ptr<DigitSource>, numSlots> slotSources;
    std::array<std::unique_ptr<sst::jucegui::components::DraggableTextEditableValue>, numSlots>
        slotEditors;
    std::array<std::unique_ptr<sst::jucegui::components::GlyphButton>, numSlots> upButtons,
        downButtons;
};
} // namespace baconpaul::six_sines::ui

#endif
