#include "Level/LevelData.h"
#include "World/LiveEffects.h"

#include <doctest/doctest.h>

#include <string>

using namespace AtomFramework; // v0.0.14: the world layer

namespace
{
    std::string Level(const std::string& body)
    {
        return R"({ "name": "t", "model": "m.glb", "collision": "c.glb", )" + body + "}";
    }

    const std::string Spawn = R"("spawns": { "a": { "position": [0,0,0] } })";
}

TEST_CASE("Movers wait, travel, wait at the far end and come back")
{
    // travel 10 s, wait 5 s: a leg is 15 s, a full cycle 30 s.
    CHECK(EvaluateMover(10.0f, 5.0f, 0.0f).progress == doctest::Approx(0.0f));
    CHECK_FALSE(EvaluateMover(10.0f, 5.0f, 4.9f).moving);
    CHECK(EvaluateMover(10.0f, 5.0f, 10.0f).progress == doctest::Approx(0.5f));
    CHECK(EvaluateMover(10.0f, 5.0f, 10.0f).moving);
    CHECK(EvaluateMover(10.0f, 5.0f, 17.0f).progress == doctest::Approx(1.0f));
    CHECK_FALSE(EvaluateMover(10.0f, 5.0f, 17.0f).moving);
    CHECK(EvaluateMover(10.0f, 5.0f, 25.0f).progress == doctest::Approx(0.5f)); // on its way back
    CHECK(EvaluateMover(10.0f, 5.0f, 30.0f).progress == doctest::Approx(0.0f)); // round again
}

TEST_CASE("Flicker: steady at zero, same place stutters the same, mostly on")
{
    const glm::vec3 sign{ 8.0f, 5.0f, 4.35f };
    int dropped = 0;
    for (int frame = 0; frame < 6000; ++frame)
    {
        const float t = frame / 60.0f;
        CHECK(FlickerFactor(t, sign, 0.0f) == 1.0f);
        const float a = FlickerFactor(t, sign, 0.8f);
        CHECK(a == FlickerFactor(t, sign, 0.8f)); // a halo and a light at the sign agree
        dropped += a < 1.0f;
    }
    CHECK(dropped > 0);
    CHECK(dropped < 6000 / 5); // short drops, not a strobe
}

TEST_CASE("Crossfade steps linearly and stops at the target")
{
    CHECK(StepTowards(0.0f, 1.0f, 0.5f, 2.0f) == doctest::Approx(0.25f));
    CHECK(StepTowards(0.9f, 1.0f, 0.5f, 2.0f) == doctest::Approx(1.0f));
    CHECK(StepTowards(1.0f, 0.0f, 0.5f, 2.0f) == doctest::Approx(0.75f));
    CHECK(StepTowards(0.3f, 1.0f, 0.1f, 0.0f) == 1.0f);
}

TEST_CASE("Cell lookup finds the first containing cell or -1")
{
    const std::vector<CellData> cells{
        { "street", { -10, -5 }, { 10, 5 }, {} },
        { "alley", { 4, -20 }, { 7, -5 }, {} },
    };
    CHECK(CellAt(cells, { 0, 0, 0 }) == 0);
    CHECK(CellAt(cells, { 5, 0, -12 }) == 1);
    CHECK(CellAt(cells, { 50, 0, 0 }) == -1);
}

TEST_CASE("Night street data parses: lights, movers, zones, chunk lightmaps; names are checked")
{
    const auto result = ParseLevel(Level(Spawn + R"(,
        "cells": [ { "name": "street", "min": [-10,0,-5], "max": [10,0,5] } ],
        "chunks": [ { "name": "s", "model": "s.glb", "cell": "street", "lightmap": "s_lm.png" } ],
        "audio": { "zones": [ { "cell": "street", "beds": [ { "sound": "traffic", "gain": 0.3 } ] } ], "zoneFadeSeconds": 1.5 },
        "lights": [ { "position": [0,2,0], "radius": 20, "entity": "train", "color": [0.8,0.9,1] },
                    { "position": [8,5,4], "radius": 7, "flicker": 0.8, "material": "atom_neon_amber" } ],
        "halos": [ { "position": [0,1.9,-18], "entity": "train" } ],
        "entities": [ { "name": "train", "position": [-18,8,-150],
                        "mover": { "travel": [0,0,300], "travelSeconds": 14, "sound": "train" } } ])"));
    INFO(result.error);
    REQUIRE(result.level.has_value());
    const LevelData& d = *result.level;
    CHECK(d.chunks[0].lightmap == "s_lm.png");
    REQUIRE(d.audioZones.size() == 1);
    CHECK(d.audioZones[0].beds[0].sound == "traffic");
    CHECK(d.zoneFadeSeconds == doctest::Approx(1.5f));
    REQUIRE(d.lights.size() == 2);
    CHECK(d.lights[0].entity == "train");
    CHECK(d.lights[1].material == "atom_neon_amber");
    CHECK(d.halos[0].entity == "train");
    REQUIRE(d.entities[0].mover.has_value());
    CHECK(d.entities[0].mover->travel.z == doctest::Approx(300.0f));
    CHECK(d.entities[0].mover->waitSeconds == doctest::Approx(20.0f)); // default

    const auto badZone = ParseLevel(Level(Spawn + R"(, "audio": { "zones": [ { "cell": "nowhere", "beds": [] } ] })"));
    CHECK(badZone.error.rfind("/audio/zones/0/cell:", 0) == 0);
    const auto badAnchor = ParseLevel(Level(Spawn + R"(, "lights": [ { "position": [0,0,0], "radius": 3, "entity": "ghost" } ])"));
    CHECK(badAnchor.error.rfind("/lights/0/entity:", 0) == 0);
    const auto noRadius = ParseLevel(Level(Spawn + R"(, "lights": [ { "position": [0,0,0], "radius": 0 } ])"));
    CHECK(noRadius.error.rfind("/lights/0:", 0) == 0);
    const auto badMover = ParseLevel(Level(Spawn + R"(, "entities": [ { "name": "x", "position": [0,0,0], "mover": { "travelSeconds": 3 } } ])"));
    CHECK(badMover.error.rfind("/entities/0/mover:", 0) == 0);
}
