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
#include <sst/jucegui/components/HSliderFilled.h>
#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/MultiSwitch.h>
#include <sst/jucegui/components/JogUpDownButton.h>
#include "patch-data-bindings.h"
#include "ui-constants.h"
#include "sst/jucegui/components/RuledLabel.h"
#include "sst/jucegui/layouts/ListLayout.h"

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
        deformL->setText("D");
        c->addAndMakeVisible(*deform);
        c->addAndMakeVisible(*deformL);

        createComponent(e, *c, v.lfoStartPhase, phase, phaseD);
        phaseL = std::make_unique<jcmp::Label>();
        phaseL->setText(std::string() + u8"\U000003C6");
        c->addAndMakeVisible(*phase);
        c->addAndMakeVisible(*phaseL);

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

        for (size_t i = 0; i < numSeqSteps; ++i)
        {
            createComponent(e, *c, v.lfoSeqSteps[i], stepSlider[i], stepSliderD[i]);
            c->addAndMakeVisible(*stepSlider[i]);
            sst::jucegui::component_adapters::setTraversalId(stepSlider[i].get(), 220 + (int)i);
        }
        createComponent(e, *c, v.lfoStepCount, stepCount, stepCountD);
        c->addAndMakeVisible(*stepCount);
        sst::jucegui::component_adapters::setTraversalId(stepCount.get(), 240);

        createComponent(e, *c, v.lfoCycleMode, cycleMode, cycleModeD);
        cycleMode->setDrawMode(jcmp::ToggleButton::DrawMode::LABELED);
        cycleMode->setLabel("cyc");
        c->addAndMakeVisible(*cycleMode);
        sst::jucegui::component_adapters::setTraversalId(cycleMode.get(), 241);

        auto updateStep = [this]() { lfoSetEnabledState(shape->isEnabled()); };
        shapeD->onGuiSetValue = updateStep;
        stepCountD->onGuiSetValue = updateStep;
        e.componentRefreshByID[v.lfoShape.meta.id] = updateStep;
        e.componentRefreshByID[v.lfoStepCount.meta.id] = updateStep;

        rateD->setTemposyncPowerPartner(tempoSyncD.get());

        sst::jucegui::component_adapters::setTraversalId(shape.get(), 200);
        sst::jucegui::component_adapters::setTraversalId(bipolar.get(), 201);
        sst::jucegui::component_adapters::setTraversalId(tempoSync.get(), 204);
        sst::jucegui::component_adapters::setTraversalId(rate.get(), 203);
        sst::jucegui::component_adapters::setTraversalId(deform.get(), 205);
        sst::jucegui::component_adapters::setTraversalId(isEnv.get(), 211);

        lfoSetEnabledState(true);
    }

    void lfoSetEnabledState(bool globallyEnabled)
    {
        if (!shape)
            return;
        auto isStep = ((int)std::round(shapeD->getValue()) == 7);
        auto count = (int)std::round(stepCountD->getValue());

        rate->setEnabled(globallyEnabled);
        deform->setEnabled(globallyEnabled);
        phase->setEnabled(globallyEnabled);
        shape->setEnabled(globallyEnabled);
        tempoSync->setEnabled(globallyEnabled);
        bipolar->setEnabled(globallyEnabled && !isStep);
        isEnv->setEnabled(globallyEnabled);
        stepCount->setEnabled(globallyEnabled && isStep);
        cycleMode->setEnabled(globallyEnabled && isStep);
        for (size_t i = 0; i < numSeqSteps; ++i)
            stepSlider[i]->setEnabled(globallyEnabled && isStep && (int)i < count);

        asComp()->repaint();
    }

    juce::Rectangle<int> layoutLFOAt(int x, int y, int width = -1)
    {
        if (!titleLab)
            return {};

        namespace jlo = sst::jucegui::layouts;

        auto w = (width > 0) ? width : (asComp()->getWidth() - x - uicMargin);
        auto h = 180;

        auto lo = jlo::VList().at(x, y).withHeight(h).withWidth(w);

        lo.add(titleLabelLayout(titleLab));

        auto columns = jlo::HList().expandToFill().withAutoGap(uicMargin);

        auto col1 = jlo::VList().withWidth(uicKnobSize * 1.5).withAutoGap(uicMargin);
        col1.add(jlo::Component(*shape).withHeight(uicLabeledKnobHeight + 3 * uicMargin +
                                                   3 * uicLabelHeight));
        col1.add(jlo::Component(*bipolar).withHeight(uicLabelHeight));

        auto col2 = jlo::VList().withWidth(uicKnobSize * 1.25).withAutoGap(uicMargin);
        col2.add(labelKnobLayout(rate, rateL).centerInParent());
        col2.add(jlo::Component(*tempoSync).withHeight(uicLabelHeight));
        col2.add(sideLabelSlider(deformL, deform));
        col2.add(sideLabelSlider(phaseL, phase));
        col2.add(jlo::Component(*isEnv).withHeight(uicLabelHeight));

        auto col3 = jlo::VList().expandToFill().withAutoGap(0);
        for (size_t i = 0; i < numSeqSteps; ++i)
            col3.add(jlo::Component(*stepSlider[i]).expandToFill());
        col3.addGap(uicMargin);
        auto bottomRow = jlo::HList().withHeight(uicLabelHeight).withAutoGap(uicMargin);
        bottomRow.add(jlo::Component(*stepCount).withWidth(70));
        bottomRow.add(jlo::Component(*cycleMode).expandToFill());
        col3.add(bottomRow);

        columns.add(col1);
        columns.add(col2);
        columns.add(col3);

        lo.add(columns);

        auto usedSpace = lo.doLayout();
        return usedSpace.translated(usedSpace.getWidth(), 0);
    }

    std::unique_ptr<jcmp::Knob> rate;
    std::unique_ptr<jcmp::HSliderFilled> deform, phase;
    std::unique_ptr<PatchContinuous> rateD, deformD, phaseD;
    std::unique_ptr<jcmp::Label> rateL, deformL, phaseL;

    std::unique_ptr<jcmp::MultiSwitch> shape;
    std::unique_ptr<PatchDiscrete> shapeD;
    std::unique_ptr<jcmp::RuledLabel> titleLab;

    std::unique_ptr<jcmp::ToggleButton> tempoSync;
    std::unique_ptr<PatchDiscrete> tempoSyncD;

    std::unique_ptr<jcmp::ToggleButton> bipolar;
    std::unique_ptr<PatchDiscrete> bipolarD;

    std::unique_ptr<jcmp::ToggleButton> isEnv;
    std::unique_ptr<PatchDiscrete> isEnvD;

    std::array<std::unique_ptr<jcmp::HSliderFilled>, numSeqSteps> stepSlider;
    std::array<std::unique_ptr<PatchContinuous>, numSeqSteps> stepSliderD;

    std::unique_ptr<jcmp::JogUpDownButton> stepCount;
    std::unique_ptr<PatchDiscrete> stepCountD;

    std::unique_ptr<jcmp::ToggleButton> cycleMode;
    std::unique_ptr<PatchDiscrete> cycleModeD;
};
} // namespace baconpaul::six_sines::ui
#endif // LFO_COMPONENTS_H
