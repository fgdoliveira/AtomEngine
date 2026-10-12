#include "Pachinko/PachinkoAttract.h"

#include <doctest/doctest.h>

using namespace Demo;

TEST_CASE("Attract loop: deterministic, balls stay on the field, it scores")
{
    PachinkoAttract a(3), b(3), other(4);
    for (int tick = 0; tick < 60 * 60; ++tick) // a minute
    {
        a.Step();
        b.Step();
        other.Step();
        REQUIRE(a.BallsInBounds());
        REQUIRE(a.GetBallCount() <= 16);
    }
    CHECK(a.GetScore() == b.GetScore());
    CHECK(a.GetFinished() == b.GetFinished());
    CHECK(a.GetBallCount() == b.GetBallCount());
    CHECK(a.GetLaunched() > 100);
    CHECK(a.GetFinished() > 100);   // balls leave; nothing gets stuck
    CHECK(a.GetScore() > 0);        // at least one reaches the start pocket
    CHECK(other.GetFinished() != a.GetFinished()); // another seed plays differently
}
