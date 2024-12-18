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

#ifndef BACONPAUL_FMTHING_UI_MAIN_PANEL_H
#define BACONPAUL_FMTHING_UI_MAIN_PANEL_H

#include "sst/jucegui/components/Label.h"

#include "ifm-editor.h"
#include "patch-data-bindings.h"

namespace baconpaul::fm::ui
{
struct MainPanel : jcmp::NamedPanel, HasEditor
{
    MainPanel(IFMEditor &);
    ~MainPanel();

    void resized() override;

    void beginEdit();

    std::unique_ptr<PatchContinuous> levData;
    std::unique_ptr<jcmp::Knob> lev;
    std::unique_ptr<jcmp::Label> levLabel;
};
} // namespace baconpaul::fm::ui
#endif // MAIN_PANEL_H
