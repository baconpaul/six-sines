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

#ifndef BACONPAUL_SIX_SINES_UI_JUCE_LNF_H
#define BACONPAUL_SIX_SINES_UI_JUCE_LNF_H

#include <juce_gui_basics/juce_gui_basics.h>

namespace baconpaul::six_sines::ui
{
struct SixSinesJuceLookAndFeel : juce::LookAndFeel_V4
{
    juce::Font popupFont{juce::FontOptions()};
    SixSinesJuceLookAndFeel(juce::Font f) : popupFont(f)
    {
        setColour(juce::PopupMenu::ColourIds::backgroundColourId, juce::Colour(0x15, 0x15, 0x15));
        setColour(juce::PopupMenu::ColourIds::highlightedBackgroundColourId,
                  juce::Colour(0x35, 0x35, 0x45));
        setColour(juce::PopupMenu::ColourIds::highlightedTextColourId,
                  juce::Colour(0xFF, 0xFF, 0x80));
        setColour(juce::TabbedComponent::ColourIds::backgroundColourId,
                  juce::Colours::black.withAlpha(0.f));
        setColour(juce::TabbedComponent::ColourIds::outlineColourId,
                  juce::Colours::black.withAlpha(0.f));
        setColour(juce::TabbedButtonBar::ColourIds::tabOutlineColourId,
                  juce::Colours::black.withAlpha(0.f));
    }

    ~SixSinesJuceLookAndFeel() {}

    juce::Font getPopupMenuFont() override { return popupFont; }
    void drawPopupMenuBackgroundWithOptions(juce::Graphics &g, int width, int height,
                                            const juce::PopupMenu::Options &o) override
    {
        auto background = findColour(juce::PopupMenu::backgroundColourId);

        g.fillAll(background);

        g.setColour(findColour(juce::PopupMenu::textColourId).withAlpha(0.6f));
        g.drawRect(0, 0, width, height);
    }
};
} // namespace baconpaul::six_sines::ui
#endif // SHORTCIRCUITXT_SCXTJUCELOOKANDFEEL_H
