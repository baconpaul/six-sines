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

#include "sst/jucegui/components/ToggleButton.h"
#include "source-panel.h"
#include "source-sub-panel.h"
#include "ui-constants.h"

namespace baconpaul::six_sines::ui
{
SourcePanel::SourcePanel(SixSinesEditor &e) : jcmp::NamedPanel("Source"), HasEditor(e)
{
    auto &mn = editor.patchCopy.sourceNodes;
    for (auto i = 0U; i < numOps; ++i)
    {
        createComponent(editor, *this, mn[i].ratio.meta.id, knobs[i], knobsData[i], i);
        knobs[i]->setDrawLabel(false);
        addAndMakeVisible(*knobs[i]);

        createComponent(editor, *this, mn[i].active.meta.id, power[i], powerData[i], i);
        power[i]->setDrawMode(sst::jucegui::components::ToggleButton::DrawMode::GLYPH);
        power[i]->setGlyph(sst::jucegui::components::GlyphPainter::POWER);
        addAndMakeVisible(*power[i]);

        labels[i] = std::make_unique<jcmp::Label>();
        labels[i]->setText("Op " + std::to_string(i + 1) + " Ratio");
        addAndMakeVisible(*labels[i]);
    }
}
SourcePanel::~SourcePanel() = default;

void SourcePanel::resized()
{
    auto b = getContentArea().reduced(uicMargin, 0);
    auto x = b.getX();
    auto y = b.getY();
    for (auto i = 0U; i < numOps; ++i)
    {
        positionPowerKnobAndLabel(x, y, power[i], knobs[i], labels[i]);
        x += uicPowerKnobWidth + uicMargin;
    }
}

void SourcePanel::beginEdit(size_t idx)
{
    editor.hideAllSubPanels();
    editor.sourceSubPanel->setVisible(true);
    editor.sourceSubPanel->setIndex(idx);
    editor.singlePanel->setName("Op " + std::to_string(idx + 1) + " Source");
}

} // namespace baconpaul::six_sines::ui