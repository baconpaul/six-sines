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

namespace baconpaul::six_sines::ui
{
struct SixSinesEditor;

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

    // invalidate cached digits and repaint the editor — call when the underlying
    // value changed (automation, preset load, knob edit). PatchContinuous doesn't
    // fan its data listeners on writes, so dataChanged() can't carry this for us
    // and the SourcePanel routes external updates through this method directly.
    void refreshFromExternal();

    // call after createComponent has set the source
    void finalizeSetup(SixSinesEditor &e);

    // Fired on a primary-button mouseDown anywhere in the inner editor (the
    // click that activates the typein). SourcePanel uses this to select the
    // owning operator's sub-panel — the inner child consumes the mouse event,
    // so SourcePanel::mouseDown never sees the click.
    std::function<void()> onSelected;

    // SixSinesEditor used to look up cached typefaces (set by finalizeSetup).
    SixSinesEditor *sixSinesEditor{nullptr};

    // decomposition: given underlying log2 value, get t1/t2/t3
    static void decompose(float val, int &t1, int &t2, int &t3);
    // recomposition: given t1/t2/t3, return clamped underlying log2 value
    float recompose(int t1, int t2, int t3) const;

    // read the current digits — returns cached ints when the UI authored the
    // current float; otherwise decomposes from the float.
    void readDigits(int &t1, int &t2, int &t3) const;
    // write digits back through the main source (with begin/end edit when no
    // outer gesture is open)
    void writeDigits(int t1, int t2, int t3);

    void jogSlot(int slot, bool up);

    // t1's range derived from the bound continuous's min/max
    int t1Min() const;
    int t1Max() const;

    // cached digits — UI-owned representation that survives float roundtrip
    mutable int cachedT1{1}, cachedT2{0}, cachedT3{0};
    mutable bool digitsValid{false};

    // Set by the inner editor's drag start/end so writeDigits skips its own
    // BEGIN/END pair while a drag gesture is open.
    bool inOuterGesture{false};

    struct RatioDTE;
    std::unique_ptr<RatioDTE> editor;
    std::array<std::unique_ptr<sst::jucegui::components::GlyphButton>, numSlots> upButtons,
        downButtons;
};
} // namespace baconpaul::six_sines::ui

#endif
