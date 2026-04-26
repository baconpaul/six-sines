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
        modTitleLab->setText("Other Modulation");
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
        namespace jlo = sst::jucegui::layouts;

        auto rowHeight = static_cast<int>(uicLabelHeight) + 4;
        auto nMods = static_cast<int>(numModsPer);
        auto totalHeight = static_cast<int>(uicTitleLabelHeight) + nMods * rowHeight +
                           (nMods - 1) * static_cast<int>(uicMargin);

        auto lo = jlo::VList()
                      .at(r.getX(), r.getBottom() - totalHeight)
                      .withWidth(r.getWidth())
                      .withHeight(totalHeight);

        lo.add(titleLabelLayout(modTitleLab));

        for (int i = 0; i < numModsPer; ++i)
        {
            auto hl = jlo::HList().withHeight(rowHeight).withAutoGap(uicMargin);
            hl.add(jlo::Component(*sourceMenu[i]).expandToFill());
            hl.add(jlo::Component(*targetMenu[i]).expandToFill());
            hl.add(jlo::Component(*depthSlider[i]).expandToFill().insetBy(0, uicMargin));
            lo.add(hl);
            if (i < numModsPer - 1)
                lo.addGap(uicMargin);
        }

        lo.doLayout();
    }

    void showTargetMenu(int index)
    {
        if (!patchPtr)
            return;

        auto p = juce::PopupMenu();
        p.addSectionHeader("Modulation Target");
        p.addSeparator();
        auto current = static_cast<int32_t>(std::round(patchPtr->modtarget[index].value));
        for (const auto &[id, nm] : patchPtr->targetList)
        {
            if (id == -1)
            {
                p.addSeparator();
            }
            else
            {
                auto ticked = (id == current);
                auto enabled = isTargetAvailable(static_cast<typename Patch::TargetID>(id));
                p.addItem(nm, enabled, ticked,
                          [si = id, w = juce::Component::SafePointer(asComp()), index]()
                          {
                              if (!w)
                                  return;
                              if (!w->patchPtr)
                                  return;
                              w->editor.setAndSendParamValue(w->patchPtr->modtarget[index], si);
                              w->resetTargetLabel(index);
                          });
            }
        }
        p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&asComp()->editor),
                        makeMenuAccessibleButtonCB(targetMenu[index].get()));
    }
    void showSourceMenu(int index)
    {
        auto p = juce::PopupMenu();
        p.addSectionHeader("Modulation Source");
        p.addSeparator();
        std::string currCat{};
        auto s = juce::PopupMenu();
        auto genSet = [c = asComp(), idx = index](int si)
        {
            if (debugLevel > 0)
                SXSNLOG("GenSet with " << si << " at " << idx);
            if (si == 2048)
            {
                SXSNLOG("ERROR: GENSET with SI=2048");
            }
            return [sCopy = si, w = juce::Component::SafePointer(c), lidx = idx]()
            {
                if (!w)
                    return;
                if (!w->patchPtr)
                    return;
                if (debugLevel > 0)
                    SXSNLOG("GenSet action at " << lidx << " with " << sCopy);
                if (sCopy == 2048)
                    SXSNLOG("ERROR: GENSET with sCopy=2048 " << lidx);
                w->editor.setAndSendParamValue(w->patchPtr->modsource[lidx], sCopy);
                w->resetSourceLabel(lidx);
            };
        };

        auto currentSource = static_cast<int32_t>(std::round(patchPtr->modsource[index].value));
        for (const auto &so : asComp()->editor.modMatrixConfig.sources)
        {
            auto dname = so.name;
            auto id = so.id;
            if (debugLevel > 0)
                dname += " (" + std::to_string(id) + ")";

            auto ticked = (static_cast<int32_t>(id) == currentSource);

            if (so.group.empty())
            {
                p.addItem(dname, true, ticked, genSet(id));
            }
            else
            {
                if (so.group != currCat && s.getNumItems() > 0)
                {
                    p.addSubMenu(currCat, s);
                    s = juce::PopupMenu();
                }
                currCat = so.group;
                s.addItem(dname, true, ticked, genSet(id));
            }
        }
        if (s.getNumItems() > 0)
        {
            p.addSubMenu(currCat, s);
        }
        p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&asComp()->editor),
                        makeMenuAccessibleButtonCB(sourceMenu[index].get()));
    }

    std::unique_ptr<jcmp::RuledLabel> modTitleLab;

    std::array<std::unique_ptr<jcmp::MenuButton>, numModsPer> sourceMenu;
    std::array<std::unique_ptr<jcmp::MenuButton>, numModsPer> targetMenu;
    std::array<std::unique_ptr<jcmp::HSliderFilled>, numModsPer> depthSlider;
    std::array<std::unique_ptr<PatchContinuous>, numModsPer> depthSliderD;

    // Override per-panel to grey out targets that don't apply to the current node state
    // (e.g. EXTEND_M only available in PHASE_REMAP). Sources are always enabled.
    std::function<bool(typename Patch::TargetID)> isTargetAvailable{[](auto) { return true; }};
};
} // namespace baconpaul::six_sines::ui
#endif // MODULATION_COMPONENTS_H
