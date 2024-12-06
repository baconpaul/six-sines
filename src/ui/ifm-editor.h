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

#ifndef BACONPAUL_FMTHING_UI_IFM_EDITOR_H
#define BACONPAUL_FMTHING_UI_IFM_EDITOR_H

#include <juce_gui_basics/juce_gui_basics.h>

namespace baconpaul::fm::ui
{
struct IFMEditor : juce::Component
{
    IFMEditor();
    virtual ~IFMEditor() = default;

    void paint(juce::Graphics &g) override;
};
} // namespace baconpaul::fm::ui
#endif // IFM_EDITOR_H
