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

#ifndef BACONPAUL_SIX_SINES_SYNTH_MACRO_USAGE_H
#define BACONPAUL_SIX_SINES_SYNTH_MACRO_USAGE_H

#include <array>
#include <vector>
#include <string>
#include "configuration.h"

namespace baconpaul::six_sines
{
struct Patch;

// One consumer of a macro. UI renders as [jump button] [nodeLabel] [mod/amp]
// [targetName]; kind+index drives the jump button.
struct MacroUsedRef
{
    enum class NodeKind
    {
        SourceOp,   // sourcePanel->beginEdit(index)
        SelfOp,     // matrixPanel->beginEdit(index, true)
        MixerOp,    // mixerPanel->beginEdit(index)
        Matrix,     // matrixPanel->beginEdit(index, false)
        Macro,      // macroPanel->beginEdit(index)
        MainOutput, // mainPanel->beginEdit(0)
        MainPan,    // mainPanel->beginEdit(1)
        FineTune    // mainPanel->beginEdit(2)
    };
    NodeKind kind;
    int index;              // op idx / matrix slot / macro idx; ignored for singletons
    std::string nodeLabel;  // "Op 1 → Op 3", "Op 1 Self", "Macro 2", "Main Output"
    bool modulated;         // true = MACRO_MOD source, false = MACRO (amp)
    std::string targetName; // "Level", "LFO Rate", ...
};

// Walk every modsource[] across the patch and bucket each active reference
// to MACRO_n / MACRO_MOD_n into the corresponding macro's list. A slot is
// "active" if its modtarget is not NONE.
std::array<std::vector<MacroUsedRef>, numMacros> computeMacroUsage(const Patch &p);
} // namespace baconpaul::six_sines

#endif // BACONPAUL_SIX_SINES_SYNTH_MACRO_USAGE_H
