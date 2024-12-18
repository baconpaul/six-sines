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

#ifndef BACONPAUL_FMTHING_UI_MIXER_PANEL_H
#define BACONPAUL_FMTHING_UI_MIXER_PANEL_H

#include <sst/jucegui/components/Knob.h>
#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/ToggleButton.h>
#include <sst/jucegui/data/Continuous.h>
#include "ifm-editor.h"
#include "patch-data-bindings.h"

namespace baconpaul::fm::ui
{
namespace jcmp = sst::jucegui::components;
namespace jdat = sst::jucegui::data;

struct MixerPanel : jcmp::NamedPanel, HasEditor
{
    MixerPanel(IFMEditor &);
    ~MixerPanel();

    void resized() override;

    void beginEdit(size_t idx);

    std::array<std::unique_ptr<jcmp::Knob>, numOps> knobs;
    std::array<std::unique_ptr<PatchContinuous>, numOps> knobsData;
    std::array<std::unique_ptr<jcmp::ToggleButton>, numOps> power;
    std::array<std::unique_ptr<PatchDiscrete>, numOps> powerData;
    std::array<std::unique_ptr<jcmp::Label>, numOps> labels;
};
} // namespace baconpaul::fm::ui
#endif // MIXER_PANE_H
