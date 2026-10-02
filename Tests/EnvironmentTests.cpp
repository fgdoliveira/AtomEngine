#include "Level/Environment.h"
#include "Level/LevelData.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <string>

using namespace AtomGame;

namespace
{
    std::string Level(const std::string& lighting)
    {
        return std::string(R"({ "name": "x", "model": "m", "collision": "c",
            "spawns": { "a": { "position": [0,0,0] } }, "lighting": )") + lighting + " }";
    }

    EnvironmentState Sunny()
    {
        EnvironmentState s;
        s.sunDirection = { 0.0f, 1.0f, 0.0f };
        s.sunColor = { 1.0f, 0.9f, 0.8f };
        s.fogColor = { 0.6f, 0.7f, 0.8f };
        s.fogDensity = 0.0f;
        s.sky = SkyGradient{ { 0.2f, 0.4f, 0.8f }, { 0.7f, 0.8f, 0.9f }, 1.5f, 0.4f };
        return s;
    }

    EnvironmentState Grey()
    {
        EnvironmentState s;
        s.sunDirection = { 1.0f, 0.0f, 0.0f };
        s.sunColor = { 0.2f, 0.2f, 0.2f };
        s.fogColor = { 0.4f, 0.4f, 0.4f };
        s.fogDensity = 0.06f;
        s.sky = SkyGradient{ { 0.3f, 0.3f, 0.35f }, { 0.4f, 0.4f, 0.4f }, 0.0f, 0.0f };
        return s;
    }
}

TEST_CASE("A day sky and the level's fog parse, and the fog takes the horizon's colour")
{
    const auto result = ParseLevel(Level(R"({ "fogDensity": 0.02,
        "skyGradient": { "zenith": [0.1,0.2,0.6], "horizon": [0.5,0.6,0.7], "sunSize": 2, "sunGlow": 0.8 } })"));
    INFO(result.error);
    REQUIRE(result.level.has_value());
    const LevelLighting& l = result.level->lighting;
    REQUIRE(l.sky.has_value());
    CHECK(l.sky->zenith.z == doctest::Approx(0.6f));
    CHECK(l.sky->sunSize == doctest::Approx(2.0f));
    CHECK(l.sky->sunGlow == doctest::Approx(0.8f));
    REQUIRE(l.fogDensity.has_value());
    CHECK(*l.fogDensity == doctest::Approx(0.02f));
    CHECK(l.fogColor.x == doctest::Approx(0.5f)); // the horizon

    // A fog colour of its own wins.
    const auto own = ParseLevel(Level(R"({ "fogColor": [0.1,0.1,0.1], "skyGradient": {} })"));
    REQUIRE(own.level.has_value());
    CHECK(own.level->lighting.fogColor.x == doctest::Approx(0.1f));
    CHECK(own.level->lighting.sky->sunSize == doctest::Approx(1.5f)); // defaults

    // Levels without either are as before.
    const auto plain = ParseLevel(Level("{}"));
    REQUIRE(plain.level.has_value());
    CHECK_FALSE(plain.level->lighting.sky.has_value());
    CHECK_FALSE(plain.level->lighting.fogDensity.has_value());

    CHECK_FALSE(ParseLevel(Level(R"({ "fogDensity": -1 })")).level.has_value());
    CHECK_FALSE(ParseLevel(Level(R"({ "skyGradient": { "sunSize": 45 } })")).level.has_value());
}

TEST_CASE("Blending environments: the ends are exact, the middle is mixed")
{
    const EnvironmentState a = Sunny();
    const EnvironmentState b = Grey();

    const EnvironmentState start = Blend(a, b, 0.0f);
    CHECK(start.sunColor.x == doctest::Approx(1.0f));
    CHECK(start.sky->horizon.x == doctest::Approx(0.7f));
    const EnvironmentState end = Blend(a, b, 1.0f);
    CHECK(end.sunColor.x == doctest::Approx(0.2f));
    CHECK(*end.fogDensity == doctest::Approx(0.06f));

    const EnvironmentState middle = Blend(a, b, 0.5f);
    CHECK(middle.fogColor.x == doctest::Approx(0.5f));
    CHECK(*middle.fogDensity == doctest::Approx(0.03f));
    CHECK(middle.sky->sunGlow == doctest::Approx(0.2f));
    // The sun's direction stays a direction, halfway round.
    CHECK(glm::length(middle.sunDirection) == doctest::Approx(1.0f));
    CHECK(middle.sunDirection.x == doctest::Approx(middle.sunDirection.y));

    // Out of range clamps.
    CHECK(Blend(a, b, 2.0f).sunColor.x == doctest::Approx(0.2f));
}

TEST_CASE("Blending: what can't mix switches at the midpoint, and opposite suns don't break")
{
    EnvironmentState a = Sunny();
    EnvironmentState b = Grey();
    b.sky.reset();
    b.fogDensity.reset();
    CHECK(Blend(a, b, 0.49f).sky.has_value());
    CHECK_FALSE(Blend(a, b, 0.5f).sky.has_value());
    CHECK(Blend(a, b, 0.49f).fogDensity.has_value());
    CHECK_FALSE(Blend(a, b, 0.51f).fogDensity.has_value());

    b.sunDirection = -a.sunDirection;
    const glm::vec3 sun = Blend(a, b, 0.5f).sunDirection;
    CHECK(glm::length(sun) == doctest::Approx(1.0f));

    // Deterministic: the same inputs give the same state.
    CHECK(Blend(a, b, 0.3f).sunColor == Blend(a, b, 0.3f).sunColor);
}
