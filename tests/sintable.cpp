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

/*
 * SinTable lays a cycle out as 4 quadrants of 4096 points, read by a 14 bit position index
 * that covers the cycle in 16384 steps. The table grid therefore has to be spaced 1/16384
 * of a cycle apart, and index nPoints of each quadrant has to land exactly on the next
 * quadrant's index 0 - that entry only exists to supply the second half of the Hermite
 * pair at the top of the quadrant.
 */

#include "catch2/catch2.hpp"

#include <cmath>

#include "configuration.h"
#include "dsp/sintable.h"

using namespace baconpaul::six_sines;

TEST_CASE("The table grid is spaced one 16384th of a cycle", "[sintable]")
{
    SinTable::initializeStatics();

    for (int q = 0; q < (int)SinTable::nQuadrants; ++q)
    {
        for (size_t i = 0; i <= SinTable::nPoints; ++i)
        {
            auto expect = 0.25 * q + 1.0 * i / (SinTable::nQuadrants * SinTable::nPoints);
            INFO("quadrant " << q << " index " << i);
            REQUIRE_THAT(SinTable::xTable[q][i], Catch::Matchers::WithinAbs(expect, 1e-12));
        }
    }
}

TEST_CASE("The quadrant overlap point is the next quadrant's first point", "[sintable]")
{
    SinTable::initializeStatics();

    // Without this the waveform is stretched by nPoints/(nPoints-1) inside each quadrant and
    // then snaps back at the seam, which is an audible 0.03% of a cycle.
    for (int q = 0; q + 1 < (int)SinTable::nQuadrants; ++q)
    {
        INFO("seam after quadrant " << q);
        REQUIRE_THAT(SinTable::xTable[q][SinTable::nPoints],
                     Catch::Matchers::WithinAbs(SinTable::xTable[q + 1][0], 1e-12));
        REQUIRE_THAT(
            SinTable::quadrantTable[SinTable::SIN][q][SinTable::nPoints],
            Catch::Matchers::WithinAbs(SinTable::quadrantTable[SinTable::SIN][q + 1][0], 1e-6));
    }
    REQUIRE_THAT(SinTable::xTable[SinTable::nQuadrants - 1][SinTable::nPoints],
                 Catch::Matchers::WithinAbs(1.0, 1e-12));
}

TEST_CASE("Reading the sine table gives a true sine", "[sintable]")
{
    SinTable st;
    st.setWaveForm(SinTable::SIN);

    double worst{0};
    for (int i = 0; i < 20011; ++i)
    {
        double x = 1.0 * i / 20011;
        auto ph = static_cast<uint32_t>(x * phase::phaseMaxF);
        worst = std::max(worst, std::abs(st.at(ph) - std::sin(2 * M_PI * x)));
    }
    CAPTURE(worst);
    // float storage plus the SIMD dot product is the floor here; a single harmonic over
    // 16384 points has no meaningful interpolation error of its own
    REQUIRE(worst < 1e-6);
}

TEST_CASE("The stored slope is the waveform derivative per table step", "[sintable]")
{
    SinTable::initializeStatics();

    auto step = 1.0 / (SinTable::nQuadrants * SinTable::nPoints);
    for (int q = 0; q < (int)SinTable::nQuadrants; ++q)
    {
        for (size_t i : {(size_t)0, (size_t)1, SinTable::nPoints / 2, SinTable::nPoints - 1})
        {
            auto x = SinTable::xTable[q][i];
            auto expect = 2 * M_PI * std::cos(2 * M_PI * x) * step;
            INFO("quadrant " << q << " index " << i);
            REQUIRE_THAT(SinTable::dQuadrantTable[SinTable::SIN][q][i],
                         Catch::Matchers::WithinAbs(expect, 1e-9));
        }
    }
}

TEST_CASE("The sine table has no discontinuity at a quadrant seam", "[sintable]")
{
    SinTable st;
    st.setWaveForm(SinTable::SIN);

    // walk across each seam a step at a time; a duplicated sample shows up as a flat spot
    static constexpr uint32_t oneStep{phase::phaseMax / 16384};
    for (int q = 1; q < (int)SinTable::nQuadrants; ++q)
    {
        auto seam = static_cast<uint32_t>(q) * (phase::phaseMax / 4);
        auto before = st.at(seam - 2 * oneStep);
        auto at = st.at(seam);
        auto after = st.at(seam + 2 * oneStep);
        auto d1 = at - before;
        auto d2 = after - at;
        INFO("seam at quadrant " << q << " deltas " << d1 << " " << d2);
        // the two slopes across the seam must agree; at a sine's peak both are near zero,
        // so compare against the local scale rather than demanding a ratio
        REQUIRE_THAT(d2, Catch::Matchers::WithinAbs(d1, 2e-4));
    }
}
