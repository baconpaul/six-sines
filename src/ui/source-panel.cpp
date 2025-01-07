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

#include "sst/jucegui/components/ToggleButton.h"
#include "source-panel.h"
#include "source-sub-panel.h"
#include "ui-constants.h"
#include "sst/jucegui/accessibility/KeyboardTraverser.h"
#include "knob-highlight.h"

namespace baconpaul::six_sines::ui
{
SourcePanel::SourcePanel(SixSinesEditor &e) : jcmp::NamedPanel("Source"), HasEditor(e)
{
    using kt_t = sst::jucegui::accessibility::KeyboardTraverser;
    auto &mn = editor.patchCopy.sourceNodes;
    for (auto i = 0U; i < numOps; ++i)
    {
        createComponent(editor, *this, mn[i].ratio, knobs[i], knobsData[i], i);
        knobs[i]->setDrawLabel(false);
        addAndMakeVisible(*knobs[i]);

        createComponent(editor, *this, mn[i].active, power[i], powerData[i], i);
        power[i]->setDrawMode(sst::jucegui::components::ToggleButton::DrawMode::GLYPH);
        power[i]->setGlyph(sst::jucegui::components::GlyphPainter::POWER);
        addAndMakeVisible(*power[i]);

        kt_t::assignTraversalIndex(power[i].get(), i * 5 + 20);
        kt_t::assignTraversalIndex(knobs[i].get(), i * 5 + 21);

        labels[i] = std::make_unique<jcmp::Label>();
        labels[i]->setText("Op " + std::to_string(i + 1) + " Ratio");
        addAndMakeVisible(*labels[i]);
    }

    highlight = std::make_unique<KnobHighlight>();
    addChildComponent(*highlight);
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

void SourcePanel::mouseDown(const juce::MouseEvent &e)
{
    if (e.mods.isPopupMenu())
    {
        editor.showNavigationMenu();
        return;
    }
    for (int i = 0; i < numOps; ++i)
    {
        if (rectangleFor(i).contains(e.position.toInt()))
        {
            beginEdit(i);
        }
    }
}

juce::Rectangle<int> SourcePanel::rectangleFor(int idx)
{
    auto b = getContentArea().reduced(uicMargin, 0);
    return juce::Rectangle<int>(b.getX() + idx * (uicPowerKnobWidth + uicMargin), b.getY(),
                                uicPowerKnobWidth + 2, uicLabeledKnobHeight);
}

void SourcePanel::beginEdit(size_t idx)
{
    editor.hideAllSubPanels();
    editor.sourceSubPanel->setVisible(true);
    editor.sourceSubPanel->setSelectedIndex(idx);
    editor.singlePanel->setName("Op " + std::to_string(idx + 1) + " Source");

    highlight->setVisible(true);
    highlight->setBounds(rectangleFor(idx));
    highlight->toBack();
}

} // namespace baconpaul::six_sines::ui