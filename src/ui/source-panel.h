/*
 * Six Sines A Sinnin'
 *
 * A mess, with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#ifndef BACONPAUL_SIX_SINES_UI_SOURCE_PANEL_H
#define BACONPAUL_SIX_SINES_UI_SOURCE_PANEL_H

#include <sst/jucegui/components/Label.h>
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

    std::unique_ptr<juce::Component> highlight;
    void clearHighlight()
    {
        if (highlight)
            highlight->setVisible(false);
    }

    std::array<std::unique_ptr<jcmp::Knob>, numOps> knobs;
    std::array<std::unique_ptr<PatchContinuous>, numOps> knobsData;
    std::array<std::unique_ptr<jcmp::ToggleButton>, numOps> power;
    std::array<std::unique_ptr<PatchDiscrete>, numOps> powerData;
    std::array<std::unique_ptr<jcmp::Label>, numOps> labels;
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_PANEL_H
