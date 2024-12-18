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

#ifndef BACONPAUL_FMTHING_UI_SOURCE_PANEL_H
#define BACONPAUL_FMTHING_UI_SOURCE_PANEL_H

#include "ifm-editor.h"
#include "patch-data-bindings.h"

namespace baconpaul::fm::ui
{
struct SourcePanel : jcmp::NamedPanel, HasEditor
{
    SourcePanel(IFMEditor &);
    ~SourcePanel();

    void resized() override;

    void beginEdit();

    std::unique_ptr<PatchContinuous> levC, panC;
    std::unique_ptr<jcmp::Knob> levK, panK;
};
} // namespace baconpaul::fm::ui
#endif // MAIN_PANEL_H
