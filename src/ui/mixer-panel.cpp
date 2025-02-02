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

#include "mixer-panel.h"
#include "mixer-sub-panel.h"
#include "six-sines-editor.h"
#include "ui-constants.h"
#include "knob-highlight.h"

namespace baconpaul::six_sines::ui
{
MixerPanel::MixerPanel(SixSinesEditor &e) : jcmp::NamedPanel("Mixer"), HasEditor(e)
{
    using kt_t = sst::jucegui::accessibility::KeyboardTraverser;

    auto &mn = editor.patchCopy.mixerNodes;
    for (auto i = 0U; i < numOps; ++i)
    {
        createComponent(editor, *this, mn[i].level, knobs[i], knobsData[i], i);
        knobs[i]->setDrawLabel(false);
        addAndMakeVisible(*knobs[i]);

        createComponent(editor, *this, mn[i].active, power[i], powerData[i], i);
        power[i]->setDrawMode(sst::jucegui::components::ToggleButton::DrawMode::GLYPH);
        power[i]->setGlyph(sst::jucegui::components::GlyphPainter::POWER);
        addAndMakeVisible(*power[i]);

        labels[i] = std::make_unique<jcmp::Label>();
        labels[i]->setText("Op " + std::to_string(i + 1) + " Level");
        addAndMakeVisible(*labels[i]);

        knobsData[i]->onPullFromMin = [i, w = juce::Component::SafePointer(this)]()
        {
            if (!w)
                return;
            w->editor.setParamValueOnCopy(w->editor.patchCopy.mixerNodes[i].active.meta.id, true,
                                          true);
            w->editor.setParamValueOnCopy(w->editor.patchCopy.sourceNodes[i].active.meta.id, true,
                                          true);
            w->repaint();
        };

        createComponent(editor, *this, mn[i].pan, panKnobs[i], panKnobsData[i], i);
        addAndMakeVisible(*panKnobs[i]);
        panLabels[i] = std::make_unique<jcmp::Label>();
        panLabels[i]->setText("Pan");
        addAndMakeVisible(*panLabels[i]);

        sst::jucegui::component_adapters::setTraversalId(power[i].get(), i * 12 + 50);
        sst::jucegui::component_adapters::setTraversalId(knobs[i].get(), i * 12 + 51);
        sst::jucegui::component_adapters::setTraversalId(panKnobs[i].get(), i * 12 + 52);
    }

    highlight = std::make_unique<KnobHighlight>(editor);
    addChildComponent(*highlight);
}
MixerPanel::~MixerPanel() = default;

void MixerPanel::resized()
{
    auto b = getContentArea().reduced(uicMargin, 0);
    auto x = b.getX();
    auto y = b.getY();
    for (auto i = 0U; i < numOps; ++i)
    {
        positionPowerKnobAndLabel(x, y, power[i], knobs[i], labels[i]);
        positionKnobAndLabel(x + uicPowerKnobWidth + uicMargin, y, panKnobs[i], panLabels[i]);
        y += uicLabeledKnobHeight + uicMargin;
    }
}

void MixerPanel::mouseDown(const juce::MouseEvent &e)
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

juce::Rectangle<int> MixerPanel::rectangleFor(int idx)
{
    auto b = getContentArea().reduced(uicMargin, 0);
    return juce::Rectangle<int>(b.getX(), b.getY() + idx * (uicLabeledKnobHeight + uicMargin),
                                uicPowerKnobWidth + uicMargin + uicKnobSize + 2,
                                uicLabeledKnobHeight);
}

void MixerPanel::beginEdit(size_t idx)
{
    editor.hideAllSubPanels();
    editor.mixerSubPanel->setSelectedIndex(idx);
    editor.mixerSubPanel->setVisible(true);

    editor.singlePanel->setName("Op " + std::to_string(idx + 1) + " Mix");

    highlight->setVisible(true);
    highlight->setBounds(rectangleFor(idx));
    highlight->toBack();
}

} // namespace baconpaul::six_sines::ui