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

#include "sst/jucegui/components/ToggleButton.h"
#include "source-panel.h"
#include "source-sub-panel.h"
#include "segmented-ratio-editor.h"
#include "ui-constants.h"
#include "dsp/sintable.h"
#include "sst/jucegui/accessibility/KeyboardTraverser.h"
#include "knob-highlight.h"
#include <cmath>

namespace baconpaul::six_sines::ui
{
SourcePanel::SourcePanel(SixSinesEditor &e) : jcmp::NamedPanel("Source"), HasEditor(e)
{
    auto &mn = editor.patchCopy.sourceNodes;
    for (auto i = 0U; i < numOps; ++i)
    {
        createComponent(editor, *this, mn[i].ratio, knobs[i], knobsData[i], i);
        knobs[i]->setDrawLabel(false);
        addAndMakeVisible(*knobs[i]);

        createComponent(editor, *this, mn[i].active, power[i], powerData[i], i);
        power[i]->setDrawMode(sst::jucegui::components::ToggleButton::DrawMode::GLYPH);
        power[i]->setGlyph(sst::jucegui::components::GlyphPainter::POWER);
        addAndMakeVisible(*power[i]);
        powerData[i]->onGuiSetValue = [w = juce::Component::SafePointer(this)]()
        {
            if (w)
                w->optionalAllSoundsOffOnToggle();
        };

        labels[i] = std::make_unique<jcmp::Label>();
        labels[i]->setText("Op " + std::to_string(i + 1) + " Ratio");
        addAndMakeVisible(*labels[i]);

        upButton[i] = std::make_unique<jcmp::GlyphButton>(jcmp::GlyphPainter::GlyphType::PLUS);
        upButton[i]->setTitle("Increase Op " + std::to_string(i + 1) + " Ratio");
        upButton[i]->setOnCallback(
            [w = juce::Component::SafePointer(this), i]()
            {
                if (w)
                    w->adjustRatio(i, true);
            });
        addAndMakeVisible(*upButton[i]);
        downButton[i] = std::make_unique<jcmp::GlyphButton>(jcmp::GlyphPainter::GlyphType::MINUS);
        downButton[i]->setTitle("Decrease Op " + std::to_string(i + 1) + " Ratio");
        downButton[i]->setOnCallback(
            [w = juce::Component::SafePointer(this), i]()
            {
                if (w)
                    w->adjustRatio(i, false);
            });
        addAndMakeVisible(*downButton[i]);
        sst::jucegui::component_adapters::setTraversalId(power[i].get(), i * 5 + 20);
        sst::jucegui::component_adapters::setTraversalId(downButton[i].get(), i * 5 + 20);
        sst::jucegui::component_adapters::setTraversalId(upButton[i].get(), i * 5 + 20);
        sst::jucegui::component_adapters::setTraversalId(knobs[i].get(), i * 5 + 21);

        // Build the segmented editor eagerly. Both knob and segmented are bound to the
        // ratio param at all times; we only swap visibility when the editor type changes.
        createComponent(editor, *this, mn[i].ratio, segmentedEditors[i], segmentedEditorsData[i],
                        i);
        segmentedEditors[i]->finalizeSetup(editor);
        addChildComponent(*segmentedEditors[i]);

        // The second createComponent above clobbered componentByID[ratioId]; restore it
        // to the knob (the popup-menu/accessibility default), then register a refresh
        // hook that fans inbound updates out to BOTH widgets so neither goes stale.
        auto id = mn[i].ratio.meta.id;
        editor.componentByID[id] = juce::Component::SafePointer<juce::Component>(knobs[i].get());
        editor.componentRefreshByID[id] =
            [kw = juce::Component::SafePointer<juce::Component>(knobs[i].get()),
             sw = juce::Component::SafePointer<SegmentedRatioEditor>(segmentedEditors[i].get())]()
        {
            if (kw)
                kw->repaint();
            if (sw)
                sw->refreshFromExternal();
        };

        // When the knob writes the ratio, force the segmented editor to re-decompose
        // from the new float — PatchContinuous doesn't fire data listeners on its own.
        knobsData[i]->onGuiSetValue =
            [sw = juce::Component::SafePointer<SegmentedRatioEditor>(segmentedEditors[i].get())]()
        {
            if (sw)
                sw->refreshFromExternal();
        };

        // The segmented editor has its own PatchContinuous binding (built by the
        // second createComponent above). Right-click → popup-menu typein writes
        // through that binding, which also bypasses data listeners; reseed the
        // cached digits the same way.
        segmentedEditorsData[i]->onGuiSetValue =
            [sw = juce::Component::SafePointer<SegmentedRatioEditor>(segmentedEditors[i].get())]()
        {
            if (sw)
                sw->refreshFromExternal();
        };
    }

    highlight = std::make_unique<KnobHighlight>(editor);
    addChildComponent(*highlight);

    hasHamburger = true;
    onHamburger = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->showHamburgerMenu();
    };

