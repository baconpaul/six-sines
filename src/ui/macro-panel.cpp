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

#include "macro-panel.h"
#include "macro-sub-panel.h"
#include "six-sines-editor.h"
#include "ui-constants.h"
#include "knob-highlight.h"

namespace baconpaul::six_sines::ui
{
MacroPanel::MacroPanel(SixSinesEditor &e) : jcmp::NamedPanel("Macros"), HasEditor(e)
{
    auto &mn = editor.patchCopy.macroNodes;
    for (auto i = 0U; i < numMacros; ++i)
    {
        createComponent(editor, *this, mn[i].level, knobs[i], knobsData[i], i);
        knobs[i]->setDrawLabel(false);
        addAndMakeVisible(*knobs[i]);

        sst::jucegui::component_adapters::setTraversalId(knobs[i].get(), i + 1);

        labels[i] = std::make_unique<jcmp::Label>();
        labels[i]->setText("Macro " + std::to_string(i + 1));
        addAndMakeVisible(*labels[i]);

        createComponent(editor, *this, mn[i].macroPower, power[i], powerD[i], i);
        power[i]->setDrawMode(sst::jucegui::components::ToggleButton::DrawMode::GLYPH);
        power[i]->setGlyph(sst::jucegui::components::GlyphPainter::POWER);
        addAndMakeVisible(*power[i]);
    }

    highlight = std::make_unique<KnobHighlight>(editor);
    addChildComponent(*highlight);
}

MacroPanel::~MacroPanel() = default;

void MacroPanel::resized()
{
    auto b = getContentArea().reduced(uicMargin, 0);
    auto x = b.getX();
    auto y = b.getY();
    for (auto i = 0U; i < numMacros; ++i)
    {
        positionPowerKnobAndLabel(x, y, power[i], knobs[i], labels[i], true);
        x += uicPowerKnobWidth + uicMargin;
    }
}

juce::Rectangle<int> MacroPanel::rectangleFor(int idx)
{
    auto b = getContentArea().reduced(uicMargin, 0);
    return juce::Rectangle<int>(b.getX() + idx * (uicPowerKnobWidth + uicMargin), b.getY(),
                                uicPowerKnobWidth + 2, uicLabeledKnobHeight);
}

void MacroPanel::mouseDown(const juce::MouseEvent &e)
{
    if (e.mods.isPopupMenu())
    {
        editor.showNavigationMenu();
        return;
    }
    for (int i = 0; i < numMacros; ++i)
    {
        if (rectangleFor(i).contains(e.position.toInt()))
        {
            beginEdit(i);
        }
    }
}

void MacroPanel::refreshLabel(size_t idx)
{
    if (idx >= numMacros || !labels[idx])
        return;
    labels[idx]->setText(displayShortName(editor, idx));
    labels[idx]->repaint();
}

void MacroPanel::beginEdit(size_t idx)
{
    editor.hideAllSubPanels();
    editor.activateHamburger(true);
    editor.macroSubPanel->setSelectedIndex(idx);
    editor.macroSubPanel->setVisible(true);
    editor.singlePanel->setName(editor.macroSubPanel->displayName());
    highlight->setBounds(rectangleFor(idx));
    highlight->setVisible(true);
    highlight->toBack();
}

} // namespace baconpaul::six_sines::ui
