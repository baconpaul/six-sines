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

#include "main-sub-panel.h"
#include "ui-constants.h"

namespace baconpaul::six_sines::ui
{
MainSubPanel::MainSubPanel(SixSinesEditor &e)
    : HasEditor(e), DAHDSRComponents(), ModulationComponents()
{
    auto &on = editor.patchCopy.output;
    voiceTrigerAllowed = false;
    setupDAHDSR(e, on);
    setupModulation(e, on);

    velTitle = std::make_unique<RuledLabel>();
    velTitle->setText("Sens");
    addAndMakeVisible(*velTitle);

    createComponent(editor, *this, on.velSensitivity, velSen, velSenD);
    addAndMakeVisible(*velSen);
    velSenL = std::make_unique<jcmp::Label>();
    velSenL->setText("Amp");
    addAndMakeVisible(*velSenL);
};

MainSubPanel::~MainSubPanel() {}

void MainSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto r = layoutDAHDSRAt(p.getX(), p.getY());

    auto depx = r.getX() + 2 * uicMargin;
    auto depy = r.getY();
    auto xtraW = 10;
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, velTitle);

    depy += uicTitleLabelHeight;
    positionKnobAndLabel(depx + xtraW, depy, velSen, velSenL);

    layoutModulation(p);
}

} // namespace baconpaul::six_sines::ui