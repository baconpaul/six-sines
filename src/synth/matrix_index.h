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

#ifndef BACONPAUL_FMTHING_SYNTH_MATRIX_INDEX_H
#define BACONPAUL_FMTHING_SYNTH_MATRIX_INDEX_H

#include <stddef.h>
#include "configuration.h"

namespace baconpaul::fm
{
struct MatrixIndex
{
    static size_t sourceIndexAt(size_t position)
    {
        assert(position < matrixSize);
        static size_t sourceTable[matrixSize];
        static bool sourceTableInit{false};
        if (!sourceTableInit)
        {
            int idx{0};
            for (int i = 1; i < numOps; ++i)
            {
                for (int j = i; j < numOps; ++j)
                {
                    sourceTable[idx++] = i - 1;
                }
            }
            sourceTableInit = true;
        }
        return sourceTable[position];
    }

    static size_t targetIndexAt(size_t position)
    {
        assert(position < matrixSize);
        static size_t targetTable[matrixSize];
        static bool targetTableInit{false};
        if (!targetTableInit)
        {
            int idx{0};
            for (int i = 1; i < numOps; ++i)
            {
                for (int j = i; j < numOps; ++j)
                {
                    targetTable[idx++] = j;
                }
            }
            targetTableInit = true;
        }
        return targetTable[position];
    }

    static size_t positionForSourceTarget(size_t source, size_t target)
    {
        static size_t positionMatrix[numOps][numOps];
        static bool matrixInit{false};

        if (!matrixInit)
        {
            for (int i = 0; i < numOps; ++i)
            {
                for (int j = 0; j < numOps; ++j)
                {
                    positionMatrix[i][j] = matrixSize + 1;
                }
            }
            for (int i = 0; i < matrixSize; ++i)
            {
                auto s = sourceIndexAt(i);
                auto t = targetIndexAt(i);
                positionMatrix[s][t] = i;
            }
            matrixInit = true;
        }

        assert(positionMatrix[source][target] < matrixSize);

        return positionMatrix[source][target];
    }
};
} // namespace baconpaul::fm
#endif // MATRIX_INDEX_H