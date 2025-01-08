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

#ifndef BACONPAUL_SIX_SINES_UI_MODULATION_COMPONENTS_H
#define BACONPAUL_SIX_SINES_UI_MODULATION_COMPONENTS_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/HSliderFilled.h>
#include "patch-data-bindings.h"
#include "ui-constants.h"
#include "sst/jucegui/components/RuledLabel.h"

namespace baconpaul::six_sines::ui
{
namespace jcmp = sst::jucegui::components;
template <typename Comp, typename Patch> struct ModulationComponents
{
    Comp *asComp() { return static_cast<Comp *>(this); }
    ModulationComponents() {}

    Patch *patchPtr{nullptr};
    void setupModulation(SixSinesEditor &e, Patch &v)
    {
        using kt_t = sst::jucegui::accessibility::KeyboardTraverser;

        patchPtr = &v;
        auto c = asComp();
        modTitleLab = std::make_unique<jcmp::RuledLabel>();
        modTitleLab->setText("Modulation");
        asComp()->addAndMakeVisible(*modTitleLab);

        for (int i = 0; i < numModsPer; ++i)
        {
            createComponent(e, *c, v.moddepth[i], depthSlider[i], depthSliderD[i]);
            sourceMenu[i] = std::make_unique<jcmp::MenuButton>();
            resetSourceLabel(i);
            sourceMenu[i]->setOnCallback(
                [i, w = juce::Component::SafePointer(asComp())]()
                {
                    if (!w)
                        return;
                    w->showSourceMenu(i);
                });
            e.componentRefreshByID[v.modsource[i].meta.id] =
                [i, w = juce::Component::SafePointer(c)]()
            {
                if (w)
                {
                    w->resetSourceLabel(i);
                }
            };
            targetMenu[i] = std::make_unique<jcmp::MenuButton>();
            resetTargetLabel(i);
            targetMenu[i]->setOnCallback(
                [i, w = juce::Component::SafePointer(asComp())]()
                {
                    if (!w)
                        return;
                    w->showTargetMenu(i);
                });
            e.componentRefreshByID[v.modtarget[i].meta.id] =
                [i, w = juce::Component::SafePointer(c)]()
            {
                if (w)
                {
                    w->resetTargetLabel(i);
                }
            };

            c->addAndMakeVisible(*depthSlider[i]);
            c->addAndMakeVisible(*sourceMenu[i]);
            c->addAndMakeVisible(*targetMenu[i]);

            sst::jucegui::component_adapters::setTraversalId(sourceMenu[i].get(), i * 5 + 900);
            sst::jucegui::component_adapters::setTraversalId(targetMenu[i].get(), i * 5 + 901);
            sst::jucegui::component_adapters::setTraversalId(depthSlider[i].get(), i * 5 + 902);
        }
    }

    void resetSourceLabel(int i)
    {
        if (!patchPtr)
        {
            sourceMenu[i]->setLabel("");
            return;
        }
        auto n = patchPtr->name();
        auto v = (uint32_t)std::round(patchPtr->modsource[i].value);

        auto p = asComp()->editor.modMatrixConfig.sourceByID.find(v);
        if (p != asComp()->editor.modMatrixConfig.sourceByID.end())
        {
            sourceMenu[i]->setLabelAndTitle(
                p->second.name, n + " Mod Source " + std::to_string(i + 1) + " " + p->second.name);
        }
        else
        {
            sourceMenu[i]->setLabel("UNK " + std::to_string(v));
        }
    }

    void resetTargetLabel(int i)
    {
        if (!patchPtr)
        {
            targetMenu[i]->setLabel("");
            return;
        }
        std::string res{"ERR"};
        auto v = (uint32_t)std::round(patchPtr->modtarget[i].value);
        auto n = patchPtr->name();
        for (auto &[id, nm] : patchPtr->targetList)
        {
            if (id == v)
            {
                res = nm;
            }
        }
        targetMenu[i]->setLabelAndTitle(res,
                                        n + " Mod Target " + std::to_string(i + 1) + " " + res);
    }

    void layoutModulation(const juce::Rectangle<int> &r)
    {
        auto modW{150}; // TODO MOVE TO UIC
        auto bx = r.withTrimmedLeft(r.getWidth() - modW);
        positionTitleLabelAt(bx.getX(), bx.getY(), modW, modTitleLab);
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

    void showTargetMenu(int index)
    {
        if (!patchPtr)
            return;

        auto p = juce::PopupMenu();
        p.addSectionHeader("Modulation Target");
        p.addSeparator();
        for (const auto &[id, nm] : patchPtr->targetList)
        {
            if (id == -1)
            {
                p.addSeparator();
            }
            else
            {
                p.addItem(nm,
                          [si = id, w = juce::Component::SafePointer(asComp()), index]()
                          {
                              if (!w)
                                  return;
                              if (!w->patchPtr)
                                  return;
                              w->patchPtr->modtarget[index].value = si;
                              w->editor.uiToAudio.push({Synth::UIToAudioMsg::SET_PARAM,
                                                        w->patchPtr->modtarget[index].meta.id,
                                                        (float)si});
                              w->resetTargetLabel(index);
                          });
            }
        }
        p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&asComp()->editor));
    }
    void showSourceMenu(int index)
    {
        auto p = juce::PopupMenu();
        p.addSectionHeader("Modulation Source");
        p.addSeparator();
        std::string currCat{};
        auto s = juce::PopupMenu();
        auto genSet = [&](auto si)
        {
            return [si, w = juce::Component::SafePointer(asComp()), index]()
            {
                if (!w)
                    return;
                if (!w->patchPtr)
                    return;
                w->patchPtr->modsource[index].value = si;
                w->editor.uiToAudio.push({Synth::UIToAudioMsg::SET_PARAM,
                                          w->patchPtr->modsource[index].meta.id, (float)si});
                w->resetSourceLabel(index);
            };
        };

        for (const auto &so : asComp()->editor.modMatrixConfig.sources)
        {
            if (so.group.empty())
            {
                p.addItem(so.name, genSet(so.id));
            }
            else
            {
                if (so.group != currCat && s.getNumItems() > 0)
                {
                    p.addSubMenu(currCat, s);
                    s = juce::PopupMenu();
                }
                currCat = so.group;
                s.addItem(so.name, genSet(so.id));
            }
        }
        if (s.getNumItems() > 0)
        {
            p.addSubMenu(currCat, s);
        }
        p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&asComp()->editor));
    }

    std::unique_ptr<jcmp::RuledLabel> modTitleLab;

    std::array<std::unique_ptr<jcmp::MenuButton>, numModsPer> sourceMenu;
    std::array<std::unique_ptr<jcmp::MenuButton>, numModsPer> targetMenu;
    std::array<std::unique_ptr<jcmp::Knob>, numModsPer> depthSlider;
    std::array<std::unique_ptr<PatchContinuous>, numModsPer> depthSliderD;
};
} // namespace baconpaul::six_sines::ui
#endif // MODULATION_COMPONENTS_H
