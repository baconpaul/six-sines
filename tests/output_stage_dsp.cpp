/*
 * Output-stage DSP regression tests. Pin numeric output of the saturator
 * shapers and the ZOH bit-rate decimator so they don't drift.
 */

#include "catch2/catch2.hpp"
#include "synth/synth.h"

#include <array>
#include <cmath>

using baconpaul::six_sines::Synth;

TEST_CASE("softSaturator golden", "[output_stage]")
{
    // Padé-style x*(27+x²)/(27+9x²) clamped to [-4, 4].
    // Smooth, monotonic, with y(0)=0 and asymptotic ~1 inside the clamp.
    SECTION("zero in, zero out") { REQUIRE(Synth::softSaturator(0.f) == 0.f); }

    SECTION("odd symmetry")
    {
        for (float x : {0.1f, 0.5f, 1.f, 2.f, 3.5f})
            REQUIRE(Synth::softSaturator(-x) == Approx(-Synth::softSaturator(x)));
    }

    SECTION("clamp at +/- 4")
    {
        // Beyond +-4 the input is clamped, so output equals the boundary value.
        auto top = Synth::softSaturator(4.f);
        REQUIRE(Synth::softSaturator(10.f) == top);
        REQUIRE(Synth::softSaturator(-10.f) == -top);
    }

    SECTION("known anchors")
    {
        // 0.5 * (27 + 0.25) / (27 + 2.25) = 13.625 / 29.25
        REQUIRE(Synth::softSaturator(0.5f) == Approx(13.625f / 29.25f).margin(1e-6));
        // 1 * 28 / 36 = 7/9
        REQUIRE(Synth::softSaturator(1.f) == Approx(7.f / 9.f).margin(1e-6));
        // 2 * 31 / 63 = 62/63
        REQUIRE(Synth::softSaturator(2.f) == Approx(62.f / 63.f).margin(1e-6));
        // At |x|=4 the rational = 4*43/171 = 172/171
        REQUIRE(Synth::softSaturator(4.f) == Approx(172.f / 171.f).margin(1e-6));
    }
}

TEST_CASE("ojdSaturator golden", "[output_stage]")
{
    // Multi-region polynomial. Verify the region boundaries are continuous
    // and the rails saturate correctly.
    SECTION("hard rails") { REQUIRE(Synth::ojdSaturator(-2.f) == -1.f); }
    SECTION("hard rail high") { REQUIRE(Synth::ojdSaturator(2.f) == 1.f); }

    SECTION("identity in the middle region")
    {
        for (float x : {-0.3f, -0.2f, 0.f, 0.4f, 0.9f})
            REQUIRE(Synth::ojdSaturator(x) == Approx(x));
    }

    SECTION("low-region boundary continuity")
    {
        // At x=-1.7 the low region equals the negative rail.
        REQUIRE(Synth::ojdSaturator(-1.7f) == Approx(-1.f).margin(1e-6));
    }

    SECTION("high-region boundary continuity")
    {
        // At x=1.1 the high region reaches +1.
        REQUIRE(Synth::ojdSaturator(1.1f) == Approx(1.f).margin(1e-6));
    }

    SECTION("known anchors")
    {
        // Low region: x=-1.0 → xl=-0.7, denLow=1/(4*0.7), v = (-0.7 + denLow*0.49) - 0.3
        REQUIRE(Synth::ojdSaturator(-1.0f) == Approx(-0.825f).margin(1e-6));
        // High region: x=1.0 → xh=0.1, denHigh=1/(4*0.1), v = (0.1 - denHigh*0.01) + 0.9
        REQUIRE(Synth::ojdSaturator(1.0f) == Approx(0.975f).margin(1e-6));
    }
}

TEST_CASE("ZOHRateDownsampler golden", "[output_stage]")
{
    Synth::ZOHRateDownsampler z;

    SECTION("4x downsample produces v1 v1 v1 v1 v5 v5 v5 v5 ...")
    {
        z.setRate(32000.f, 128000.f);
        z.reset();

        std::array<float, 8> in{1, 2, 3, 4, 5, 6, 7, 8};
        std::array<float, 8> rIn = in;
        std::array<float, 8> expected{1, 1, 1, 1, 5, 5, 5, 5};

        for (int i = 0; i < 8; ++i)
            z.step(in[i], rIn[i]);

        for (int i = 0; i < 8; ++i)
        {
            REQUIRE(in[i] == Approx(expected[i]));
            REQUIRE(rIn[i] == Approx(expected[i]));
        }
    }

    SECTION("rate=1 passes through")
    {
        z.setRate(48000.f, 48000.f);
        z.reset();
        std::array<float, 4> in{0.1f, 0.2f, 0.3f, 0.4f};
        std::array<float, 4> rIn = in;
        for (int i = 0; i < 4; ++i)
            z.step(in[i], rIn[i]);
        for (int i = 0; i < 4; ++i)
        {
            REQUIRE(in[i] == Approx(0.1f * (i + 1)));
        }
    }

    SECTION("non-integer ratio sample pattern")
    {
        // 3:5 ratio: rate = 0.6, ticks where phase>=1 are samples 0, 2, 4, ...
        // phase init 1.0: sample0=in0 (phase->0+0.6=0.6), sample1=last (0.6+0.6=1.2),
        // sample2=in2 (phase->0.2+0.6=0.8), sample3=last (0.8+0.6=1.4),
        // sample4=in4 (phase->0.4+0.6=1.0), sample5=in5 (1.0->0+0.6=0.6).
        z.setRate(3.f, 5.f);
        z.reset();
        std::array<float, 6> in{10, 20, 30, 40, 50, 60};
        std::array<float, 6> rIn = in;
        std::array<float, 6> expected{10, 10, 30, 30, 50, 60};
        for (int i = 0; i < 6; ++i)
            z.step(in[i], rIn[i]);
        for (int i = 0; i < 6; ++i)
            REQUIRE(in[i] == Approx(expected[i]));
    }
}
