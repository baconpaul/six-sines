/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work will be
 * released under GPL3.
 *
 * The source code and license are at https://github.com/baconpaul/six-sines
 */

#ifndef BACONPAUL_SIX_SINES_UI_UI_CONSTANTS_H
#define BACONPAUL_SIX_SINES_UI_UI_CONSTANTS_H

#include <cstdint>

namespace baconpaul::six_sines::ui
{
static constexpr uint32_t uicKnobSize{50};
static constexpr uint32_t uicPowerButtonSize{24};
static constexpr uint32_t uicLabelHeight{18};
static constexpr uint32_t uicLabelGap{2};
static constexpr uint32_t uicMargin{4};
static constexpr uint32_t uicSliderWidth{24};
static constexpr uint32_t uicTitleLabelHeight{22};
static constexpr uint32_t uicTitleLabelInnerBox{18};

static constexpr uint32_t uicLabeledKnobHeight{uicKnobSize + uicLabelHeight + uicLabelGap};
static constexpr uint32_t uicPowerKnobWidth{uicKnobSize + uicPowerButtonSize + uicMargin};

template <typename K, typename L>
inline void positionKnobAndLabel(uint32_t x, uint32_t y, const K &k, const L &l)
{
    auto b = juce::Rectangle<int>(x, y, uicKnobSize, uicKnobSize);
    k->setBounds(b);
    l->setBounds(b.translated(0, uicKnobSize + uicLabelGap).withHeight(uicLabelHeight));
}

template <typename T, typename K, typename L>
inline void positionPowerKnobAndLabel(uint32_t x, uint32_t y, const T &t, const K &k, const L &l,
                                      bool centerPower = true)
{
    auto b = juce::Rectangle<int>(x, y, uicKnobSize + uicPowerButtonSize + uicMargin, uicKnobSize);
    k->setBounds(b.withTrimmedLeft(uicPowerButtonSize + uicMargin));
    if (centerPower)
    {
        t->setBounds(b.withWidth(uicPowerButtonSize)
                         .withTrimmedTop((uicKnobSize - uicPowerButtonSize) / 2)
                         .withTrimmedBottom((uicKnobSize - uicPowerButtonSize) / 2));
    }
    else
    {
        t->setBounds(b.withWidth(uicPowerButtonSize)
                         .withTrimmedTop(uicMargin)
                         .withTrimmedBottom(uicKnobSize - uicPowerButtonSize - uicMargin));
    }
    l->setBounds(b.translated(0, uicKnobSize + uicLabelGap).withHeight(uicLabelHeight));
}

template <typename T, typename S, typename K, typename L>
inline void positionPowerKnobSwitchAndLabel(uint32_t x, uint32_t y, const T &t, const S &s,
                                            const K &k, const L &l)
{
    auto b = juce::Rectangle<int>(x, y, uicKnobSize + uicPowerButtonSize + uicMargin, uicKnobSize);
    k->setBounds(b.withTrimmedLeft(uicPowerButtonSize + uicMargin));
    t->setBounds(b.withWidth(uicPowerButtonSize)
                     .withTrimmedTop(uicMargin)
                     .withTrimmedBottom(uicKnobSize - uicPowerButtonSize - uicMargin));
    s->setBounds(b.withWidth(uicPowerButtonSize)
                     .withTrimmedTop(t->getHeight() + uicMargin)
                     .withTrimmedBottom(uicMargin));

    l->setBounds(b.translated(0, uicKnobSize + uicLabelGap).withHeight(uicLabelHeight));
}
template <typename T> inline void positionTitleLabelAt(int x, int y, int w, const T &t)
{
    t->setBounds(x, y, w, uicTitleLabelInnerBox);
}
} // namespace baconpaul::six_sines::ui
#endif // UI_CONSTANTS_H
