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

#ifndef BACONPAUL_SIX_SINES_UI_ABOUT_SCREEN_H
#define BACONPAUL_SIX_SINES_UI_ABOUT_SCREEN_H

#include <vector>
#include <string>
#include <utility>
#include <sst/jucegui/screens/ModalBase.h>
#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/TextPushButton.h>
#include "six-sines-editor.h"

namespace baconpaul::six_sines::ui
{
// Modal "About" overlay: icon, acknowledgements text, manual/source links and a
// build/system information table sourced from sst-plugininfra and juce::SystemStats.
struct AboutScreen : sst::jucegui::screens::ModalBase, HasEditor
{
    AboutScreen(SixSinesEditor &e);
    ~AboutScreen();

    juce::Point<int> innerContentSize() override { return {880, 600}; }
    void resized() override;
    void paintContents(juce::Graphics &g) override;
    // Click on the dimmed backdrop (outside the content box) dismisses.
    void mouseDown(const juce::MouseEvent &event) override;

    static constexpr const char *manualURL{
        "https://github.com/baconpaul/six-sines/blob/main/doc/manual.md"};
    static constexpr const char *sourceURL{"https://github.com/baconpaul/six-sines/"};

  private:
    void buildInfo();

    static constexpr int infoRowHeight{22};
    static constexpr int titleFontHeight{46}, subtitleFontHeight{22};
    static constexpr int titleBoxHeight{50}, subtitleBoxHeight{28}, titleSubtitleGap{2};

    juce::Image iconImage;
    std::vector<std::pair<std::string, std::string>> infoRows;
    std::string infoClipboardText;

    // Set in resized(), consumed by paintContents().
    juce::Rectangle<int> iconArea, headerArea, infoArea;

    std::unique_ptr<jcmp::Label> ackLabel;
    std::unique_ptr<jcmp::TextPushButton> manualButton, sourceButton, copyButton;
};
} // namespace baconpaul::six_sines::ui
#endif // BACONPAUL_SIX_SINES_UI_ABOUT_SCREEN_H
