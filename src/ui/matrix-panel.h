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

#ifndef BACONPAUL_SIX_SINES_UI_MATRIX_PANEL_H
#define BACONPAUL_SIX_SINES_UI_MATRIX_PANEL_H

#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/ToggleButton.h>
#include "six-sines-editor.h"
#include "patch-data-bindings.h"

namespace baconpaul::six_sines::ui
{
struct MatrixPanel : jcmp::NamedPanel, HasEditor
{
    MatrixPanel(SixSinesEditor &);
    ~MatrixPanel();

    void resized() override;

    void beginEdit(size_t, bool);

    std::array<std::unique_ptr<jcmp::Knob>, numOps> Sknobs;
    std::array<std::unique_ptr<PatchContinuous>, numOps> SknobsData;
    std::array<std::unique_ptr<jcmp::ToggleButton>, numOps> Spower;
    std::array<std::unique_ptr<PatchDiscrete>, numOps> SpowerData;
    std::array<std::unique_ptr<jcmp::Label>, numOps> Slabels;

    std::array<std::unique_ptr<jcmp::Knob>, matrixSize> Mknobs;
    std::array<std::unique_ptr<PatchContinuous>, matrixSize> MknobsData;
    std::array<std::unique_ptr<jcmp::ToggleButton>, matrixSize> Mpower;
    std::array<std::unique_ptr<PatchDiscrete>, matrixSize> MpowerData;
    std::array<std::unique_ptr<jcmp::Label>, matrixSize> Mlabels;
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_PANEL_H
