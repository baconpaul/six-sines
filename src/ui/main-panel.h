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

#ifndef BACONPAUL_SIX_SINES_UI_MAIN_PANEL_H
#define BACONPAUL_SIX_SINES_UI_MAIN_PANEL_H

#include <sst/jucegui/component-adapters/DiscreteToReference.h>

#include "sst/jucegui/components/Label.h"
#include "sst/jucegui/components/VUMeter.h"

#include "six-sines-editor.h"
#include "patch-data-bindings.h"

namespace baconpaul::six_sines::ui
{
struct MainPanel : jcmp::NamedPanel, HasEditor
{
    MainPanel(SixSinesEditor &);
    ~MainPanel();

    void resized() override;

    void mouseDown(const juce::MouseEvent &e) override;
    juce::Rectangle<int> rectangleFor(int idx);

    void beginEdit(int which);

    std::unique_ptr<juce::Component> highlight;
    bool supressPowerOff{false};
    void clearHighlight()
    {
        if (highlight)
            highlight->setVisible(false);
        if (playScreenD && !supressPowerOff)
            playScreenD->setValueFromModel(false);
    }

    std::unique_ptr<PatchContinuous> levData;
    std::unique_ptr<jcmp::Knob> lev;
    std::unique_ptr<jcmp::Label> levLabel;

    std::unique_ptr<PatchContinuous> panData;
    std::unique_ptr<jcmp::Knob> pan;
    std::unique_ptr<jcmp::Label> panLabel;

    std::unique_ptr<PatchContinuous> tunData;
    std::unique_ptr<jcmp::Knob> tun;
    std::unique_ptr<jcmp::Label> tunLabel;

    void setVoiceCount(int vc) { voiceCount->setText("V: " + std::to_string(vc)); }
    std::unique_ptr<jcmp::Label> voiceCount;

    jcmp::ToggleButton *playScreen{nullptr}; // owned by the d. detail to fix up later
    std::unique_ptr<
        sst::jucegui::component_adapters::DiscreteToValueReference<jcmp::ToggleButton, bool>>
        playScreenD;
    bool isPlayScreenShowing{false};
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_PANEL_H
