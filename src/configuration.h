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
#include <cstring>
#include <iostream>
#include "sst/plugininfra/version_information.h"

namespace baconpaul::six_sines
{

static constexpr size_t blockSize{8};

static constexpr size_t numOps{6};
static constexpr size_t matrixSize{(numOps * (numOps - 1)) / 2};
static constexpr size_t numMacros{6};

static constexpr size_t maxVoices{64};

static constexpr size_t numModsPer{3};

extern int debugLevel;

// These values are streamed
enum SampleRateStrategy
{
    SR_110120 = 0, // If in 44.1 land use 110.25, if not use 120 (2.5x the 'base' rate)
    SR_132144 = 1, // or 3x
    SR_176192 = 2, // or 4x
    SR_220240 = 3  // or 5x
};

enum ResamplerEngine
{
    SRC_FAST,
    SRC_MEDIUM,
    SRC_BEST,
    LANCZOS,
    LINTERP,
    ZOH
};

} // namespace baconpaul::six_sines

inline std::string fileTrunc(const std::string &f)
{
    auto p = f.find(sst::plugininfra::VersionInformation::cmake_source_dir);
    if (p != std::string::npos)
    {
        return f.substr(p + strlen(sst::plugininfra::VersionInformation::cmake_source_dir) + 1);
    }
    return f;
}

#define SXSNLOG(...)                                                                               \
    std::cout << fileTrunc(__FILE__) << ":" << __LINE__ << " " << __VA_ARGS__ << std::endl;
#define SXSNV(x) " " << #x << "=" << x

#endif // CONFIGURATION_H
