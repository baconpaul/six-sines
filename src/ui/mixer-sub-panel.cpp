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

    createComponent(editor, *this, sn.envLfoSum, lfoMul, lfoMulD);
    addAndMakeVisible(*lfoMul);
    lfoMul->direction = jcmp::MultiSwitch::VERTICAL;

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

    panTitle = std::make_unique<RuledLabel>();
    panTitle->setText("Pan");
    createComponent(editor, *this, sn.pan, pan, panD);
    panL = std::make_unique<jcmp::Label>();
    panL->setText("Pan");
    addAndMakeVisible(*panTitle);
    addAndMakeVisible(*pan);
    addAndMakeVisible(*panL);

    resized();
}

void MixerSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto gh = (pn.getHeight() - 2 * uicPowerButtonSize) / 2;
    lfoMul->setBounds(pn.getX() + uicMargin, pn.getY() + gh, uicPowerButtonSize,
                      2 * uicPowerButtonSize);
    pn = pn.translated(2 * uicMargin + uicPowerButtonSize, 0);
    auto r = layoutLFOAt(pn.getX(), p.getY(), uicMargin + uicKnobSize);

    positionKnobAndLabel(r.getX() - uicKnobSize, r.getY() + uicTitleLabelHeight, lfoToLevel,
                         lfoToLevelL);
    positionKnobAndLabel(r.getX() - uicKnobSize,
                         r.getY() + uicTitleLabelHeight + uicLabeledKnobHeight + uicMargin,
                         lfoToPan, lfoToPanL);

    auto depx = r.getX() + 2 * uicMargin;
    auto depy = r.getY();
    positionTitleLabelAt(depx, depy, uicKnobSize + uicMargin, panTitle);

    depy += uicTitleLabelHeight;
    positionKnobAndLabel(depx, depy, pan, panL);
}

} // namespace baconpaul::six_sines::ui