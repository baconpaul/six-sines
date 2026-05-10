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
        // The toggle's onBeginEdit calls beginEdit(i) → setSelectedIndex →
        // setEnabledState before setValueFromGUI flips macroPower, so the sub-panel
        // would latch the stale value. Re-run setEnabledState after the GUI write.
        powerD[i]->onGuiSetValue = [this, i]()
        {
            if (editor.macroSubPanel && editor.macroSubPanel->isVisible() &&
                editor.macroSubPanel->index == i)
                editor.macroSubPanel->setEnabledState();
        };
        addAndMakeVisible(*power[i]);

        usageButtons[i] = std::make_unique<jcmp::TextPushButton>();
        usageButtons[i]->setLabel("0");
        usageButtons[i]->setOnCallback([this, i]() { showUsageMenu(i); });
        addAndMakeVisible(*usageButtons[i]);
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
        positionPowerKnobSwitchAndLabel(x, y, power[i], usageButtons[i], knobs[i], labels[i]);
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

void MacroPanel::refreshUsage(size_t idx)
{
    if (idx >= numMacros)
        return;
    auto count = editor.macroUsageCache[idx].size();
    if (usageButtons[idx])
    {
        usageButtons[idx]->setLabel(std::to_string(count));
        usageButtons[idx]->setEnabled(count > 0);
        usageButtons[idx]->repaint();
    }
    if (labels[idx])
    {
        labels[idx]->setEnabled(count > 0);
        labels[idx]->repaint();
    }
}

void MacroPanel::showUsageMenu(size_t idx)
{
    if (idx >= numMacros)
        return;
    auto &uses = editor.macroUsageCache[idx];
    if (uses.empty())
        return;

    auto p = juce::PopupMenu();
    p.addSectionHeader(displayShortName(editor, idx));
    p.addSeparator();
    for (auto &u : uses)
    {
        auto label = u.nodeLabel + " — " + u.targetName + (u.modulated ? " (mod)" : "");
        auto refCopy = u;
        p.addItem(label,
                  [w = juce::Component::SafePointer(this), refCopy]()
                  {
                      if (w && w->editor.macroSubPanel)
                          w->editor.macroSubPanel->jumpTo(refCopy);
                  });
    }
    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&editor),
                    makeMenuAccessibleButtonCB(usageButtons[idx].get()));
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
