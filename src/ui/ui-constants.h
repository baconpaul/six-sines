/*
 * Six Sines A Sinnin'
 *
 * A mess, with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
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
inline void positionPowerKnobAndLabel(uint32_t x, uint32_t y, const T &t, const K &k, const L &l)
{
    auto b = juce::Rectangle<int>(x, y, uicKnobSize + uicPowerButtonSize + uicMargin, uicKnobSize);
    k->setBounds(b.withTrimmedLeft(uicPowerButtonSize + uicMargin));
    t->setBounds(b.withWidth(uicPowerButtonSize)
                     .withTrimmedTop((uicKnobSize - uicPowerButtonSize) / 2)
                     .withTrimmedBottom((uicKnobSize - uicPowerButtonSize) / 2));
    l->setBounds(b.translated(0, uicKnobSize + uicLabelGap).withHeight(uicLabelHeight));
}
} // namespace baconpaul::six_sines::ui
#endif // UI_CONSTANTS_H
