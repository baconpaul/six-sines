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

    struct StepEditor : juce::Component
    {
        SixSinesEditor &editor;
        const Patch &patch;
        std::array<std::unique_ptr<PatchContinuous>, numSeqSteps> stepD;
        StepEditor(SixSinesEditor &e, const Patch &p) : editor(e), patch(p)
        {
            for (size_t i = 0; i < numSeqSteps; ++i)
                stepD[i] = std::make_unique<PatchContinuous>(e, p.lfoSeqSteps[i].meta.id);

            // These are JUST for the menu callback. The other begin/end we do internally
            onBeginEdit = [this]()
            {
                if (menuContinuousIndex >= 0)
                    editGuard(menuContinuousIndex, true);
            };
            onEndEdit = [this]()
            {
                if (menuContinuousIndex >= 0)
                    editGuard(menuContinuousIndex, false);
            };
        }

        // HasContinuous concept surface
        sst::jucegui::data::Continuous *continuous()
        {
            if (menuContinuousIndex < 0 || menuContinuousIndex >= (int)numSeqSteps)
                return nullptr;
            return stepD[menuContinuousIndex].get();
        }
        std::function<void()> onBeginEdit{[]() {}};
        std::function<void()> onEndEdit{[]() {}};
        // Stub: the menu typein calls this after a value change; the StepEditor
        // doesn't expose per-row accessibility events yet, so nothing to announce.
        void notifyAccessibleChange() {}
        void paint(juce::Graphics &g) override
        {
            auto bg = editor.style()->getColour(jcmp::base_styles::PushButton::styleClass,
                                                jcmp::base_styles::PushButton::fill);
            auto handleCol = editor.style()->getColour(jcmp::HSliderFilled::Styles::styleClass,
                                                       jcmp::HSliderFilled::Styles::handle);
            auto val = editor.style()->getColour(jcmp::HSliderFilled::Styles::styleClass,
                                                 jcmp::HSliderFilled::Styles::value);

            if (!isEnabled())
            {
                val = val.withAlpha(0.2f);
                handleCol = handleCol.withAlpha(0.2f);
            }

            g.fillAll(bg);

            auto n = stepCount();
            if (n == 0)
                return;

            auto bounds = getLocalBounds().toFloat();
            auto rowH = bounds.getHeight() / (float)n;
            auto midX = bounds.getCentreX();
            auto halfW = bounds.getWidth() * 0.5f;

            for (size_t i = 0; i < n; ++i)
            {
                float y = bounds.getY() + i * rowH;
                float v = std::clamp((float)stepD[i]->getValue(), -1.f, 1.f);
                float valX = midX + v * halfW;
                float left = std::min(valX, midX);
                float right = std::max(valX, midX);

                g.setColour(val);
                g.fillRect(juce::Rectangle<float>(left, y, right - left, rowH));

                constexpr float handleThickness = 2.0f;
                g.setColour(handleCol);
                g.fillRect(juce::Rectangle<float>(valX - handleThickness * 0.5f, y, handleThickness,
                                                  rowH));
            }

            g.setColour(handleCol.withAlpha(0.3f));
            for (size_t i = 1; i < n; ++i)
            {
                float y = bounds.getY() + i * rowH;
                g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());
            }
        }

        size_t stepCount() const { return (int)std::round(patch.lfoStepCount.value); }

        int draggedRow{-1};
        int menuContinuousIndex{-1};

        float rowHeight() const { return getLocalBounds().getHeight() / (float)stepCount(); }
        void mouseDown(const juce::MouseEvent &event) override
        {
            if (!isEnabled())
                return;

            auto n = stepCount();
            if (n == 0)
                return;

            auto rowH = rowHeight();
            auto rowIdx = (int)std::floor((event.position.y) / rowH);
            if (rowIdx < 0 || rowIdx >= (int)n)
                return;

            if (event.mods.isPopupMenu())
            {
                draggedRow = rowIdx;
                menuContinuousIndex = rowIdx;
                sst::jucegui::component_adapters::setClapParamId(this,
                                                                 patch.lfoSeqSteps[rowIdx].meta.id);
                editor.popupMenuForContinuous(this);
                return;
            }

            auto x01 = std::clamp((event.position.x) / getWidth(), 0.f, 1.f);

            editGuard(rowIdx, true);
            stepD[rowIdx]->setValueFromGUI(x01 * 2.f - 1.f);
            draggedRow = rowIdx;

            editor.updateTooltip(stepD[rowIdx].get());
            editor.showTooltipOn(this);
            repaint();
        }

        void mouseDrag(const juce::MouseEvent &event) override
        {
            if (!isEnabled())
                return;

            auto n = stepCount();
            if (n == 0)
                return;

            auto rowH = rowHeight();
            auto rowIdx = (int)std::floor((event.position.y) / rowH);
            if (rowIdx >= 0 && rowIdx < (int)n)
            {
                if (rowIdx != draggedRow)
                {
                    if (draggedRow >= 0)
                    {
                        editGuard(draggedRow, false);
                    }
                    editGuard(rowIdx, true);
                    draggedRow = rowIdx;
                }
                auto x01 = std::clamp((event.position.x) / getWidth(), 0.f, 1.f);

                stepD[draggedRow]->setValueFromGUI(x01 * 2.f - 1.f);
                editor.updateTooltip(stepD[draggedRow].get());
                repaint();
            }
        }

        void mouseUp(const juce::MouseEvent &event) override
        {
            if (!isEnabled())
                return;

            if (!event.mods.isPopupMenu())
            {
                editGuard(draggedRow, false);
                draggedRow = -1;
            }

            editor.hideTooltip();
        }

        void editGuard(int row, bool se)
        {
            auto id = patch.lfoSeqSteps[row].meta.id;
            if (se)
                editor.mainToAudio.push({Synth::MainToAudioMsg::Action::BEGIN_EDIT, id});
            else
                editor.mainToAudio.push({Synth::MainToAudioMsg::Action::END_EDIT, id});
        }
    };

    void setupLFO(SixSinesEditor &e, const Patch &v)
    {
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

        lfoTitleLab = std::make_unique<jcmp::RuledLabel>();
        lfoTitleLab->setText("LFO");
        c->addAndMakeVisible(*lfoTitleLab);

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

        stepEditor = std::make_unique<StepEditor>(e, v);
        c->addAndMakeVisible(*stepEditor);
        for (size_t i = 0; i < numSeqSteps; ++i)
        {
            e.componentByID[v.lfoSeqSteps[i].meta.id] =
                juce::Component::SafePointer<juce::Component>(stepEditor.get());
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

        rate->setEnabled(globallyEnabled);
        deform->setEnabled(globallyEnabled);
        phase->setEnabled(globallyEnabled);
        shape->setEnabled(globallyEnabled);
        tempoSync->setEnabled(globallyEnabled);
        bipolar->setEnabled(globallyEnabled && !isStep);
        isEnv->setEnabled(globallyEnabled);
        stepCount->setEnabled(globallyEnabled && isStep);
        cycleMode->setEnabled(globallyEnabled && isStep);
        stepEditor->setEnabled(globallyEnabled && isStep);
        asComp()->repaint();
    }

    juce::Rectangle<int> layoutLFOAt(int x, int y, int width = -1)
    {
        if (!lfoTitleLab)
            return {};

        namespace jlo = sst::jucegui::layouts;

        auto w = (width > 0) ? width : (asComp()->getWidth() - x - uicMargin);
        auto h = 180;

        auto lo = jlo::VList().at(x, y).withHeight(h).withWidth(w);

        lo.add(titleLabelLayout(lfoTitleLab));

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
        col3.add(jlo::Component(*stepEditor).expandToFill());
        col3.addGap(uicMargin);
        auto bottomRow = jlo::HList().withHeight(uicLabelHeight).withAutoGap(uicMargin);
        bottomRow.add(jlo::Component(*stepCount).withWidth(70));
        bottomRow.add(jlo::Component(*cycleMode).expandToFill());
        col3.add(bottomRow);
        col3.addGap(uicMargin);

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
    std::unique_ptr<jcmp::RuledLabel> lfoTitleLab;

    std::unique_ptr<jcmp::ToggleButton> tempoSync;
    std::unique_ptr<PatchDiscrete> tempoSyncD;

    std::unique_ptr<jcmp::ToggleButton> bipolar;
    std::unique_ptr<PatchDiscrete> bipolarD;

    std::unique_ptr<jcmp::ToggleButton> isEnv;
    std::unique_ptr<PatchDiscrete> isEnvD;

    std::unique_ptr<StepEditor> stepEditor;

    std::unique_ptr<jcmp::JogUpDownButton> stepCount;
    std::unique_ptr<PatchDiscrete> stepCountD;

    std::unique_ptr<jcmp::ToggleButton> cycleMode;
    std::unique_ptr<PatchDiscrete> cycleModeD;
};
} // namespace baconpaul::six_sines::ui
#endif // BACONPAUL_SIX_SINES_UI_LFO_COMPONENTS_H
