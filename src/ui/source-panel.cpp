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

#include "source-panel.h"

#include "source-sub-panel.h"

namespace baconpaul::fm::ui
{
SourcePanel::SourcePanel(IFMEditor &e) : jcmp::NamedPanel("Source"), HasEditor(e)
{
    createComponent(editor, *this, editor.patchCopy.output.level.meta.id, levK, levC);
    addAndMakeVisible(*levK);

    createComponent(editor, *this, editor.patchCopy.output.pan.meta.id, panK, panC);
    addAndMakeVisible(*panK);
}
SourcePanel::~SourcePanel() = default;

void SourcePanel::resized()
{
    auto b = getContentArea();
    auto h = getContentArea().getHeight() - 18;
    auto q = b.withWidth(h);
    levK->setBounds(q);
    panK->setBounds(q.translated(h + 2, 0));
}

void SourcePanel::beginEdit()
{
    editor.hideAllSubPanels();
    editor.sourceSubPanel->setVisible(true);
}

} // namespace baconpaul::fm::ui