    auto stored = editor.defaultsProvider->getUserDefaultValue(Defaults::sourceEditorType,
                                                               static_cast<int>(SourceEditor_Knob));
    // Sanitize: if the persisted value isn't one of the editor types we know about
    // (e.g. left over from an earlier build with extra enumerators), fall back to Knob.
    if (stored != SourceEditor_Knob && stored != SourceEditor_SegmentedDecimal)
        stored = SourceEditor_Knob;
    setEditorType(static_cast<SourceEditorType>(stored), false);
}
SourcePanel::~SourcePanel() = default;

void SourcePanel::resized()
{
    auto b = getContentArea().reduced(uicMargin, 0);
    auto x = b.getX();
    auto y = b.getY();
    for (auto i = 0U; i < numOps; ++i)
    {
        if (editorType == SourceEditor_Knob)
        {
            positionPowerKnobSwitchAndLabel(x, y, power[i], upButton[i], knobs[i], labels[i]);
            auto b = upButton[i]->getBounds().reduced(2, 0).translated(0, -2);
            auto ub = b.withTrimmedLeft(b.getWidth() / 2).withTrimmedBottom(uicMargin / 2);
            auto db = b.withTrimmedRight(b.getWidth() / 2).withTrimmedBottom(uicMargin / 2);
            upButton[i]->setBounds(ub);
            downButton[i]->setBounds(db);
        }
        else
        {
            // editor takes the full cell width above the label row;
            // power button sits in the label row on the left, label to its right
            auto cell = juce::Rectangle<int>(x, y, uicPowerKnobWidth, uicKnobSize);
            segmentedEditors[i]->setBounds(cell);

            auto labelRow = juce::Rectangle<int>(x, y + uicKnobSize + uicLabelGap,
                                                 uicPowerKnobWidth, uicLabelHeight);
            power[i]->setBounds(labelRow.withWidth(uicLabelHeight).reduced(2));
            labels[i]->setBounds(labelRow.withTrimmedLeft(uicLabelHeight + uicMargin));
        }
        x += uicPowerKnobWidth + uicMargin;
    }
}

void SourcePanel::setEditorType(SourceEditorType t, bool persist)
{
    editorType = t;

    for (auto i = 0U; i < numOps; ++i)
    {
        bool isKnob = (t == SourceEditor_Knob);
        bool isSeg = (t == SourceEditor_SegmentedDecimal);
        knobs[i]->setVisible(isKnob);
        upButton[i]->setVisible(isKnob);
        downButton[i]->setVisible(isKnob);
        segmentedEditors[i]->setVisible(isSeg);
        labels[i]->setText(isKnob ? ("Op " + std::to_string(i + 1) + " Ratio")
                                  : ("Op " + std::to_string(i + 1)));

        updateOpEnabledState(i);
    }

    if (persist)
        editor.defaultsProvider->updateUserDefaultValue(Defaults::sourceEditorType,
                                                        static_cast<int>(t));

    resized();
    repaint();
}

