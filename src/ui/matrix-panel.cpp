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

#include "matrix-panel.h"
#include "matrix-sub-panel.h"
#include "self-sub-panel.h"
#include "ui-constants.h"
#include "knob-highlight.h"

namespace baconpaul::six_sines::ui
{
MatrixPanel::MatrixPanel(SixSinesEditor &e) : jcmp::NamedPanel("Matrix"), HasEditor(e)
{
    using kt_t = sst::jucegui::accessibility::KeyboardTraverser;

    auto &mn = editor.patchCopy.selfNodes;
    for (auto i = 0U; i < numOps; ++i)
    {
        createRescaledComponent(editor, *this, mn[i].fbLevel, Sknobs[i], SknobsData[i], i, true);
        Sknobs[i]->setDrawLabel(false);
        addAndMakeVisible(*Sknobs[i]);

        createComponent(editor, *this, mn[i].active, Spower[i], SpowerData[i], i, true);
        Spower[i]->setDrawMode(sst::jucegui::components::ToggleButton::DrawMode::GLYPH);
        Spower[i]->setGlyph(sst::jucegui::components::GlyphPainter::POWER);
        addAndMakeVisible(*Spower[i]);

        Slabels[i] = std::make_unique<jcmp::Label>();
        Slabels[i]->setText("Op " + std::to_string(i + 1) + " " + u8"\U000021A9");
        addAndMakeVisible(*Slabels[i]);

        SknobsData[i]->under->onPullFromMin = [i, w = juce::Component::SafePointer(this)]()
        {
            if (!w)
                return;
            w->editor.setParamValueOnCopy(w->editor.patchCopy.selfNodes[i].active.meta.id, true,
                                          true);
            w->editor.setParamValueOnCopy(w->editor.patchCopy.sourceNodes[i].active.meta.id, true,
                                          true);
            w->repaint();
        };
        kt_t::assignTraversalIndex(Spower[i].get(), i * 50 + 47);
        kt_t::assignTraversalIndex(Sknobs[i].get(), i * 50 + 48);
    }

    auto &mx = editor.patchCopy.matrixNodes;
    for (auto i = 0U; i < matrixSize; ++i)
    {
        createRescaledComponent(editor, *this, mx[i].level, Mknobs[i], MknobsData[i], i, false);
        Mknobs[i]->setDrawLabel(false);
        addAndMakeVisible(*Mknobs[i]);

        createComponent(editor, *this, mx[i].active, Mpower[i], MpowerData[i], i, false);
        Mpower[i]->setDrawMode(sst::jucegui::components::ToggleButton::DrawMode::GLYPH);
        Mpower[i]->setGlyph(sst::jucegui::components::GlyphPainter::POWER);
        addAndMakeVisible(*Mpower[i]);

        createComponent(editor, *this, mx[i].pmOrRM, Mpmrm[i], MpmrmD[i], i, false);
        Mpmrm[i]->direction = sst::jucegui::components::MultiSwitch::HORIZONTAL;
        addAndMakeVisible(*Mpmrm[i]);

        auto si = MatrixIndex::sourceIndexAt(i);
        auto ti = MatrixIndex::targetIndexAt(i);
        std::string glyph = "+";
        if (si == ti - 1)
            glyph = u8"\U00002192";
        Mlabels[i] = std::make_unique<jcmp::Label>();
        Mlabels[i]->setText("Op " + std::to_string(si + 1) + " " + glyph);
        addAndMakeVisible(*Mlabels[i]);

        MknobsData[i]->under->onPullFromMin = [i, si, w = juce::Component::SafePointer(this)]()
        {
            if (!w)
                return;
            w->editor.setParamValueOnCopy(w->editor.patchCopy.matrixNodes[i].active.meta.id, true,
                                          true);
            w->editor.setParamValueOnCopy(w->editor.patchCopy.sourceNodes[si].active.meta.id, true,
                                          true);
            w->repaint();
        };

        kt_t::assignTraversalIndex(Mpower[i].get(), ti * 50 + si * 5 + 1);
        kt_t::assignTraversalIndex(Mpmrm[i].get(), ti * 50 + +si * 5 + 2);
        kt_t::assignTraversalIndex(Mknobs[i].get(), ti * 50 + si * 5 + 3);
    }

    highlight = std::make_unique<KnobHighlight>();
    addChildComponent(*highlight);
}
MatrixPanel::~MatrixPanel() = default;

void MatrixPanel::resized()
{
    auto b = getContentArea().reduced(uicMargin, 0);
    auto x = b.getX();
    auto y = b.getY();
    for (auto i = 0U; i < numOps; ++i)
    {
        positionPowerKnobAndLabel(x, y, Spower[i], Sknobs[i], Slabels[i], false);
        y += uicLabeledKnobHeight + uicMargin;
        x += uicPowerKnobWidth + uicMargin;
    }

    for (auto i = 0U; i < matrixSize; ++i)
    {
        auto si = MatrixIndex::sourceIndexAt(i);
        auto ti = MatrixIndex::targetIndexAt(i);
        auto y = b.getY() + ti * (uicLabeledKnobHeight + uicMargin);
        auto x = b.getX() + si * (uicPowerKnobWidth + uicMargin);
        positionPowerKnobSwitchAndLabel(x, y, Mpower[i], Mpmrm[i], Mknobs[i], Mlabels[i]);
    }
}

void MatrixPanel::paint(juce::Graphics &g)
{
    jcmp::NamedPanel::paint(g);
    auto b = getContentArea().reduced(0, 0);
    auto x = b.getX();
    auto y = b.getY();
    auto fillCol = getColour(Styles::background).brighter(0.1).withAlpha(0.35f);
    auto strokeCol = getColour(Styles::background).brighter(0.2);
    auto r = juce::Rectangle<int>(x + uicMargin, y, numOps * (uicPowerKnobWidth + uicMargin),
                                  uicLabeledKnobHeight)
                 .reduced(1);
    for (auto i = 0U; i < numOps; ++i)
    {
        if (i % 2 == 1)
        {
            g.setColour(fillCol);
            g.fillRoundedRectangle(r.toFloat(), 4);
            g.setColour(strokeCol);
            // g.drawRoundedRectangle(r.toFloat(), 4, 1);
        }
        r = r.translated(0, uicLabeledKnobHeight + uicMargin);
    }
    r = juce::Rectangle<int>(x + uicMargin, y, uicPowerKnobWidth,
                             numOps * (uicLabeledKnobHeight + uicMargin) - uicMargin);
    for (auto i = 0U; i < numOps; ++i)
    {
        if (i % 2 == 1)
        {
            g.setColour(fillCol);
            g.fillRoundedRectangle(r.toFloat(), 4);
            g.setColour(strokeCol);
            // g.drawRoundedRectangle(r.toFloat(), 4, 1);
        }
        r = r.translated(uicPowerKnobWidth + uicMargin, 0);
    }
}

void MatrixPanel::mouseDown(const juce::MouseEvent &e)
{
    if (e.mods.isPopupMenu())
    {
        editor.showNavigationMenu();
        return;
    }

    for (int i = 0; i < numOps; ++i)
    {
        if (rectangleFor(i, true).contains(e.position.toInt()))
        {
            beginEdit(i, true);
            return;
        }
    }

    for (int i = 0; i < matrixSize; ++i)
    {
        if (rectangleFor(i, false).contains(e.position.toInt()))
        {
            beginEdit(i, false);
            return;
        }
    }
}

juce::Rectangle<int> MatrixPanel::rectangleFor(int idx, bool self)
{
    auto b = getContentArea().reduced(uicMargin, 0);

    if (self)
    {
        return juce::Rectangle<int>(b.getX() + idx * (uicPowerKnobWidth + uicMargin),
                                    b.getY() + idx * (uicLabeledKnobHeight + uicMargin),
                                    uicPowerKnobWidth + 2, uicLabeledKnobHeight);
    }
    else
    {
        auto si = MatrixIndex::sourceIndexAt(idx);
        auto ti = MatrixIndex::targetIndexAt(idx);
        auto y = b.getY() + ti * (uicLabeledKnobHeight + uicMargin);
        auto x = b.getX() + si * (uicPowerKnobWidth + uicMargin);
        return juce::Rectangle<int>(x, y, uicPowerKnobWidth + 2, uicLabeledKnobHeight);
    }
}

void MatrixPanel::beginEdit(size_t idx, bool self)
{
    editor.hideAllSubPanels();

    if (self)
    {
        editor.singlePanel->setName("Op " + std::to_string(idx + 1) + " feedback");
        editor.selfSubPanel->setVisible(true);
        editor.selfSubPanel->setSelectedIndex(idx);
    }
    else
    {
        auto si = MatrixIndex::sourceIndexAt(idx);
        auto ti = MatrixIndex::targetIndexAt(idx);
        std::string glyph = u8"\U00002192";
        auto nm = "Op " + std::to_string(si + 1) + " " + glyph + " Op " + std::to_string(ti + 1);
        editor.singlePanel->setName(nm);

        editor.matrixSubPanel->setVisible(true);
        editor.matrixSubPanel->setSelectedIndex(idx);
    }

    highlight->setVisible(true);
    highlight->setBounds(rectangleFor(idx, self));
    highlight->toBack();
}

} // namespace baconpaul::six_sines::ui