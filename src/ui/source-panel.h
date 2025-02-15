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

#ifndef BACONPAUL_SIX_SINES_UI_SOURCE_PANEL_H
#define BACONPAUL_SIX_SINES_UI_SOURCE_PANEL_H

#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/GlyphButton.h>
#include "six-sines-editor.h"
#include "patch-data-bindings.h"

namespace baconpaul::six_sines::ui
{
struct SourcePanel : jcmp::NamedPanel, HasEditor
{
    SourcePanel(SixSinesEditor &);
    ~SourcePanel();

    void resized() override;

    void beginEdit(size_t idx);

    void mouseDown(const juce::MouseEvent &e) override;
    juce::Rectangle<int> rectangleFor(int idx);

    std::unique_ptr<juce::Component> highlight;
    void clearHighlight()
    {
        if (highlight)
            highlight->setVisible(false);
    }

    std::array<std::unique_ptr<jcmp::Knob>, numOps> knobs;
    std::array<std::unique_ptr<PatchContinuous>, numOps> knobsData;
    std::array<std::unique_ptr<jcmp::ToggleButton>, numOps> power;
    std::array<std::unique_ptr<jcmp::Label>, numOps> labels;
    std::array<std::unique_ptr<PatchDiscrete>, numOps> powerData;
    std::array<std::unique_ptr<jcmp::GlyphButton>, numOps> upButton, downButton;

    void adjustRatio(int idx, bool up);
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_PANEL_H
