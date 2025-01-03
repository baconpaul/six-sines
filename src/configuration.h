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

#ifndef BACONPAUL_SIX_SINES_CONFIGURATION_H
#define BACONPAUL_SIX_SINES_CONFIGURATION_H

#include <stddef.h>
#include <cstdint>
#include <string>
#include <iostream>

namespace baconpaul::six_sines
{

static constexpr size_t blockSize{8};
static constexpr float overSampleFactor{2.5};

static constexpr size_t numOps{6};
static constexpr size_t matrixSize{(numOps * (numOps - 1)) / 2};
static constexpr size_t numMacros{6};

static constexpr size_t maxVoices{64};

static constexpr size_t numModsPer{3};

} // namespace baconpaul::six_sines

inline std::string fileTrunc(const std::string &f)
{
    auto p = f.find("/src/");
    if (p != std::string::npos)
    {
        return f.substr(p + 1);
    }
    return f;
}

#define SXSNLOG(...)                                                                               \
    std::cout << fileTrunc(__FILE__) << ":" << __LINE__ << " " << __VA_ARGS__ << std::endl;
#define SXSNV(x) " " << #x << "=" << x

#endif // CONFIGURATION_H
