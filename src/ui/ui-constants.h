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

#ifndef BACONPAUL_SIX_SINES_UI_UI_CONSTANTS_H
#define BACONPAUL_SIX_SINES_UI_UI_CONSTANTS_H

#include <cstdint>
#include "sst/jucegui/layouts/ListLayout.h"

namespace baconpaul::six_sines::ui
{
static constexpr uint32_t uicKnobSize{45};
static constexpr uint32_t uicPowerButtonSize{24};
static constexpr uint32_t uicPowerButtonMargin{4};
static constexpr uint32_t uicLabelHeight{18};
static constexpr uint32_t uicLabelGap{2};
static constexpr uint32_t uicMargin{4};
static constexpr uint32_t uicSliderWidth{24};
static constexpr uint32_t uicTitleLabelHeight{22};
static constexpr uint32_t uicTitleLabelInnerBox{18};

static constexpr uint32_t uicLabeledKnobHeight{uicKnobSize + uicLabelHeight + uicLabelGap};
static constexpr uint32_t uicPowerKnobWidth{uicKnobSize + uicPowerButtonSize + uicMargin};

static constexpr uint32_t uicSubPanelColumnWidth{uicKnobSize + 24};

static constexpr uint32_t uicModulationWidth{150};

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
                         .withTrimmedBottom((uicKnobSize - uicPowerButtonSize) / 2)
                         .reduced(uicPowerButtonMargin));
    }
    else
    {
        t->setBounds(b.withWidth(uicPowerButtonSize)
                         .withTrimmedTop(uicMargin)
                         .withTrimmedBottom(uicKnobSize - uicPowerButtonSize - uicMargin)
                         .withTrimmedBottom(2 * uicPowerButtonMargin)
                         .reduced(uicPowerButtonMargin, 0));
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
                     .withTrimmedBottom(uicKnobSize - uicPowerButtonSize - uicMargin)
                     .withTrimmedBottom(2 * uicPowerButtonMargin)
                     .reduced(uicPowerButtonMargin, 0));
    s->setBounds(b.withWidth(uicPowerButtonSize)
                     .withTrimmedTop(t->getHeight() + 2 * uicMargin)
                     .withTrimmedBottom(0)
                     .reduced(0, 2));

    l->setBounds(b.translated(0, uicKnobSize + uicLabelGap).withHeight(uicLabelHeight));
}
template <typename T> inline void positionTitleLabelAt(int x, int y, int w, const T &t)
{
    t->setBounds(x, y, w, uicTitleLabelInnerBox);
}

// put this in UIC
template <typename T> inline sst::jucegui::layouts::LayoutComponent titleLabelLayout(T &f)
{
    namespace jlo = sst::jucegui::layouts;

    auto res = jlo::VList().withHeight(uicTitleLabelHeight);
    res.add(jlo::Component(*f).withHeight(uicTitleLabelInnerBox));
    res.add(jlo::Gap().withHeight(uicTitleLabelHeight - uicTitleLabelInnerBox));
    return res;
};

template <typename T> inline sst::jucegui::layouts::LayoutComponent titleLabelGaplessLayout(T &f)
{
    namespace jlo = sst::jucegui::layouts;

    return jlo::Component(*f).withHeight(uicTitleLabelInnerBox);
};

template <typename L, typename C>
inline sst::jucegui::layouts::LayoutComponent sideLabel(const L &l, const C &c)
{
    namespace jlo = sst::jucegui::layouts;
    auto ul = jlo::HList().withHeight(uicLabelHeight);
    ul.add(jlo::Component(*l).withWidth(14));
    ul.add(jlo::Component(*c).expandToFill());
    return ul;
};

template <typename L, typename C>
inline sst::jucegui::layouts::LayoutComponent sideLabelSlider(const L &l, const C &c)
{
    namespace jlo = sst::jucegui::layouts;
    auto ul = jlo::HList().withHeight(uicLabelHeight);
    ul.add(jlo::Component(*l).withWidth(14));
    ul.add(jlo::Component(*c).insetBy(0, 2).expandToFill());
    return ul;
};

template <typename K, typename L>
inline sst::jucegui::layouts::LayoutComponent labelKnobLayout(const K &k, const L &l)
{
    namespace jlo = sst::jucegui::layouts;
    auto res =
        jlo::VList().withHeight(uicKnobSize + uicLabelGap + uicLabelHeight).withWidth(uicKnobSize);
    res.add(jlo::Component(*k).withHeight(uicKnobSize));
    res.add(jlo::Gap().withHeight(uicLabelGap));
    res.add(jlo::Component(*l).withHeight(uicLabelHeight));

    return res;
}

inline std::function<void(int)> makeMenuAccessibleButtonCB(juce::Component *c)
{
    return [w = juce::Component::SafePointer(c)](int)
    {
        if (w)
        {
            w->grabKeyboardFocus();
            auto h = w->getAccessibilityHandler();
            if (h)
            {
                h->grabFocus();
                h->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
            }
        }
    };
}

} // namespace baconpaul::six_sines::ui
#endif // UI_CONSTANTS_H
