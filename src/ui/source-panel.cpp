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
#include <cmath>

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

        labels[i] = std::make_unique<jcmp::Label>();
        labels[i]->setText("Op " + std::to_string(i + 1) + " Ratio");
        addAndMakeVisible(*labels[i]);

        upButton[i] = std::make_unique<jcmp::GlyphButton>(jcmp::GlyphPainter::GlyphType::PLUS);
        upButton[i]->setTitle("Increase Op " + std::to_string(i + 1) + " Ratio");
        upButton[i]->setOnCallback(
            [w = juce::Component::SafePointer(this), i]()
            {
                if (w)
                    w->adjustRatio(i, true);
            });
        addAndMakeVisible(*upButton[i]);
        downButton[i] = std::make_unique<jcmp::GlyphButton>(jcmp::GlyphPainter::GlyphType::MINUS);
        downButton[i]->setTitle("Decrease Op " + std::to_string(i + 1) + " Ratio");
        downButton[i]->setOnCallback(
            [w = juce::Component::SafePointer(this), i]()
            {
                if (w)
                    w->adjustRatio(i, false);
            });
        addAndMakeVisible(*downButton[i]);
        sst::jucegui::component_adapters::setTraversalId(power[i].get(), i * 5 + 20);
        sst::jucegui::component_adapters::setTraversalId(downButton[i].get(), i * 5 + 20);
        sst::jucegui::component_adapters::setTraversalId(upButton[i].get(), i * 5 + 20);
        sst::jucegui::component_adapters::setTraversalId(knobs[i].get(), i * 5 + 21);
    }

    highlight = std::make_unique<KnobHighlight>(editor);
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
        positionPowerKnobSwitchAndLabel(x, y, power[i], upButton[i], knobs[i], labels[i]);
        auto b = upButton[i]->getBounds().reduced(2, 0).translated(0, -2);
        auto ub = b.withTrimmedLeft(b.getWidth() / 2).withTrimmedBottom(uicMargin / 2);
        auto db = b.withTrimmedRight(b.getWidth() / 2).withTrimmedBottom(uicMargin / 2);
        upButton[i]->setBounds(ub);
        downButton[i]->setBounds(db);
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
    editor.activateHamburger(true);
    editor.sourceSubPanel->setVisible(true);
    editor.sourceSubPanel->setSelectedIndex(idx);
    editor.singlePanel->setName("Op " + std::to_string(idx + 1) + " Source");

    highlight->setVisible(true);
    highlight->setBounds(rectangleFor(idx));
    highlight->toBack();
}

void SourcePanel::adjustRatio(int idx, bool up)
{
    auto rat = knobsData[idx]->getValue();

    double newValue{0};

    auto m = juce::ModifierKeys::getCurrentModifiers();
    if (m.isCommandDown())
    {
        double ttr{0.};
        if (up)
            ttr = std::floor(rat + 0.001);
        else
            ttr = std::ceil(rat - 0.001);
        newValue = std::clamp((float)(ttr + (up ? 1 : -1)), knobsData[idx]->getMin(),
                              knobsData[idx]->getMax());
    }
    else
    {
        // step by harmonics so need to step in log2 space
        float sideSign{1.0};
        if (rat < 0)
            sideSign = -1.0;

        rat *= sideSign;

        double ttr{0.};
        if (up)
            ttr = std::floor(pow(2.0, rat) + 0.001);
        else
            ttr = std::ceil(pow(2.0, rat) - 0.001);

        if (up)
        {
            ttr += sideSign * 1;
        }
        else
        {
            if (ttr == 1)
                ttr = 0.5;
            else
                ttr -= sideSign * 1;
        }
        newValue = std::clamp(sideSign * (float)std::log2(ttr), knobsData[idx]->getMin(),
                              knobsData[idx]->getMax());
    }

    knobs[idx]->onBeginEdit();
    knobsData[idx]->setValueFromGUI(newValue);
    knobs[idx]->onEndEdit();
    repaint();
}

} // namespace baconpaul::six_sines::ui