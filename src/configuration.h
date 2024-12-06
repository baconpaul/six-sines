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

#ifndef BACONPAUL_FMTHING_CONFIGURATION_H
#define BACONPAUL_FMTHING_CONFIGURATION_H

#include <stddef.h>
#include <cstdint>
#include <string>
#include <iostream>

namespace baconpaul::fm
{
static constexpr size_t blockSize{16};
static constexpr float gSampleRate{128000.0};
static constexpr size_t numOps{6};
static constexpr size_t matrixSize{(numOps * (numOps - 1)) / 2};

} // namespace baconpaul::fm

inline std::string fileTrunc(const std::string &f)
{
    auto p = f.find("/src/");
    if (p != std::string::npos)
    {
        return f.substr(p + 1);
    }
    return f;
}

#define FMLOG(...)                                                                                 \
    std::cout << fileTrunc(__FILE__) << ":" << __LINE__ << " " << __VA_ARGS__ << std::endl;

#endif // CONFIGURATION_H
