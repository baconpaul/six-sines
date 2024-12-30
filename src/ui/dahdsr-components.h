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
#include "ruled-label.h"

namespace baconpaul::six_sines::ui
{
namespace jcmp = sst::jucegui::components;
template <typename Comp, typename PatchPart> struct DAHDSRComponents
{
    bool voiceTrigerAllowed{true};
    Comp *asComp() { return static_cast<Comp *>(this); }
    DAHDSRComponents() {}

    const Param *triggerModePtr{nullptr}; // bit of a hack since refs arent mutable
    void setupDAHDSR(SixSinesEditor &e, const PatchPart &v)
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
        triggerModePtr = &v.triggerMode;
        setTriggerLabel();
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
            if (i == 0)
                triggerButton->setBounds(bx.withHeight(uicSliderWidth).reduced(2));
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

    std::unique_ptr<jcmp::TextPushButton> triggerButton;
    void setTriggerLabel()
    {
        if (!triggerModePtr)
            return;
        auto tmv = (int)std::round(triggerModePtr->value);
        switch (tmv)
        {
        case (int)TriggerMode::NEW_GATE:
            triggerButton->setLabel("G");
            break;
        case (int)TriggerMode::NEW_VOICE:
            triggerButton->setLabel("V");
            break;
        case (int)TriggerMode::KEY_PRESS:
            triggerButton->setLabel("K");
            break;
        case (int)TriggerMode::ON_RELEASE:
            triggerButton->setLabel("R");
            break;
        case (int)TriggerMode::PATCH_DEFAULT:
            triggerButton->setLabel("D");
            break;
        }
        triggerButton->repaint();
    }
    void showTriggerPopup()
    {
        if (!triggerModePtr)
            return;
        auto tmv = (int)std::round(triggerModePtr->value);

        auto genSet = [w = juce::Component::SafePointer(asComp())](int nv)
        {
            auto that = w;
            return [nv, that]()
            {
                auto pid = that->triggerModePtr->meta.id;
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
            if (v == (int)TriggerMode::PATCH_DEFAULT)
                p.addSeparator();
            p.addItem(TriggerModeName[v], enabled, tmv == v, genSet(v));
        }

        p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&asComp()->editor));
    }
};
} // namespace baconpaul::six_sines::ui
#endif // DAHDSR_COMPONENTS_H
