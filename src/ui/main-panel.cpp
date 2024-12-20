/*
 * Six Sines A Sinnin'
 *
 * A mess, with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#include "main-panel.h"
#include "ui-constants.h"
#include "main-sub-panel.h"

namespace baconpaul::six_sines::ui
{
MainPanel::MainPanel(SixSinesEditor &e) : jcmp::NamedPanel("Main"), HasEditor(e)
{
    createComponent(editor, *this, editor.patchCopy.output.level.meta.id, lev, levData);
    lev->setDrawLabel(false);
    addAndMakeVisible(*lev);
    levLabel = std::make_unique<jcmp::Label>();
    levLabel->setText("Level");
    addAndMakeVisible(*levLabel);
}
MainPanel::~MainPanel() = default;

void MainPanel::resized()
{
    auto b = getContentArea().reduced(uicMargin, 0);

    positionKnobAndLabel(b.getX(), b.getY(), lev, levLabel);
}

void MainPanel::beginEdit()
{
    editor.hideAllSubPanels();
    editor.mainSubPanel->setVisible(true);

    editor.singlePanel->setName("Main Output");
}

} // namespace baconpaul::six_sines::ui