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

#ifndef BACONPAUL_SIX_SINES_SYNTH_MATRIX_INDEX_H
#define BACONPAUL_SIX_SINES_SYNTH_MATRIX_INDEX_H

#include <stddef.h>
#include "configuration.h"

namespace baconpaul::six_sines
{
struct MatrixIndex
{
    static inline size_t sourceTable[matrixSize];
    static inline size_t targetTable[matrixSize];
    static inline size_t positionMatrix[numOps][numOps];

    static inline bool tablesInitialized{false};
    static bool initialize()
    {
        if (tablesInitialized)
            return tablesInitialized;
        tablesInitialized = true;
        {
            int idx{0};
            for (int t = 1; t < numOps; ++t)
            {
                for (int s = 0; s < t; ++s)
                {
                    sourceTable[idx++] = s;
                }
            }
        }
        {
            int idx{0};
            for (int t = 1; t < numOps; ++t)
            {
                for (int s = 0; s < t; ++s)
                {
                    targetTable[idx++] = t;
                }
            }
        }

        for (int i = 0; i < numOps; ++i)
        {
            for (int j = 0; j < numOps; ++j)
            {
                positionMatrix[i][j] = matrixSize + 1;
            }
        }
        for (int i = 0; i < matrixSize; ++i)
        {
            auto s = sourceTable[i];
            auto t = targetTable[i];
            positionMatrix[s][t] = i;
            SXSNLOG("At " << i << " source " << s << " target " << t);
        }
        return tablesInitialized;
    }

    static inline size_t sourceIndexAt(size_t position)
    {
        assert(tablesInitialized);
        assert(position < matrixSize);
        return sourceTable[position];
    }

    static inline size_t targetIndexAt(size_t position)
    {
        assert(tablesInitialized);
        assert(position < matrixSize);
        return targetTable[position];
    }

    static inline size_t positionForSourceTarget(size_t source, size_t target)
    {
        assert(tablesInitialized);
        assert(positionMatrix[source][target] < matrixSize);

        return positionMatrix[source][target];
    }
};
} // namespace baconpaul::six_sines
#endif // MATRIX_INDEX_H
