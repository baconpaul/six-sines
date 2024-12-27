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

#ifndef BACONPAUL_SIX_SINES_UI_MODULATION_COMPONENTS_H
#define BACONPAUL_SIX_SINES_UI_MODULATION_COMPONENTS_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/HSliderFilled.h>
#include "patch-data-bindings.h"
#include "ui-constants.h"
#include "ruled-label.h"

namespace baconpaul::six_sines::ui
{
namespace jcmp = sst::jucegui::components;
template <typename Comp, typename Patch> struct ModulationComponents
{
    Comp *asComp() { return static_cast<Comp *>(this); }
    ModulationComponents() {}
    void setupModulation(SixSinesEditor &e, const Patch &v)
    {
        auto c = asComp();
        modTitleLab = std::make_unique<RuledLabel>();
        modTitleLab->setText("Modulation");
        asComp()->addAndMakeVisible(*modTitleLab);

        for (int i = 0; i < numModsPer; ++i)
        {
            createComponent(e, *c, v.moddepth[i], depthSlider[i], depthSliderD[i]);
            sourceMenu[i] = std::make_unique<jcmp::MenuButton>();
            sourceMenu[i]->setLabel("Src " + std::to_string(i));
            targetMenu[i] = std::make_unique<jcmp::MenuButton>();
            targetMenu[i]->setLabel("Tgt " + std::to_string(i));

            c->addAndMakeVisible(*depthSlider[i]);
            c->addAndMakeVisible(*sourceMenu[i]);
            c->addAndMakeVisible(*targetMenu[i]);
        }
    }

    void layoutModulation(const juce::Rectangle<int> &r)
    {
        auto modW{150}; // TODO MOVE TO UIC
        auto bx = r.withTrimmedLeft(r.getWidth() - modW);
        modTitleLab->setBounds(bx.withHeight(uicTitleLabelHeight));
        auto bq = bx.translated(0, uicTitleLabelHeight + uicMargin)
                      .withHeight(uicLabelHeight + uicMargin);

        auto mks = 2 * uicLabelHeight + 2 * uicMargin;

        for (int i = 0; i < numModsPer; ++i)
        {
            auto kb = bq.withTrimmedLeft(bq.getWidth() - mks).withHeight(mks);
            depthSlider[i]->setBounds(kb);

            sourceMenu[i]->setBounds(bq.withTrimmedRight(mks));
            bq = bq.translated(0, uicLabelHeight + uicMargin);
            targetMenu[i]->setBounds(bq.withTrimmedRight(mks));
            bq = bq.translated(0, uicLabelHeight + 3 * uicMargin);
        }
    }

    std::unique_ptr<RuledLabel> modTitleLab;

    std::array<std::unique_ptr<jcmp::MenuButton>, numModsPer> sourceMenu;
    std::array<std::unique_ptr<jcmp::MenuButton>, numModsPer> targetMenu;
    std::array<std::unique_ptr<jcmp::Knob>, numModsPer> depthSlider;
    std::array<std::unique_ptr<PatchContinuous>, numModsPer> depthSliderD;
};
} // namespace baconpaul::six_sines::ui
#endif // MODULATION_COMPONENTS_H
