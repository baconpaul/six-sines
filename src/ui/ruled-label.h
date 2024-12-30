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

#ifndef BACONPAUL_SIX_SINES_UI_RULED_LABEL_H
#define BACONPAUL_SIX_SINES_UI_RULED_LABEL_H

#include <sst/jucegui/components/Label.h>
#include "ui-constants.h"

// This really belongs in jucegui
namespace baconpaul::six_sines::ui
{
struct RuledLabel : sst::jucegui::components::Label
{
    RuledLabel() : sst::jucegui::components::Label()
    {
        setJustification(juce::Justification::centred);
    }

    void paint(juce::Graphics &g) override
    {
        g.setColour(getColour(Styles::labelcolor));
        if (!isEnabled())
            g.setColour(getColour(Styles::labelcolor).withAlpha(0.5f));
        auto ft = getFont(Styles::labelfont);
        g.setFont(ft);
        g.drawText(text, getLocalBounds(), justification);
        auto w = juce::GlyphArrangement::getStringWidth(ft, text);
        auto lc =
            style()->getColour(sst::jucegui::components::base_styles::Outlined::styleClass,
                               sst::jucegui::components::base_styles::Outlined::brightoutline);
        g.setColour(lc);
        g.drawLine(0, getHeight() / 2, getWidth() / 2 - w / 2 - uicMargin, getHeight() / 2);
        g.drawLine(getWidth() / 2 + w / 2 + uicMargin, getHeight() / 2, getWidth(),
                   getHeight() / 2);
    }
};

} // namespace baconpaul::six_sines::ui
#endif // RULED_LABEL_H
