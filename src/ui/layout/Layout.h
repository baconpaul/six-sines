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

#ifndef BACONPAUL_SIX_SINES_UI_LAYOUT_LAYOUT_H
#define BACONPAUL_SIX_SINES_UI_LAYOUT_LAYOUT_H

#include <iostream>
#include <cassert>

/*
 * This is an experiment to see if we can figure out something which works for more props
 */
namespace sst::jucegui::layout
{
// A#define LOLOG(...) std::cout << __VA_ARGS__ << std::endl;
#define LOLOG(...) ((void)0);
struct LayoutComponent
{
    using loc_t = LayoutComponent;

    loc_t at(int x, int y)
    {
        auto res = *this;
        res.x = x;
        res.y = y;
        return res;
    }

    loc_t withHeight(int h)
    {
        auto res = *this;
        res.h = h;
        return res;
    }

    loc_t withWidth(int w)
    {
        auto res = *this;
        res.w = w;
        return res;
    }

    loc_t insetBy(int xi) { return insetBy(xi, xi); }
    loc_t insetBy(int xi, int yi)
    {
        auto res = *this;
        res.insetx = xi;
        res.insety = yi;
        return res;
    }

    loc_t expandToFill()
    {
        auto res = *this;
        res.isFilling = true;
        return res;
    }

    enum LayoutMechanism
    {
        LEAF,
        VLIST,
        HLIST
    } mechanism;

    loc_t withLayoutMechanism(LayoutMechanism m)
    {
        auto res = *this;
        res.mechanism = m;
        return res;
    }

    loc_t withComponent(juce::Component &c)
    {
        auto res = *this;
        res.component = juce::Component::SafePointer(&c);
        return res;
    }

    loc_t withAutoGap(int g)
    {
        assert(mechanism == VLIST || mechanism == HLIST);
        auto res = *this;
        res.autoGap = g;
        return res;
    }

    void add(const loc_t &l)
    {
        if (autoGap > 0 && !children.empty())
        {
            addGap(autoGap);
        }

        children.push_back(l);
    }

    void addGap(int size)
    {
        if (mechanism == VLIST)
        {
            children.push_back(loc_t().withLayoutMechanism(LayoutComponent::LEAF).withHeight(size));
        }
        else if (mechanism == HLIST)
        {
            children.push_back(loc_t().withLayoutMechanism(LayoutComponent::LEAF).withWidth(size));
        }
        else
        {
            assert(false && "addGap only works with hlist/vlist right now");
        }
    }

    juce::Rectangle<int> doLayout(const std::string &pfx = "+--")
    {
        switch (mechanism)
        {
        case LEAF:
        {
            auto res = juce::Rectangle<int>(x, y, w, h).reduced(insetx, insety);
            if (component)
                component->setBounds(res);
            LOLOG(pfx << " LEAF " << (component ? component->getTitle() : "-gap-") << " @ "
                      << res.toString());
            return res;
        }
        break;
        case HLIST:
        case VLIST:
        {
            std::string tag = (mechanism == HLIST ? "hlist" : "vlist");

            LOLOG(pfx << " " << tag << " x==" << x << " y=" << y << " w=" << w << " h=" << h);
            int fillingCount{0}, unfillingSpace{0};
            for (auto &c : children)
            {
                if (c.isFilling)
                {
                    fillingCount++;
                    LOLOG(pfx << " " << tag << "  Filling Element");
                }
                else
                {
                    auto consumes = (mechanism == HLIST ? c.w : c.h);
                    unfillingSpace += consumes;
                    LOLOG(pfx << " " << tag << "  Specified element consumes=" << consumes
                              << " ufs=" << unfillingSpace);
                }
            }
            auto leftoverSpace = (mechanism == HLIST ? w : h) - unfillingSpace;
            auto fillingItemSize = fillingCount > 0 ? (leftoverSpace / fillingCount) : 0;
            LOLOG(pfx << " " << tag << " fc=" << fillingCount << " fis=" << fillingItemSize);

            auto pos = mechanism == VLIST ? y : x;
            for (auto &c : children)
            {
                if (c.isFilling)
                {
                    if (mechanism == HLIST)
                    {
                        c.w = std::min(fillingItemSize, leftoverSpace);
                    }
                    else
                    {
                        c.h = std::min(fillingItemSize, leftoverSpace);
                    }

                    leftoverSpace -= fillingItemSize;
                }

                if (mechanism == VLIST)
                {
                    if (c.w == 0)
                        c.w = w;
                    c.x = x;
                    c.y = pos;
                    LOLOG(pfx << " " << tag << "  - setting element to " << c.x << "," << c.y
                              << " with dims=" << c.w << "," << c.h << " and pos=" << pos);
                    pos += c.h;
                    LOLOG(pfx << " " << tag << " Post set pos = " << pos);
                }
                else
                {
                    if (c.h == 0)
                        c.h = h;
                    c.x = pos;
                    c.y = y;
                    LOLOG(pfx << " " << tag << "  - setting element to " << c.x << "," << c.y
                              << " with dums=" << c.w << "," << c.h << " and pos=" << pos);
                    pos += c.w;
                }
                c.doLayout(pfx + "|--");
            }

            juce::Rectangle<int> res{x, y, w, h};
            LOLOG(pfx << " " << tag << " Returning " << res.toString());
            return res;
        }
        break;
        }

        return {};
    }

  private:
    // details go here
    int x{0}, y{0}, w{0}, h{0};
    int insetx{0}, insety{0};
    bool isFilling{false};

    int autoGap{0};

    std::vector<loc_t> children;

    juce::Component::SafePointer<juce::Component> component{nullptr};
};

inline LayoutComponent VList()
{
    return LayoutComponent().withLayoutMechanism(LayoutComponent::VLIST);
}

inline LayoutComponent HList()
{
    return LayoutComponent().withLayoutMechanism(LayoutComponent::HLIST);
}

inline LayoutComponent Gap()
{
    return LayoutComponent().withLayoutMechanism(LayoutComponent::LEAF);
}

inline LayoutComponent Component(juce::Component &c)
{
    return LayoutComponent().withLayoutMechanism(LayoutComponent::LEAF).withComponent(c);
}
} // namespace sst::jucegui::layout

#endif