void SourcePanel::showHamburgerMenu()
{
    juce::PopupMenu p;
    p.addSectionHeader("Source Editor");
    p.addSeparator();
    auto add = [this, &p](const juce::String &name, SourceEditorType t)
    {
        p.addItem(name, true, editorType == t,
                  [w = juce::Component::SafePointer(this), t]()
                  {
                      if (w)
                          w->setEditorType(t, true);
                  });
    };
    add("Knob", SourceEditor_Knob);
    add("Segmented", SourceEditor_SegmentedDecimal);
    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&editor));
}

void SourcePanel::mouseDown(const juce::MouseEvent &e)
{
    if (e.mods.isPopupMenu())
    {
        editor.showNavigationMenu();
        return;
    }
    if (hasHamburger && getHamburgerRegion().contains(e.position.toInt()))
    {
        jcmp::NamedPanel::mouseDown(e);
        return;
    }
    for (int i = 0; i < numOps; ++i)
    {
        if (rectangleFor(i).contains(e.position.toInt()))
        {
            beginEdit(i);
        }
    }
}

juce::Rectangle<int> SourcePanel::rectangleFor(int idx)
{
    auto b = getContentArea().reduced(uicMargin, 0);
    return juce::Rectangle<int>(b.getX() + idx * (uicPowerKnobWidth + uicMargin), b.getY(),
                                uicPowerKnobWidth + 2, uicLabeledKnobHeight);
}

void SourcePanel::beginEdit(size_t idx)
{
    editor.hideAllSubPanels();
    editor.activateHamburger(true);
    editor.sourceSubPanel->setVisible(true);
    editor.sourceSubPanel->setSelectedIndex(idx);
    editor.singlePanel->setName("Op " + std::to_string(idx + 1) + " Source");

    highlight->setVisible(true);
    highlight->setBounds(rectangleFor(idx));
    highlight->toBack();
}

void SourcePanel::adjustRatio(int idx, bool up)
{
    auto rat = knobsData[idx]->getValue();

    double newValue{0};

    auto m = juce::ModifierKeys::getCurrentModifiers();
    if (m.isCommandDown())
    {
        double ttr{0.};
        if (up)
            ttr = std::floor(rat + 0.001);
        else
            ttr = std::ceil(rat - 0.001);
        newValue = std::clamp((float)(ttr + (up ? 1 : -1)), knobsData[idx]->getMin(),
                              knobsData[idx]->getMax());
    }
    else
    {
        // step by harmonics so need to step in log2 space
        float sideSign{1.0};
        if (rat < 0)
            sideSign = -1.0;

        rat *= sideSign;

        double ttr{0.};
        if ((up && sideSign > 0) || (!up && sideSign < 0))
            ttr = std::floor(pow(2.0, rat) + 0.00001);
        else
            ttr = std::ceil(pow(2.0, rat) - 0.00001);

        if (up)
        {
            ttr += sideSign * 1;
        }
        else
        {
            if (ttr == 1)
                ttr = sideSign < 0 ? 2 : 0.5;
            else
                ttr -= sideSign * 1;
        }
        newValue = std::clamp(sideSign * (float)std::log2(ttr), knobsData[idx]->getMin(),
                              knobsData[idx]->getMax());
    }

    knobs[idx]->onBeginEdit();
    knobsData[idx]->setValueFromGUI(newValue);
    knobs[idx]->onEndEdit();
    repaint();
}

void SourcePanel::updateOpEnabledState(int idx)
{
    if (idx < 0 || idx >= (int)numOps)
        return;
    auto &sn = editor.patchCopy.sourceNodes[idx];
    auto isAudioIn = ((int)std::round(sn.waveForm.value) == SinTable::WaveForm::AUDIO_IN);
    knobs[idx]->setEnabled(!isAudioIn);
    upButton[idx]->setEnabled(!isAudioIn);
    downButton[idx]->setEnabled(!isAudioIn);
    segmentedEditors[idx]->setEnabled(!isAudioIn);
}

} // namespace baconpaul::six_sines::ui