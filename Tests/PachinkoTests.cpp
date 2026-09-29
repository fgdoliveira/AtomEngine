#include "Audio/Reverb.h"
#include "Level/LevelData.h"
#include "World/FixedStep.h"
#include "World/PachinkoAttract.h"

#include <doctest/doctest.h>

#include <cmath>
#include <string>

using namespace AtomGame;

TEST_CASE("Fixed step: whole steps from any frame rate, leftovers carried, hitches capped")
{
    FixedStep clock;
    int steps = 0;
    for (int frame = 0; frame < 144; ++frame)
    {
        steps += clock.Advance(1.0f / 144.0f); // one second at 144 fps
    }
    CHECK(steps == doctest::Approx(60).epsilon(0.02));

    FixedStep slow;
    steps = 0;
    for (int frame = 0; frame < 30; ++frame)
    {
        steps += slow.Advance(1.0f / 30.0f); // one second at 30 fps
    }
    CHECK(steps == doctest::Approx(60).epsilon(0.02));

    FixedStep hitch;
    CHECK(hitch.Advance(2.0f) == hitch.maxSteps); // a two-second stall
    CHECK(hitch.accumulator <= hitch.step);       // the backlog is dropped
    CHECK(hitch.Advance(0.0f) <= 1);
}

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

TEST_CASE("Reverb: silent when off, rings after a click, decays")
{
    Atom::Reverb off;
    off.Configure(0.0f, 1.0f, 0.5f, 48000);
    float l = 0.5f, r = 0.5f;
    off.Process(l, r);
    CHECK(l == 0.5f);

    Atom::Reverb hall;
    hall.Configure(0.4f, 1.5f, 0.7f, 48000);
    float energyEarly = 0.0f, energyLate = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        float left = i == 0 ? 1.0f : 0.0f;
        float right = left;
        hall.Process(left, right);
        const float e = left * left + right * right;
        if (i > 1000 && i < 5000) energyEarly += e;
        if (i > 40000) energyLate += e;
        REQUIRE(std::isfinite(left));
    }
    CHECK(energyEarly > 0.0f);           // a tail after the click
    CHECK(energyLate < energyEarly * 0.1f); // and it dies away
}

TEST_CASE("Screens and reverb parse from the level")
{
    const auto result = ParseLevel(R"({ "name": "t", "model": "m.glb", "collision": "c.glb",
        "spawns": { "a": { "position": [0,0,0] } },
        "screens": [ { "material": "atom_pachinko_screen", "seed": 2 } ],
        "audio": { "reverb": { "mix": 0.3, "size": 1.4, "feedback": 0.6 } } })");
    INFO(result.error);
    REQUIRE(result.level.has_value());
    REQUIRE(result.level->screens.size() == 1);
    CHECK(result.level->screens[0].seed == 2);
    CHECK(result.level->reverb.mix == doctest::Approx(0.3f));

    const auto bad = ParseLevel(R"({ "name": "t", "model": "m.glb", "collision": "c.glb",
        "spawns": { "a": { "position": [0,0,0] } }, "audio": { "reverb": { "feedback": 1.2 } } })");
    CHECK(bad.error.rfind("/audio/reverb:", 0) == 0);
}
