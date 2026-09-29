#include "Level/LevelData.h"
#include "World/Impostors.h"

#include <doctest/doctest.h>

#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include <fstream>
#include <sstream>
#include <string>

using namespace AtomGame;

namespace
{
    constexpr float Step = glm::two_pi<float>() / 8.0f; // 45 degrees
    const float Hysteresis = glm::radians(7.5f);
}

TEST_CASE("Impostor views: the nearest direction, both ways round")
{
    CHECK(SelectImpostorView(-1, 0.0f, 8, Hysteresis) == 0);
    CHECK(SelectImpostorView(-1, Step * 2.0f, 8, Hysteresis) == 2);         // from +X
    CHECK(SelectImpostorView(-1, -Step, 8, Hysteresis) == 7);               // negative angles wrap
    CHECK(SelectImpostorView(-1, glm::two_pi<float>() + Step, 8, Hysteresis) == 1);
    CHECK(SelectImpostorView(-1, Step * 0.49f, 8, Hysteresis) == 0);
    CHECK(SelectImpostorView(-1, Step * 0.51f, 8, Hysteresis) == 1);
}

TEST_CASE("Impostor views: hysteresis keeps the current view near a boundary")
{
    // Just past the boundary between 0 and 1: without hysteresis it's 1.
    const float justPast = Step * 0.5f + glm::radians(3.0f);
    CHECK(SelectImpostorView(-1, justPast, 8, Hysteresis) == 1);
    CHECK(SelectImpostorView(0, justPast, 8, Hysteresis) == 0);          // stays
    CHECK(SelectImpostorView(0, Step * 0.5f + Hysteresis * 1.2f, 8, Hysteresis) == 1); // clearly past
    // Swinging back and forth across the boundary never flips while inside the margin.
    int view = 0;
    for (int i = 0; i < 20; ++i)
    {
        const float angle = Step * 0.5f + (i % 2 ? 1.0f : -1.0f) * glm::radians(5.0f);
        view = SelectImpostorView(view, angle, 8, Hysteresis);
        CHECK(view == 0);
    }
}

TEST_CASE("The shipped impostor descriptor parses; broken ones don't")
{
    std::ifstream file(ATOM_SOURCE_DIR "/Assets/City/tower_impostor.json");
    std::stringstream text;
    text << file.rdbuf();
    std::string error;
    const auto descriptor = ParseImpostorDescriptor(text.str(), error);
    INFO(error);
    REQUIRE(descriptor.has_value());
    CHECK(descriptor->views == 8);
    CHECK(descriptor->height > descriptor->width);
    CHECK(descriptor->fog < 1.0f);

    CHECK_FALSE(ParseImpostorDescriptor(R"({ "atlas": "a.png", "views": 0, "width": 1, "height": 1 })", error));
    CHECK_FALSE(ParseImpostorDescriptor(R"({ "views": 8 )", error));
    CHECK(error.rfind("line 1", 0) == 0);
}

TEST_CASE("Levels list impostors with a layer; mid and far only")
{
    const auto result = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } },
        "impostors": [ { "set": "City/t.json", "position": [100,0,0], "yaw": 90 },
                       { "set": "City/t.json", "position": [60,0,0], "layer": "mid" } ] })");
    INFO(result.error);
    REQUIRE(result.level.has_value());
    REQUIRE(result.level->impostors.size() == 2);
    CHECK(result.level->impostors[0].layer == ChunkLayer::Far);
    CHECK(result.level->impostors[1].layer == ChunkLayer::Mid);

    const auto near = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } },
        "impostors": [ { "set": "t.json", "position": [1,0,0], "layer": "near" } ] })");
    CHECK(near.error.rfind("/impostors/0/layer:", 0) == 0);
}
