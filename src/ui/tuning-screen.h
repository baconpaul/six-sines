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

#ifndef BACONPAUL_SIX_SINES_UI_TUNING_SCREEN_H
#define BACONPAUL_SIX_SINES_UI_TUNING_SCREEN_H

#include <memory>
#include <sst/jucegui/screens/ModalBase.h>
#include <sst/jucegui/components/NamedPanel.h>
#include <sst/jucegui/components/TextPushButton.h>
#include "six-sines-editor.h"

namespace baconpaul::six_sines::ui
{
// the session scl and kbm as plain editable text, deliberately nothing fancier
struct TuningScreen : sst::jucegui::screens::ModalBase, HasEditor
{
    TuningScreen(SixSinesEditor &e);
    ~TuningScreen();

    juce::Point<int> innerContentSize() override { return {720, 540}; }
    void resized() override;
    void onEscape() override { setVisible(false); }

    void doLoad();
    void doApply();
    void doReset();
    void showSessionText();
    void setActiveTab(int t);

    std::unique_ptr<jcmp::NamedPanel> panel;
    std::unique_ptr<juce::TextEditor> sclEditor, kbmEditor;
    std::unique_ptr<jcmp::TextPushButton> loadButton, applyButton, resetButton, closeButton;
    std::unique_ptr<juce::FileChooser> fileChooser;
    int activeTab{0};

  private:
    std::unique_ptr<juce::TextEditor> makeTextEditor();
};
} // namespace baconpaul::six_sines::ui
#endif // BACONPAUL_SIX_SINES_UI_TUNING_SCREEN_H
