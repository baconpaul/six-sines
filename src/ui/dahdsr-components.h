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

#ifndef BACONPAUL_SIX_SINES_UI_DAHDSR_COMPONENTS_H
#define BACONPAUL_SIX_SINES_UI_DAHDSR_COMPONENTS_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/components/VSlider.h>
#include <sst/jucegui/components/Label.h>
#include "patch-data-bindings.h"
#include "ui-constants.h"

namespace baconpaul::six_sines::ui
{
namespace jcmp = sst::jucegui::components;
template <typename Comp, typename Patch> struct DAHDSRComponents
{
    Comp *asComp() { return static_cast<Comp *>(this); }
    DAHDSRComponents() {}
    void setupDAHDSR(SixSinesEditor &e, const Patch &v)
    {
        auto mk = [&e, this](auto id, auto idx, auto lb)
        {
            auto c = asComp();
            createComponent(e, *c, id, slider[idx], sliderD[idx]);
            lab[idx] = std::make_unique<jcmp::Label>();
            lab[idx]->setText(lb);
            c->addAndMakeVisible(*slider[idx]);
            c->addAndMakeVisible(*lab[idx]);
        };
        mk(v.delay.meta.id, 0, "del");
        mk(v.attack.meta.id, 1, "atk");
        mk(v.hold.meta.id, 2, "hld");
        mk(v.decay.meta.id, 3, "dcy");
        mk(v.sustain.meta.id, 4, "sus");
        mk(v.release.meta.id, 5, "rel");
    }

    juce::Rectangle<int> layoutDAHDSRAt(int x, int y)
    {
        auto c = asComp();
        auto h = c->getHeight() - y;
        auto q = h - uicLabelHeight - uicLabelGap;
        auto w = uicSliderWidth;

        auto bx = juce::Rectangle<int>(x, y, w, h);
        for (int i = 0; i < nels; ++i)
        {
            if (!slider[i])
                continue;
            slider[i]->setBounds(bx.withHeight(q));
            lab[i]->setBounds(bx.withTrimmedTop(q + uicLabelGap));
            bx = bx.translated(uicSliderWidth, 0);
        }
        return juce::Rectangle<int>(x, y, 0, 0).withLeft(bx.getRight()).withBottom(bx.getBottom());
    }

    static constexpr int nels{6}; // dahdsr
    std::array<std::unique_ptr<jcmp::VSlider>, nels> slider;
    std::array<std::unique_ptr<PatchContinuous>, nels> sliderD;
    std::array<std::unique_ptr<jcmp::Label>, nels> lab;
};
} // namespace baconpaul::six_sines::ui
#endif // DAHDSR_COMPONENTS_H
