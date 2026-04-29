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

#include "macro_usage.h"
#include "patch.h"
#include "matrix_index.h"
#include "mod_matrix.h"

namespace baconpaul::six_sines
{

namespace
{
template <typename Node>
void scanNode(const Node &node, MacroUsedRef::NodeKind kind, int nodeIndex,
              const std::string &nodeLabel,
              std::array<std::vector<MacroUsedRef>, numMacros> &result)
{
    for (int s = 0; s < numModsPer; ++s)
    {
        auto src = static_cast<int>(std::round(node.modsource[s].value));
        auto tgt = static_cast<int>(std::round(node.modtarget[s].value));
        if (tgt == 0 /* NONE */ || src == ModMatrixConfig::Source::OFF)
            continue;

        int macroIdx = -1;
        bool modulated = false;
        if (src >= ModMatrixConfig::Source::MACRO_0 &&
            src < ModMatrixConfig::Source::MACRO_0 + (int)numMacros)
        {
            macroIdx = src - ModMatrixConfig::Source::MACRO_0;
            modulated = false;
        }
        else if (src >= ModMatrixConfig::Source::MACRO_MOD_0 &&
                 src < ModMatrixConfig::Source::MACRO_MOD_0 + (int)numMacros)
        {
            macroIdx = src - ModMatrixConfig::Source::MACRO_MOD_0;
            modulated = true;
        }
        if (macroIdx < 0)
            continue;

        std::string targetName{"?"};
        for (auto &[id, nm] : node.targetList)
        {
            if (id == tgt)
            {
                targetName = nm;
                break;
            }
        }

        result[macroIdx].push_back({kind, nodeIndex, nodeLabel, modulated, targetName});
    }
}
} // namespace

std::array<std::vector<MacroUsedRef>, numMacros> computeMacroUsage(const Patch &p)
{
    std::array<std::vector<MacroUsedRef>, numMacros> result;

    using K = MacroUsedRef::NodeKind;
    for (int i = 0; i < numOps; ++i)
    {
        scanNode(p.sourceNodes[i], K::SourceOp, i, "Op " + std::to_string(i + 1) + " Source",
                 result);
        scanNode(p.selfNodes[i], K::SelfOp, i, "Op " + std::to_string(i + 1) + " Self", result);
        scanNode(p.mixerNodes[i], K::MixerOp, i, "Op " + std::to_string(i + 1) + " Mixer", result);
    }
    for (int i = 0; i < (int)matrixSize; ++i)
    {
        auto srcOp = MatrixIndex::sourceIndexAt(i);
        auto tgtOp = MatrixIndex::targetIndexAt(i);
        scanNode(p.matrixNodes[i], K::Matrix, i,
                 "Op " + std::to_string(srcOp + 1) + " " + u8"\U00002192" + " Op " +
                     std::to_string(tgtOp + 1),
                 result);
    }
    for (int i = 0; i < numMacros; ++i)
    {
        scanNode(p.macroNodes[i], K::Macro, i, "Macro " + std::to_string(i + 1), result);
    }
    scanNode(p.output, K::MainOutput, 0, "Main Output", result);
    scanNode(p.fineTuneMod, K::FineTune, 0, "Main Tuning", result);
    scanNode(p.mainPanMod, K::MainPan, 0, "Main Pan", result);

    return result;
}

} // namespace baconpaul::six_sines
