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

#include "main-panel.h"

#include "ui-constants.h"
#include "main-sub-panel.h"
#include "mainpan-sub-panel.h"
#include "finetune-sub-panel.h"
#include "playmode-sub-panel.h"
#include "knob-highlight.h"

namespace baconpaul::six_sines::ui
{
MainPanel::MainPanel(SixSinesEditor &e) : jcmp::NamedPanel("Main"), HasEditor(e)
{
    createComponent(editor, *this, editor.patchCopy.output.level, lev, levData, 0);
    addAndMakeVisible(*lev);
    sst::jucegui::component_adapters::setTraversalId(lev.get(), 1);
    levLabel = std::make_unique<jcmp::Label>();
    levLabel->setText("Level");
    addAndMakeVisible(*levLabel);

    createComponent(editor, *this, editor.patchCopy.output.pan, pan, panData, 1);
    sst::jucegui::component_adapters::setTraversalId(pan.get(), 2);
    addAndMakeVisible(*pan);
    panLabel = std::make_unique<jcmp::Label>();
    panLabel->setText("Pan");
    addAndMakeVisible(*panLabel);

    createComponent(editor, *this, editor.patchCopy.output.fineTune, tun, tunData, 2);
    sst::jucegui::component_adapters::setTraversalId(tun.get(), 3);
    addAndMakeVisible(*tun);
    tunLabel = std::make_unique<jcmp::Label>();
    tunLabel->setText("Tune");
    addAndMakeVisible(*tunLabel);

    tunData->onGuiSetValue = [w = juce::Component::SafePointer(this)]()
    {
        if (!w)
            return;
        w->editor.fineTuneSubPanel->repaint();
    };

    voiceCount = std::make_unique<jcmp::Label>();
    addAndMakeVisible(*voiceCount);
    setVoiceCount(0);

    playScreenD = std::make_unique<
        sst::jucegui::component_adapters::DiscreteToValueReference<jcmp::ToggleButton, bool>>(
        isPlayScreenShowing);
    playScreen = playScreenD->widget.get();
    playScreen->setDrawMode(sst::jucegui::components::ToggleButton::DrawMode::GLYPH_WITH_BG);
    playScreen->setGlyph(sst::jucegui::components::GlyphPainter::SETTINGS);
    addAndMakeVisible(*playScreen);
    playScreenD->onValueChanged = [this](bool b)
    {
        if (b)
        {
            beginEdit(3);
        }
    };
    sst::jucegui::component_adapters::setTraversalId(playScreen, 4);

    highlight = std::make_unique<KnobHighlight>(editor);
    addChildComponent(*highlight);
}
MainPanel::~MainPanel() = default;

void MainPanel::resized()
{
    auto b = getContentArea().reduced(uicMargin, 0);

    positionKnobAndLabel(b.getX(), b.getY(), lev, levLabel);
    b = b.withTrimmedLeft(uicKnobSize + uicMargin);
    positionKnobAndLabel(b.getX(), b.getY(), pan, panLabel);

    b = b.withTrimmedLeft(uicKnobSize + uicMargin);
    positionKnobAndLabel(b.getX(), b.getY(), tun, tunLabel);
    b = b.withTrimmedLeft(uicKnobSize + uicMargin);

    positionKnobAndLabel(b.getX(), b.getY(), playScreenD->widget, voiceCount);

    // vb = vb.translated(0, uicLabelHeight + uicMargin);
    // voiceCount->setBounds(vb);
    // vb = vb.translated(0, uicLabelHeight + uicMargin);
    // voiceLimit->setBounds(vb);
}

void MainPanel::mouseDown(const juce::MouseEvent &e)
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

juce::Rectangle<int> MainPanel::rectangleFor(int idx)
{
    auto b = getContentArea().reduced(uicMargin, 0);
    return juce::Rectangle<int>(b.getX() + idx * uicKnobSize + (idx ? idx - 0.5 : 0) * uicMargin,
                                b.getY(), uicKnobSize + uicMargin, uicLabeledKnobHeight);
}

void MainPanel::beginEdit(int which)
{
    if (which == 3)
        supressPowerOff = true;
    editor.hideAllSubPanels();
    supressPowerOff = false;
    if (which != 3)
    {
        playScreenD->setValueFromModel(false);
    }
    if (which == 0)
    {
        editor.mainSubPanel->setVisible(true);
        editor.singlePanel->setName("Main Output");
        auto b = getContentArea().reduced(uicMargin, 0);
        highlight->setBounds(b.getX(), b.getY(), uicKnobSize + uicMargin, uicLabeledKnobHeight);
    }
    else if (which == 1)
    {
        editor.mainPanSubPanel->setVisible(true);
        editor.singlePanel->setName("Main Panning");
        auto b = getContentArea().reduced(uicMargin, 0);
        highlight->setBounds(b.getX() + uicKnobSize + 0.5 * uicMargin, b.getY(),
                             uicKnobSize + uicMargin, uicLabeledKnobHeight);
    }
    else if (which == 2)
    {
        editor.fineTuneSubPanel->setVisible(true);
        editor.singlePanel->setName("Main Tuning");
        auto b = getContentArea().reduced(uicMargin, 0);
        highlight->setBounds(b.getX() + 2 * uicKnobSize + 1.5 * uicMargin, b.getY(),
                             uicKnobSize + uicMargin, uicLabeledKnobHeight);
    }
    else if (which == 3)
    {
        editor.playModeSubPanel->setVisible(true);
        editor.singlePanel->setName("Settings");
        auto b = getContentArea().reduced(uicMargin, 0);
        highlight->setBounds(b.getX() + 3 * uicKnobSize + 2.5 * uicMargin, b.getY(),
                             uicKnobSize + uicMargin, uicLabeledKnobHeight);
    }
    highlight->setVisible(true);
    highlight->toBack();
}

} // namespace baconpaul::six_sines::ui