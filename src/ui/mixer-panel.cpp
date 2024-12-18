/*
 * BaconPaul's FM Atrocity
 *
 * A mess, with FM.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#include "mixer-panel.h"
#include "mixer-sub-panel.h"
#include "ifm-editor.h"

namespace baconpaul::fm::ui
{
MixerPanel::MixerPanel(IFMEditor &e) : jcmp::NamedPanel("Mixer"), HasEditor(e)
{
    auto &mn = editor.patchCopy.mixerNodes;
    for (auto i = 0U; i < numOps; ++i)
    {
        createComponent(editor, *this, mn[i].level.meta.id, knobs[i], knobsData[i], i);
        knobs[i]->setDrawLabel(false);
        addAndMakeVisible(*knobs[i]);

        createComponent(editor, *this, mn[i].active.meta.id, power[i], powerData[i], i);
        power[i]->setDrawMode(sst::jucegui::components::ToggleButton::DrawMode::GLYPH);
        power[i]->setGlyph(sst::jucegui::components::GlyphPainter::POWER);
        addAndMakeVisible(*power[i]);

        labels[i] = std::make_unique<jcmp::Label>();
        labels[i]->setText("Op " + std::to_string(i + 1) + " Lev");
        addAndMakeVisible(*labels[i]);
    }
}
MixerPanel::~MixerPanel() = default;

void MixerPanel::resized()
{
    auto sz = 50;
    auto bsz = 26;
    auto lsz = 18;
    auto b = getContentArea().withWidth(sz + bsz).withHeight(sz);
    for (auto i = 0U; i < numOps; ++i)
    {
        knobs[i]->setBounds(b.withTrimmedLeft(bsz));
        power[i]->setBounds(b.withWidth(bsz).withTrimmedTop(bsz / 2).withTrimmedBottom(bsz / 2));
        labels[i]->setBounds(b.translated(0, sz).withHeight(lsz));
        b = b.translated(0, sz + lsz + 2);
    }
}

void MixerPanel::beginEdit(size_t idx)
{
    editor.hideAllSubPanels();
    editor.mixerSubPanel->setIndex(idx);
    editor.mixerSubPanel->setVisible(true);
}

} // namespace baconpaul::fm::ui