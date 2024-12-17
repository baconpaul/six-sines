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
        createComponent(editor, *this, mn[i].level.meta.id, knobs[i], knobsData[i]);
        knobs[i]->setDrawLabel(false);
        addAndMakeVisible(*knobs[i]);
        labels[i] = std::make_unique<jcmp::Label>();
        labels[i]->setText("Op " + std::to_string(i + 1) + " Lev");
        addAndMakeVisible(*labels[i]);
    }
}
MixerPanel::~MixerPanel() = default;

void MixerPanel::resized()
{
    auto sz = 50;
    auto lsz = 18;
    auto b = getContentArea().withWidth(sz).withHeight(sz);
    for (auto i = 0U; i < numOps; ++i)
    {
        knobs[i]->setBounds(b);
        labels[i]->setBounds(b.translated(0, sz).withHeight(lsz));
        b = b.translated(0, sz + lsz + 2);
    }
}

void MixerPanel::beginEdit()
{
    FMLOG("MixerPanel beginEdit");
    editor.hideAllSubPanels();
    editor.mixerSubPanel->setVisible(true);
}

} // namespace baconpaul::fm::ui