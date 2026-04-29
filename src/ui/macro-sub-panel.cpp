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

#include "macro-sub-panel.h"
#include "macro-panel.h"
#include "main-panel.h"
#include "matrix-panel.h"
#include "mixer-panel.h"
#include "source-panel.h"
#include "ui-constants.h"
#include <cstring>
#include <sst/jucegui/layouts/ListLayout.h>

namespace baconpaul::six_sines::ui
{

MacroSubPanel::MacroSubPanel(SixSinesEditor &e) : HasEditor(e)
{
    nameEditor = std::make_unique<jcmp::TextEditor>();
    nameEditor->setMultiLine(false);
    nameEditor->setReturnKeyStartsNewLine(false);
    nameEditor->setJustification(juce::Justification::centredTop);
    nameEditor->onReturnKey = [this]() { commitName(); };
    nameEditor->onFocusLost = [this]() { commitName(); };
    addAndMakeVisible(*nameEditor);

    nameTitle = std::make_unique<jcmp::RuledLabel>();
    nameTitle->setText("Name");
    addAndMakeVisible(*nameTitle);

    usedByTitle = std::make_unique<jcmp::RuledLabel>();
    usedByTitle->setText("Used By");
    addAndMakeVisible(*usedByTitle);

    setSelectedIndex(0);
}

MacroSubPanel::~MacroSubPanel() {}

void MacroSubPanel::setSelectedIndex(size_t i)
{
    index = i;

    removeAllChildren();

    auto &mn = editor.patchCopy.macroNodes[i];

    setupDAHDSR(editor, mn);
    setupLFO(editor, mn);
    setupModulation(editor, mn);

    // Excludes self/higher-indexed macros, then falls through to shared.
    sourceFilter = [this](int sid) -> SourceFilterResult
    {
        using S = ModMatrixConfig::Source;
        auto isAmp = sid >= S::MACRO_0 && sid < S::MACRO_0 + (int)numMacros;
        auto isMod = sid >= S::MACRO_MOD_0 && sid < S::MACRO_MOD_0 + (int)numMacros;
        if (isAmp && (sid - (int)S::MACRO_0) >= (int)index)
            return SourceFilterResult::Exclude;
        if (isMod && (sid - (int)S::MACRO_MOD_0) >= (int)index)
            return SourceFilterResult::Exclude;
        return sharedSourceFilter(sid);
    };

    addAndMakeVisible(*nameTitle);
    addAndMakeVisible(*nameEditor);
    addAndMakeVisible(*usedByTitle);

    // Env-/LFO-→ row above the Name section
    envSecLab = std::make_unique<jcmp::RuledLabel>();
    envSecLab->setText(std::string() + "Env" + u8"\U00002192");
    addAndMakeVisible(*envSecLab);

    lfoSecLab = std::make_unique<jcmp::RuledLabel>();
    lfoSecLab->setText(std::string() + "LFO" + u8"\U00002192");
    addAndMakeVisible(*lfoSecLab);

    createRescaledComponent(editor, *this, mn.envDepth, envDepthKnob, envDepthDA);
    addAndMakeVisible(*envDepthKnob);
    envDepthLab = std::make_unique<jcmp::Label>();
    envDepthLab->setText("Depth");
    addAndMakeVisible(*envDepthLab);

    createRescaledComponent(editor, *this, mn.lfoDepth, lfoDepthKnob, lfoDepthDA);
    addAndMakeVisible(*lfoDepthKnob);
    lfoDepthLab = std::make_unique<jcmp::Label>();
    lfoDepthLab->setText("Depth");
    addAndMakeVisible(*lfoDepthLab);

    createComponent(editor, *this, mn.envIsMultiplcative, envMul, envMulD);
    addAndMakeVisible(*envMul);

    auto refreshEnabled = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->setEnabledState();
    };
    editor.componentRefreshByID[mn.envIsMultiplcative.meta.id] = refreshEnabled;
    envMulD->onGuiSetValue = refreshEnabled;

    auto &nameBuf = editor.patchCopy.macroNames[i];
    nameEditor->setText(juce::String(nameBuf.data()), juce::dontSendNotification);

    setEnabledState();
    refreshUsedByList();

    resized();
    repaint();
}

void MacroSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);

    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto r = layoutLFOAt(pn.getX() + uicMargin, p.getY());
    layoutModulation(p);

    // Env-→ row spans two columns (depth knob + add/scale switch); LFO-→ stays one column.
    auto depY = std::max(pn.getBottom(), r.getBottom()) + uicMargin;
    auto envColW = 2 * uicSubPanelColumnWidth + uicMargin;
    auto lfoColW = uicSubPanelColumnWidth;
    auto envX = p.getX();
    auto lfoX = envX + envColW + 2 * uicMargin;

    envSecLab->setBounds(envX, depY, envColW, uicTitleLabelInnerBox);
    lfoSecLab->setBounds(lfoX, depY, lfoColW, uicTitleLabelInnerBox);

    auto bodyY = depY + uicTitleLabelInnerBox + uicMargin;

    auto envDepthKnobX = envX + (int)(uicSubPanelColumnWidth - uicKnobSize) / 2;
    envDepthKnob->setBounds(envDepthKnobX, bodyY, uicKnobSize, uicKnobSize);
    envDepthLab->setBounds(envX, bodyY + uicKnobSize + uicLabelGap, uicSubPanelColumnWidth,
                           uicLabelHeight);

    // Vertically center the add/scale switch against the labeled knob height.
    auto switchH = 2 * uicLabelHeight + uicMargin;
    auto switchX = envX + uicSubPanelColumnWidth + uicMargin;
    auto switchY = bodyY + ((int)uicLabeledKnobHeight - (int)switchH) / 2;
    envMul->setBounds(switchX, switchY, uicSubPanelColumnWidth, switchH);

    auto lfoDepthKnobX = lfoX + (int)(uicSubPanelColumnWidth - uicKnobSize) / 2;
    lfoDepthKnob->setBounds(lfoDepthKnobX, bodyY, uicKnobSize, uicKnobSize);
    lfoDepthLab->setBounds(lfoX, bodyY + uicKnobSize + uicLabelGap, lfoColW, uicLabelHeight);

    auto nameY = bodyY + uicLabeledKnobHeight + uicMargin;
    nameTitle->setBounds(p.getX(), nameY, p.getWidth(), uicTitleLabelInnerBox);
    auto edInset = 60;
    nameEditor->setBounds(p.getX() + edInset, nameY + uicTitleLabelInnerBox + uicMargin,
                          p.getWidth() - 2 * edInset, 24);

    auto usedByY = nameY + uicTitleLabelInnerBox + uicMargin + 24 + uicMargin;
    usedByTitle->setBounds(p.getX(), usedByY, p.getWidth(), uicTitleLabelInnerBox);
    auto rowY = usedByY + uicTitleLabelInnerBox + uicMargin;

    if (usedByEmptyState && !usedByRows.empty() && usedByRows.front().nodeLabel)
    {
        usedByRows.front().nodeLabel->setBounds(p.getX(), rowY, p.getWidth(), uicLabelHeight);
        return;
    }

    constexpr int btnW = uicLabelHeight;
    constexpr int variantW = 40;
    constexpr int nodeW = 140;
    auto gap = uicMargin;
    for (auto &row : usedByRows)
    {
        auto x = p.getX();
        if (row.jumpButton)
            row.jumpButton->setBounds(x, rowY, btnW, uicLabelHeight);
        x += btnW + gap;
        if (row.nodeLabel)
            row.nodeLabel->setBounds(x, rowY, nodeW, uicLabelHeight);
        x += nodeW + gap;
        if (row.variantLabel)
            row.variantLabel->setBounds(x, rowY, variantW, uicLabelHeight);
        x += variantW + gap;
        if (row.targetLabel)
            row.targetLabel->setBounds(x, rowY, p.getRight() - x, uicLabelHeight);
        rowY += uicLabelHeight + uicLabelGap;
    }
}

void MacroSubPanel::setEnabledState()
{
    if (!patchPartPtr)
        return;
    auto on = patchPartPtr->macroPower.value > 0.5;

    // Envelope widgets
    for (auto &s : slider)
        if (s)
            s->setEnabled(on);
    for (auto &l : lab)
        if (l)
            l->setEnabled(on);
    for (auto &sh : shapes)
        if (sh)
            sh->setEnabled(on);
    if (triggerButton)
        triggerButton->setEnabled(on);
    if (envTitleLab)
        envTitleLab->setEnabled(on);
    if (lfoTitleLab)
        lfoTitleLab->setEnabled(on);

    // LFO widgets
    lfoSetEnabledState(on);

    // Modulation widgets
    for (auto &sm : sourceMenu)
        if (sm)
            sm->setEnabled(on);
    for (auto &tm : targetMenu)
        if (tm)
            tm->setEnabled(on);
    for (auto &ds : depthSlider)
        if (ds)
            ds->setEnabled(on);
    if (modTitleLab)
        modTitleLab->setEnabled(on);

    // Env-/LFO-→ widgets. envDepth only matters in Add mode (envIsMul=0).
    auto inAddMode = envMulD && envMulD->getValue() < 0.5;
    if (envSecLab)
        envSecLab->setEnabled(on);
    if (lfoSecLab)
        lfoSecLab->setEnabled(on);
    if (envDepthKnob)
        envDepthKnob->setEnabled(on && inAddMode);
    if (envDepthLab)
        envDepthLab->setEnabled(on && inAddMode);
    if (envMul)
        envMul->setEnabled(on);
    if (lfoDepthKnob)
        lfoDepthKnob->setEnabled(on);
    if (lfoDepthLab)
        lfoDepthLab->setEnabled(on);

    // nameEditor, powerToggle stay enabled regardless
    if (nameEditor)
        nameEditor->setEnabled(true);
    repaint();
}

