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

#include "settings-panel.h"
#include "ui-constants.h"
#include "playmode-sub-panel.h"

namespace baconpaul::six_sines::ui
{
SettingsPanel::SettingsPanel(SixSinesEditor &e) : jcmp::NamedPanel("Settings"), HasEditor(e)
{
    hasHamburger = false;

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
            beginEdit();
    };
    sst::jucegui::component_adapters::setTraversalId(playScreen, 4);

    voiceCount = std::make_unique<jcmp::Label>();
    setVoiceCount(0);
    addAndMakeVisible(*voiceCount);

    cpuLabel = std::make_unique<jcmp::Label>();
    cpuLabel->setText("CPU: 0.0%");
    addAndMakeVisible(*cpuLabel);
}

SettingsPanel::~SettingsPanel() = default;

void SettingsPanel::resized()
{
    auto sb = getContentArea().reduced(uicMargin, 0);
    int settingsBtnSize = uicKnobSize + 8;
    int btnX = sb.getX() + 2 * uicMargin;
    int btnY = sb.getY() + (sb.getHeight() - settingsBtnSize) / 2;
    playScreen->setBounds(btnX, btnY, settingsBtnSize, settingsBtnSize);

    int labelX = btnX + settingsBtnSize + 2 * uicMargin;
    int labelW = sb.getRight() - labelX;
    int labelH = uicLabelHeight;
    int labelsY = sb.getY() + (sb.getHeight() - 2 * labelH) / 2;
    voiceCount->setBounds(labelX, labelsY, labelW, labelH);
    cpuLabel->setBounds(labelX, labelsY + labelH, labelW, labelH);
}

void SettingsPanel::beginEdit()
{
    suppressPowerOff = true;
    editor.hideAllSubPanels();
    editor.activateHamburger(false);
    suppressPowerOff = false;

    editor.playModeSubPanel->setVisible(true);
    editor.singlePanel->setName("Settings");
}

void SettingsPanel::clearHighlight()
{
    if (playScreenD && !suppressPowerOff)
        playScreenD->setValueFromModel(false);
}
} // namespace baconpaul::six_sines::ui
