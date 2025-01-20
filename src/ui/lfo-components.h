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

#ifndef BACONPAUL_SIX_SINES_UI_LFO_COMPONENTS_H
#define BACONPAUL_SIX_SINES_UI_LFO_COMPONENTS_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/components/VSlider.h>
#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/MultiSwitch.h>
#include "patch-data-bindings.h"
#include "ui-constants.h"
#include "sst/jucegui/components/RuledLabel.h"
#include "ui/layout/Layout.h"

namespace baconpaul::six_sines::ui
{
namespace jcmp = sst::jucegui::components;
template <typename Comp, typename Patch> struct LFOComponents
{
    Comp *asComp() { return static_cast<Comp *>(this); }
    LFOComponents() {}
    void setupLFO(SixSinesEditor &e, const Patch &v)
    {
        using kt_t = sst::jucegui::accessibility::KeyboardTraverser;

        auto c = asComp();
        createComponent(e, *c, v.lfoRate, rate, rateD);
        rateL = std::make_unique<jcmp::Label>();
        rateL->setText("Rate");
        c->addAndMakeVisible(*rate);
        c->addAndMakeVisible(*rateL);

        createComponent(e, *c, v.lfoDeform, deform, deformD);
        deformL = std::make_unique<jcmp::Label>();
        deformL->setText("Deform");
        c->addAndMakeVisible(*deform);
        c->addAndMakeVisible(*deformL);

        createComponent(e, *c, v.lfoShape, shape, shapeD);
        c->addAndMakeVisible(*shape);

        titleLab = std::make_unique<jcmp::RuledLabel>();
        titleLab->setText("LFO");
        c->addAndMakeVisible(*titleLab);

        createComponent(e, *c, v.tempoSync, tempoSync, tempoSyncD);
        tempoSync->setDrawMode(jcmp::ToggleButton::DrawMode::LABELED);
        tempoSync->setLabel("Sync");
        c->addAndMakeVisible(*tempoSync);

        createComponent(e, *c, v.lfoBipolar, bipolar, bipolarD);
        bipolar->setDrawMode(jcmp::ToggleButton::DrawMode::LABELED);
        bipolar->setLabel("Bipolar");
        c->addAndMakeVisible(*bipolar);

        createComponent(e, *c, v.lfoIsEnveloped, isEnv, isEnvD);
        isEnv->setDrawMode(jcmp::ToggleButton::DrawMode::LABELED);
        isEnv->setLabel("* Env");
        c->addAndMakeVisible(*isEnv);

        rateD->setTemposyncPowerPartner(tempoSyncD.get());

        sst::jucegui::component_adapters::setTraversalId(shape.get(), 200);
        sst::jucegui::component_adapters::setTraversalId(bipolar.get(), 201);
        sst::jucegui::component_adapters::setTraversalId(tempoSync.get(), 204);
        sst::jucegui::component_adapters::setTraversalId(rate.get(), 203);
        sst::jucegui::component_adapters::setTraversalId(deform.get(), 205);
        sst::jucegui::component_adapters::setTraversalId(isEnv.get(), 211);
    }

    juce::Rectangle<int> layoutLFOAt(int x, int y, int extraWidth = 0)
    {
        if (!titleLab)
            return {};

        namespace jlo = sst::jucegui::layout;

        auto lo = jlo::VList()
                      .at(x, y)
                      .withHeight(asComp()->getHeight() - y)
                      .withWidth(uicKnobSize * 2.5 + uicMargin);

        lo.add(titleLabelLayout(titleLab));

        auto columns = jlo::HList().expandToFill().withAutoGap(uicMargin);

        auto col1 = jlo::VList().withWidth(uicKnobSize * 1.5).withAutoGap(uicMargin);
        col1.add(jlo::Component(*shape).withHeight(2 * uicLabeledKnobHeight - uicLabelHeight));
        col1.add(jlo::Component(*tempoSync).withHeight(uicLabelHeight));
        col1.add(jlo::Component(*bipolar).withHeight(uicLabelHeight));

        auto col2 = jlo::VList().withWidth(uicKnobSize).withAutoGap(uicMargin);
        col2.add(labelKnobLayout(rate, rateL));
        col2.add(labelKnobLayout(deform, deformL));
        col2.add(jlo::Component(*isEnv).withHeight(uicLabelHeight));

        columns.add(col1);
        columns.add(col2);

        lo.add(columns);

        auto usedSpace = lo.doLayout();
        return usedSpace.translated(usedSpace.getWidth(), 0);
    }

    std::unique_ptr<jcmp::Knob> rate, deform;
    std::unique_ptr<PatchContinuous> rateD, deformD;
    std::unique_ptr<jcmp::Label> rateL, deformL;

    std::unique_ptr<jcmp::MultiSwitch> shape;
    std::unique_ptr<PatchDiscrete> shapeD;
    std::unique_ptr<jcmp::RuledLabel> titleLab;

    std::unique_ptr<jcmp::ToggleButton> tempoSync;
    std::unique_ptr<PatchDiscrete> tempoSyncD;

    std::unique_ptr<jcmp::ToggleButton> bipolar;
    std::unique_ptr<PatchDiscrete> bipolarD;

    std::unique_ptr<jcmp::ToggleButton> isEnv;
    std::unique_ptr<PatchDiscrete> isEnvD;
};
} // namespace baconpaul::six_sines::ui
#endif // LFO_COMPONENTS_H
