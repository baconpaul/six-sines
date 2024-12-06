/*
 * BaconPaul's FM Atrocity
 *
 * A mess, with FM.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#include "ifm-editor.h"

namespace baconpaul::fm::ui
{
IFMEditor::IFMEditor() { setSize(900, 340); }

void IFMEditor::paint(juce::Graphics &g)
{
    auto w = getWidth();
    auto h = getHeight();
    auto x = 0;
    auto y = 0;

    int it{0};
    while (w > 0 && h > 0)
    {
        auto r = 255 * std::sin(it * 2 * M_PI * 0.21);
        g.setColour(juce::Colour(r, (it * 10) % 255, 255 - r));
        g.fillRect(x, y, w, h);
        x += 10;
        y += 10;
        w -= 20;
        h -= 20;
        it++;
    }
}

} // namespace baconpaul::fm::ui