#include "Level/LevelData.h"
#include "World/FixedStep.h"

#include <doctest/doctest.h>

#include <string>

using namespace AtomFramework; // v0.0.14: the world layer

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
