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

#include "mixer-sub-panel.h"

namespace baconpaul::six_sines::ui
{
MixerSubPanel::MixerSubPanel(SixSinesEditor &e) : HasEditor(e) { setSelectedIndex(0); };
MixerSubPanel::~MixerSubPanel() {}

void MixerSubPanel::setSelectedIndex(int idx)
{
    index = idx;
    repaint();

    removeAllChildren();

    auto &sn = editor.patchCopy.mixerNodes[idx];
    setupDAHDSR(editor, sn);
    setupLFO(editor, sn);
    setupModulation(editor, sn);

    createComponent(editor, *this, sn.lfoToLevel, lfoToLevel, lfoToLevelD);
    addAndMakeVisible(*lfoToLevel);
    lfoToLevelL = std::make_unique<jcmp::Label>();
    lfoToLevelL->setText(std::string() + u8"\U00002192" + "Lev");
    addAndMakeVisible(*lfoToLevelL);

    createComponent(editor, *this, sn.lfoToPan, lfoToPan, lfoToPanD);
    addAndMakeVisible(*lfoToPan);
    lfoToPanL = std::make_unique<jcmp::Label>();
    lfoToPanL->setText(std::string() + u8"\U00002192" + "Pan");
    addAndMakeVisible(*lfoToPanL);

    resized();
}

void MixerSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto gh = (pn.getHeight() - 2 * uicPowerButtonSize) / 2;
    auto r = layoutLFOAt(pn.getX() + uicMargin, p.getY(), uicMargin + uicKnobSize);

    positionKnobAndLabel(r.getX() - uicKnobSize, r.getY() + uicTitleLabelHeight, lfoToLevel,
                         lfoToLevelL);
    positionKnobAndLabel(r.getX() - uicKnobSize,
                         r.getY() + uicTitleLabelHeight + uicLabeledKnobHeight + uicMargin,
                         lfoToPan, lfoToPanL);

    layoutModulation(p);
}

} // namespace baconpaul::six_sines::ui