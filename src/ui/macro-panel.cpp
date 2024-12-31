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

#include "macro-panel.h"
#include "six-sines-editor.h"
#include "ui-constants.h"
#include "knob-highlight.h"

namespace baconpaul::six_sines::ui
{
MacroPanel::MacroPanel(SixSinesEditor &e) : jcmp::NamedPanel("Macros"), HasEditor(e)
{
    auto &mn = editor.patchCopy.macroNodes;
    for (auto i = 0U; i < numOps; ++i)
    {
        createComponent(editor, *this, mn[i].level, knobs[i], knobsData[i], i);
        knobs[i]->setDrawLabel(false);
        addAndMakeVisible(*knobs[i]);

        labels[i] = std::make_unique<jcmp::Label>();
        labels[i]->setText("Macro " + std::to_string(i + 1));
        addAndMakeVisible(*labels[i]);
    }
}
MacroPanel::~MacroPanel() = default;

void MacroPanel::resized()
{
    auto b = getContentArea().reduced(uicMargin, 0);
    auto x = b.getX() + (b.getWidth() - uicKnobSize) / 2;
    auto y = b.getY();
    for (auto i = 0U; i < numOps; ++i)
    {
        positionKnobAndLabel(x, y, knobs[i], labels[i]);
        y += uicLabeledKnobHeight + uicMargin;
    }
}

void MacroPanel::beginEdit(size_t idx) {}

} // namespace baconpaul::six_sines::ui