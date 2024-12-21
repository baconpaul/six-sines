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

#ifndef BACONPAUL_SIX_SINES_UI_LFO_COMPONENTS_H
#define BACONPAUL_SIX_SINES_UI_LFO_COMPONENTS_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/components/VSlider.h>
#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/MultiSwitch.h>
#include "patch-data-bindings.h"
#include "ui-constants.h"

namespace baconpaul::six_sines::ui
{
namespace jcmp = sst::jucegui::components;
template <typename Comp, typename Patch> struct LFOComponents
{
    Comp *asComp() { return static_cast<Comp *>(this); }
    LFOComponents() {}
    void setupLFO(SixSinesEditor &e, const Patch &v)
    {
        auto c = asComp();
        createComponent(e, *c, v.lfoRate.meta.id, rate, rateD);
        rateL = std::make_unique<jcmp::Label>();
        rateL->setText("Rate");
        c->addAndMakeVisible(*rate);
        c->addAndMakeVisible(*rateL);

        createComponent(e, *c, v.lfoDeform.meta.id, deform, deformD);
        deformL = std::make_unique<jcmp::Label>();
        deformL->setText("Deform");
        c->addAndMakeVisible(*deform);
        c->addAndMakeVisible(*deformL);

        createComponent(e, *c, v.lfoShape.meta.id, shape, shapeD);
        c->addAndMakeVisible(*shape);
    }

    juce::Rectangle<int> layoutLFOAt(int x, int y)
    {
        auto c = asComp();
        auto h = c->getHeight() - y;
        auto q = h;
        auto w = uicKnobSize * 1.5;

        auto bx = juce::Rectangle<int>(x, y, w, h);
        shape->setBounds(bx.withHeight(2 * uicLabeledKnobHeight + uicMargin));

        bx = bx.translated(w + uicMargin, 0);
        positionKnobAndLabel(bx.getX(), bx.getY(), rate, rateL);
        positionKnobAndLabel(bx.getX(), bx.getY() + uicLabeledKnobHeight + uicMargin, deform,
                             deformL);
        bx = bx.translated(uicKnobSize + uicMargin, 0);
        return juce::Rectangle<int>(x + w + uicKnobSize + uicMargin, y, 0, 0)
            .withBottom(bx.getBottom());
    }

    std::unique_ptr<jcmp::Knob> rate, deform;
    std::unique_ptr<PatchContinuous> rateD, deformD;
    std::unique_ptr<jcmp::Label> rateL, deformL;

    std::unique_ptr<jcmp::MultiSwitch> shape;
    std::unique_ptr<PatchDiscrete> shapeD;
};
} // namespace baconpaul::six_sines::ui
#endif // LFO_COMPONENTS_H
