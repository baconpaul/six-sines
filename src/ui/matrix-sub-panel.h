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

#ifndef BACONPAUL_SIX_SINES_UI_MATRIX_SUB_PANEL_H
#define BACONPAUL_SIX_SINES_UI_MATRIX_SUB_PANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "six-sines-editor.h"
#include "dahdsr-components.h"
#include "lfo-components.h"
#include "modulation-components.h"

namespace baconpaul::six_sines::ui
{
struct MatrixSubPanel : juce::Component,
                        HasEditor,
                        DAHDSRComponents<MatrixSubPanel, Patch::MatrixNode>,
                        LFOComponents<MatrixSubPanel, Patch::MatrixNode>
{
    MatrixSubPanel(SixSinesEditor &);
    ~MatrixSubPanel();

    void resized() override;

    void beginEdit() {}

    size_t index{0};
    void setSelectedIndex(int idx);

    std::unique_ptr<jcmp::Knob> lfoToDepth;
    std::unique_ptr<PatchContinuous> lfoToDepthD;
    std::unique_ptr<jcmp::Label> lfoToDepthL;

    std::unique_ptr<jcmp::MultiSwitch> lfoMul;
    std::unique_ptr<PatchDiscrete> lfoMulD;
};
} // namespace baconpaul::six_sines::ui
#endif // MAIN_SUB_PANEL_H
