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

#ifndef BACONPAUL_SIX_SINES_UI_DAHDSR_COMPONENTS_H
#define BACONPAUL_SIX_SINES_UI_DAHDSR_COMPONENTS_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/components/VSlider.h>
#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/TextPushButton.h>
#include "patch-data-bindings.h"
#include "ui-constants.h"
#include "sst/jucegui/components/RuledLabel.h"

#include "sst/jucegui/layouts/ListLayout.h"

namespace baconpaul::six_sines::ui
{
namespace jcmp = sst::jucegui::components;
template <typename Comp, typename PatchPart> struct DAHDSRComponents
{
    bool voiceTrigerAllowed{true};
    Comp *asComp() { return static_cast<Comp *>(this); }
    DAHDSRComponents() {}

    const PatchPart *patchPartPtr{nullptr}; // bit of a hack since refs arent mutable
    void setupDAHDSR(SixSinesEditor &e, const PatchPart &v)
    {
        using kt_t = sst::jucegui::accessibility::KeyboardTraverser;

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

        titleLab = std::make_unique<jcmp::RuledLabel>();
        titleLab->setText("Envelope");
        asComp()->addAndMakeVisible(*titleLab);

        triggerButton = std::make_unique<jcmp::TextPushButton>();
        triggerButton->setOnCallback(
            [w = juce::Component::SafePointer(asComp())]()
            {
                if (!w)
                    return;
                w->showTriggerPopup();
            });
        asComp()->addAndMakeVisible(*triggerButton);
        e.componentRefreshByID[v.triggerMode.meta.id] =
            [w = juce::Component::SafePointer(asComp())]()
        {
            if (w)
                w->setTriggerLabel();
        };
        patchPartPtr = &v;
        setTriggerLabel();

        sst::jucegui::component_adapters::setTraversalId(triggerButton.get(), 40);
        for (int i = 0; i < nels; ++i)
            sst::jucegui::component_adapters::setTraversalId(slider[i].get(), 10 + i);
        for (int i = 0; i < 3; ++i)
            sst::jucegui::component_adapters::setTraversalId(shapes[i].get(), 20 + i);
    }

    juce::Rectangle<int> layoutDAHDSRAt(int x, int y)
    {
        if (!titleLab || !slider[0])
            return {};

        namespace jlo = sst::jucegui::layouts;

        auto lo = jlo::VList()
                      .at(x, y)
                      .withHeight(asComp()->getHeight() - y)
                      .withWidth(nels * uicSliderWidth);

        lo.add(titleLabelLayout(titleLab));
        auto sliders = jlo::HList().expandToFill();

        for (int i = 0; i < nels; ++i)
        {
            auto sliderStack = jlo::VList().withWidth(uicSliderWidth);

            if (i == 0)
                sliderStack.add(
                    jlo::Component(*triggerButton).withHeight(uicSliderWidth).insetBy(2));
            else if (i % 2)
                sliderStack.add(jlo::Component(*shapes[i / 2]).withHeight(uicSliderWidth));
            else
                sliderStack.addGap(uicSliderWidth);

            sliderStack.add(jlo::Component(*slider[i]).expandToFill());

            sliderStack.addGap(uicLabelGap);
            sliderStack.add(jlo::Component(*lab[i]).withHeight(uicLabelHeight));

            sliders.add(sliderStack);
        }

        lo.add(sliders);
        auto usedSpace = lo.doLayout();
        return usedSpace.translated(usedSpace.getWidth(), 0);
    }

    static constexpr int nels{6};   // dahdsr
    static constexpr int nShape{3}; // no shape for delay jp;d or release
    std::array<std::unique_ptr<jcmp::VSlider>, nels> slider;
    std::array<std::unique_ptr<PatchContinuous>, nels> sliderD;
    std::array<std::unique_ptr<jcmp::Knob>, nShape> shapes;
    std::array<std::unique_ptr<PatchContinuous>, nShape> shapesD;
    std::array<std::unique_ptr<jcmp::Label>, nels> lab;
    std::unique_ptr<jcmp::RuledLabel> titleLab;

    std::unique_ptr<jcmp::TextPushButton> triggerButton;
    void setTriggerLabel()
    {
        if (!patchPartPtr)
            return;
        auto tmv = (int)std::round(patchPartPtr->triggerMode.value);
        auto osv = (int)std::round(patchPartPtr->envIsOneShot.value);

        std::string LE = (osv ? u8"\U000000B9" : "");
        std::string LEacc = (osv ? " (one shot)" : "");
        std::string nm = "";
        if (patchPartPtr)
            nm = patchPartPtr->name();

        switch (tmv)
        {
        case (int)TriggerMode::NEW_GATE:
            triggerButton->setLabelAndTitle("L" + LE, nm + " Env Trigger Legato" + LEacc);
            break;
        case (int)TriggerMode::NEW_VOICE:
            triggerButton->setLabelAndTitle("S" + LE, nm + " Env Trigger on Voice" + LEacc);
            break;
        case (int)TriggerMode::KEY_PRESS:
            triggerButton->setLabelAndTitle("K" + LE, nm + " Env Trigger on Key" + LEacc);
            break;
        case (int)TriggerMode::ON_RELEASE:
            triggerButton->setLabelAndTitle("R", nm + " Env Trigger on Release" +
                                                     LEacc); // one shot does nothing with release
            break;
        case (int)TriggerMode::PATCH_DEFAULT:
            triggerButton->setLabelAndTitle("D" + LE, nm + " Env Patch Default Trigger" + LEacc);
            break;
        }
        triggerButton->repaint();
    }
    void showTriggerPopup()
    {
        if (!patchPartPtr)
            return;
        auto tmv = (int)std::round(patchPartPtr->triggerMode.value);
        auto osv = (bool)std::round(patchPartPtr->envIsOneShot.value);
        auto tfz = (bool)std::round(patchPartPtr->envTriggersFromZero.value);

        auto genSet = [w = juce::Component::SafePointer(asComp())](int nv)
        {
            auto that = w;
            return [nv, that]()
            {
                auto pid = that->patchPartPtr->triggerMode.meta.id;
                that->editor.patchCopy.paramMap.at(pid)->value = nv;
                that->setTriggerLabel();

                that->editor.uiToAudio.push(
                    {Synth::UIToAudioMsg::Action::SET_PARAM, pid, (float)nv});
                that->editor.flushOperator();
            };
        };
        auto p = juce::PopupMenu();
        p.addSectionHeader("Trigger Mode");
        p.addSeparator();
        for (auto v :
             {(int)TriggerMode::KEY_PRESS, (int)TriggerMode::NEW_GATE, (int)TriggerMode::NEW_VOICE,
              (int)TriggerMode::ON_RELEASE, (int)TriggerMode::PATCH_DEFAULT})
        {
            bool enabled = true;
            if (v == (int)TriggerMode::NEW_VOICE && !voiceTrigerAllowed)
                enabled = false;
            if (v == (int)TriggerMode::ON_RELEASE && !voiceTrigerAllowed)
                enabled = false;
            if (v == (int)TriggerMode::PATCH_DEFAULT)
                p.addSeparator();
            p.addItem(TriggerModeName[v], enabled, tmv == v, genSet(v));
        }

        p.addSeparator();
        p.addItem("One Shot", tmv != (int)TriggerMode::ON_RELEASE, osv,
                  [osv, w = juce::Component::SafePointer(asComp())]()
                  {
                      if (!w)
                          return;

                      auto pid = w->patchPartPtr->envIsOneShot.meta.id;
                      w->editor.patchCopy.paramMap.at(pid)->value = !(osv);
                      w->setTriggerLabel();

                      w->editor.uiToAudio.push(
                          {Synth::UIToAudioMsg::Action::SET_PARAM, pid, (float)(!osv)});
                      w->editor.flushOperator();
                  });
        p.addItem("Retrigger from Zero", true, tfz,
                  [tfz, w = juce::Component::SafePointer(asComp())]()
                  {
                      if (!w)
                          return;

                      auto pid = w->patchPartPtr->envTriggersFromZero.meta.id;
                      w->editor.patchCopy.paramMap.at(pid)->value = !(tfz);
                      w->setTriggerLabel();

                      w->editor.uiToAudio.push(
                          {Synth::UIToAudioMsg::Action::SET_PARAM, pid, (float)(!tfz)});
                      w->editor.flushOperator();
                  });
        p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&asComp()->editor));
    }
};
} // namespace baconpaul::six_sines::ui
#endif // DAHDSR_COMPONENTS_H
