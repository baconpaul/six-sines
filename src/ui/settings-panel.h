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

#ifndef BACONPAUL_SIX_SINES_UI_SETTINGS_PANEL_H
#define BACONPAUL_SIX_SINES_UI_SETTINGS_PANEL_H

#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/NamedPanel.h>
#include <sst/jucegui/components/ToggleButton.h>
#include <sst/jucegui/component-adapters/DiscreteToReference.h>
#include <fmt/core.h>
#include "six-sines-editor.h"

namespace baconpaul::six_sines::ui
{
struct SettingsPanel : jcmp::NamedPanel, HasEditor
{
    SettingsPanel(SixSinesEditor &);
    ~SettingsPanel();

    void resized() override;
    void beginEdit();
    void clearHighlight();

    void setVoiceCount(int vc) { voiceCount->setText("Voices: " + std::to_string(vc)); }
    void setCpuUsage(double cpu)
    {
        if (std::round(cpu) != std::round(lastCpu))
        {
            cpuLabel->setText(fmt::format("CPU: {} %", std::round(cpu)));
            repaint();
            lastCpu = cpu;
        }
    }

    std::unique_ptr<jcmp::Label> voiceCount;
    std::unique_ptr<jcmp::Label> cpuLabel;
    double lastCpu{-2000};

    bool isPlayScreenShowing{false};
    bool suppressPowerOff{false};
    jcmp::ToggleButton *playScreen{nullptr};
    std::unique_ptr<
        sst::jucegui::component_adapters::DiscreteToValueReference<jcmp::ToggleButton, bool>>
        playScreenD;
};
} // namespace baconpaul::six_sines::ui
#endif // SETTINGS_PANEL_H
