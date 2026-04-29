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

#ifndef BACONPAUL_SIX_SINES_UI_MACRO_PANEL_H
#define BACONPAUL_SIX_SINES_UI_MACRO_PANEL_H

#include <sst/jucegui/components/Knob.h>
#include <sst/jucegui/components/Label.h>
#include <sst/jucegui/components/ToggleButton.h>
#include <sst/jucegui/data/Continuous.h>
#include "six-sines-editor.h"
#include "patch-data-bindings.h"

namespace baconpaul::six_sines::ui
{
namespace jcmp = sst::jucegui::components;
namespace jdat = sst::jucegui::data;

struct MacroPanel : jcmp::NamedPanel, HasEditor
{
    MacroPanel(SixSinesEditor &);
    ~MacroPanel();

    void resized() override;

    void beginEdit(size_t idx);

    void refreshLabel(size_t idx);

    // Single source of truth for macro display names. Short = knob label,
    // Full = sub-panel title. Both fall back to "Macro N" when unrenamed.
    static std::string displayShortName(const SixSinesEditor &editor, size_t idx)
    {
        auto &buf = editor.patchCopy.macroNames[idx];
        std::string nm(buf.data());
        auto def = "Macro " + std::to_string(idx + 1);
        return (nm.empty() || nm == def) ? def : nm;
    }
    static std::string displayName(const SixSinesEditor &editor, size_t idx)
    {
        auto &buf = editor.patchCopy.macroNames[idx];
        std::string nm(buf.data());
        auto def = "Macro " + std::to_string(idx + 1);
        return (nm.empty() || nm == def) ? def : (nm + " (" + def + ")");
    }

    void mouseDown(const juce::MouseEvent &e) override;
    juce::Rectangle<int> rectangleFor(int idx);

    std::unique_ptr<juce::Component> highlight;
    void clearHighlight()
    {
        if (highlight)
            highlight->setVisible(false);
    }

    std::array<std::unique_ptr<jcmp::Knob>, numMacros> knobs;
    std::array<std::unique_ptr<PatchContinuous>, numMacros> knobsData;
    std::array<std::unique_ptr<jcmp::Label>, numMacros> labels;

    std::array<std::unique_ptr<jcmp::ToggleButton>, numMacros> power;
    std::array<std::unique_ptr<PatchDiscrete>, numMacros> powerD;
};
} // namespace baconpaul::six_sines::ui
#endif // BACONPAUL_SIX_SINES_UI_MACRO_PANEL_H
