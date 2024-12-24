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
#include "ruled-label.h"

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
        mk(v.delay, 0, "D");
        mk(v.attack, 1, "A");
        mk(v.hold, 2, "H");
        mk(v.decay, 3, "D");
        mk(v.sustain, 4, "S");
        mk(v.release, 5, "R");

        auto c = asComp();
        createComponent(e, *c, v.aShape, shapes[0], shapesD[0]);
        createComponent(e, *c, v.dShape, shapes[1], shapesD[1]);
        createComponent(e, *c, v.rShape, shapes[2], shapesD[2]);

        c->addAndMakeVisible(*shapes[0]);
        c->addAndMakeVisible(*shapes[1]);
        c->addAndMakeVisible(*shapes[2]);

        titleLab = std::make_unique<RuledLabel>();
        titleLab->setText("Envelope");
        asComp()->addAndMakeVisible(*titleLab);
    }

    juce::Rectangle<int> layoutDAHDSRAt(int x, int y)
    {
        if (!titleLab)
            return {};

        auto lh = uicTitleLabelHeight;
        auto c = asComp();
        auto h = c->getHeight() - y;
        auto q = h - uicLabelHeight - uicLabelGap - lh;
        auto w = uicSliderWidth;

        positionTitleLabelAt(x, y, nels * uicSliderWidth, titleLab);

        auto bx = juce::Rectangle<int>(x, y + lh, w, h - lh);
        for (int i = 0; i < nels; ++i)
        {
            if (!slider[i])
                continue;
            slider[i]->setBounds(bx.withHeight(q).withTrimmedTop(uicSliderWidth));
            if (i % 2)
            {
                shapes[i / 2]->setBounds(bx.withHeight(uicSliderWidth));
            }
            lab[i]->setBounds(bx.withTrimmedTop(q + uicLabelGap));
            bx = bx.translated(uicSliderWidth, 0);
        }
        bx = bx.translated(-uicSliderWidth, 0);
        return juce::Rectangle<int>(x, y, 0, 0).withLeft(bx.getRight()).withBottom(bx.getBottom());
    }

    static constexpr int nels{6};   // dahdsr
    static constexpr int nShape{3}; // no shape for delay jp;d or release
    std::array<std::unique_ptr<jcmp::VSlider>, nels> slider;
    std::array<std::unique_ptr<PatchContinuous>, nels> sliderD;
    std::array<std::unique_ptr<jcmp::Knob>, nShape> shapes;
    std::array<std::unique_ptr<PatchContinuous>, nShape> shapesD;
    std::array<std::unique_ptr<jcmp::Label>, nels> lab;
    std::unique_ptr<RuledLabel> titleLab;
};
} // namespace baconpaul::six_sines::ui
#endif // DAHDSR_COMPONENTS_H
