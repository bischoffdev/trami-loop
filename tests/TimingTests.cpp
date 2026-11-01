#include "model/Timing.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using trami::Timing;

TEST_CASE("Samples per step and bar at 120 BPM / 48 kHz")
{
    const Timing t(120.0, 48000.0);
    REQUIRE(t.samplesPerStep() == Catch::Approx(6000.0));
    REQUIRE(t.samplesPerBar() == Catch::Approx(96000.0));
}

TEST_CASE("Straight timing places steps on exact samples")
{
    const Timing t(120.0, 48000.0);
    REQUIRE(t.stepToSample(0) == 0);
    REQUIRE(t.stepToSample(1) == 6000);
    REQUIRE(t.stepToSample(16) == 96000);
}

TEST_CASE("Swing delays only odd steps")
{
    const Timing t(120.0, 48000.0, 0.5);
    REQUIRE(t.stepToSample(0) == 0);
    REQUIRE(t.stepToSample(1) == 6000 + 1500);
    REQUIRE(t.stepToSample(2) == 12000);
}

TEST_CASE("Swing is clamped to [0, 1]")
{
    REQUIRE(Timing(120.0, 48000.0, 3.0).swing() == 1.0);
    REQUIRE(Timing(120.0, 48000.0, -1.0).swing() == 0.0);
}

TEST_CASE("Loop BPM is derived from duration and bars")
{
    REQUIRE(Timing::loopBpm(2.0, 1) == Catch::Approx(120.0));
    REQUIRE(Timing::loopBpm(8.0, 4) == Catch::Approx(120.0));
    REQUIRE(Timing::loopBpm(0.0, 4) == 0.0);
}