void MacroSubPanel::refreshNameFromPatch()
{
    if (!nameEditor)
        return;
    auto &buf = editor.patchCopy.macroNames[index];
    nameEditor->setText(juce::String(buf.data()), juce::dontSendNotification);
    if (editor.singlePanel)
        editor.singlePanel->setName(displayName());
}

void MacroSubPanel::refreshUsedByList()
{
    usedByRows.clear();
    auto &uses = editor.macroUsageCache[index];
    if (uses.empty())
    {
        UsedByRow row;
        row.nodeLabel = std::make_unique<jcmp::Label>();
        row.nodeLabel->setText("(macro unused)");
        addAndMakeVisible(*row.nodeLabel);
        usedByRows.push_back(std::move(row));
        usedByEmptyState = true;
    }
    else
    {
        usedByEmptyState = false;
        for (auto &u : uses)
        {
            UsedByRow row;
            row.jumpButton =
                std::make_unique<jcmp::GlyphButton>(jcmp::GlyphPainter::GlyphType::JOG_RIGHT);
            // Copy: the cache rebuilds on routing changes, invalidating refs into it.
            auto refCopy = u;
            row.jumpButton->setOnCallback(
                [w = juce::Component::SafePointer(this), refCopy]()
                {
                    if (w)
                        w->jumpTo(refCopy);
                });
            addAndMakeVisible(*row.jumpButton);

            row.nodeLabel = std::make_unique<jcmp::Label>();
            row.nodeLabel->setText(u.nodeLabel);
            addAndMakeVisible(*row.nodeLabel);

            row.variantLabel = std::make_unique<jcmp::Label>();
            row.variantLabel->setText(u.modulated ? "mod" : "amp");
            addAndMakeVisible(*row.variantLabel);

            row.targetLabel = std::make_unique<jcmp::Label>();
            row.targetLabel->setText(u.targetName);
            addAndMakeVisible(*row.targetLabel);

            usedByRows.push_back(std::move(row));
        }
    }
    resized();
}

void MacroSubPanel::jumpTo(const MacroUsedRef &ref)
{
    using K = MacroUsedRef::NodeKind;
    switch (ref.kind)
    {
    case K::SourceOp:
        if (editor.sourcePanel)
            editor.sourcePanel->beginEdit(ref.index);
        break;
    case K::SelfOp:
        if (editor.matrixPanel)
            editor.matrixPanel->beginEdit(ref.index, true);
        break;
    case K::MixerOp:
        if (editor.mixerPanel)
            editor.mixerPanel->beginEdit(ref.index);
        break;
    case K::Matrix:
        if (editor.matrixPanel)
            editor.matrixPanel->beginEdit(ref.index, false);
        break;
    case K::Macro:
        if (editor.macroPanel)
            editor.macroPanel->beginEdit(ref.index);
        break;
    case K::MainOutput:
        if (editor.mainPanel)
            editor.mainPanel->beginEdit(0);
        break;
    case K::MainPan:
        if (editor.mainPanel)
            editor.mainPanel->beginEdit(1);
        break;
    case K::FineTune:
        if (editor.mainPanel)
            editor.mainPanel->beginEdit(2);
        break;
    }
}

std::string MacroSubPanel::displayShortName() const
{
    return MacroPanel::displayShortName(editor, index);
}

std::string MacroSubPanel::displayName() const { return MacroPanel::displayName(editor, index); }

void MacroSubPanel::commitName()
{
    auto txt = nameEditor->getText().toStdString();
    auto &buf = editor.patchCopy.macroNames[index];

    // No-op on focus-enter/exit without an edit; only push on real changes.
    if (std::strncmp(buf.data(), txt.c_str(), buf.size()) == 0)
        return;

    std::fill(buf.begin(), buf.end(), 0);
    auto n = std::min(txt.size(), buf.size() - 1);
    std::memcpy(buf.data(), txt.data(), n);

    Synth::MainToAudioMsg msg{Synth::MainToAudioMsg::SEND_MACRO_NAME};
    msg.paramId = static_cast<uint32_t>(index);
    msg.uiManagedPointer = buf.data();
    editor.mainToAudio.push(msg);

    if (editor.macroPanel && index < editor.macroPanel->labels.size() &&
        editor.macroPanel->labels[index])
    {
        editor.macroPanel->labels[index]->setText(displayShortName());
        editor.macroPanel->labels[index]->repaint();
    }
    if (editor.singlePanel)
        editor.singlePanel->setName(displayName());
}

IMPLEMENTS_CLIPBOARD_SUPPORT(MacroSubPanel, macroNodes[index], Clipboard::ClipboardType::NONE)

} // namespace baconpaul::six_sines::ui
