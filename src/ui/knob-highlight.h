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

#ifndef BACONPAUL_SIX_SINES_UI_KNOB_HIGHLIGHT_H
#define BACONPAUL_SIX_SINES_UI_KNOB_HIGHLIGHT_H

namespace baconpaul::six_sines::ui
{
struct KnobHighlight : public juce::Component
{
    void paint(juce::Graphics &g) override
    {
        g.setColour(juce::Colour(0xFF * 0.3, 0x90 * 0.3, 00));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4);

        g.setColour(juce::Colour(0xFF * 0.5, 0x90 * 0.5, 80));
        g.drawRoundedRectangle(getLocalBounds().toFloat(), 4, 2);
    }
};
} // namespace baconpaul::six_sines::ui
#endif // KNOB_HIGHLIGHT_H
