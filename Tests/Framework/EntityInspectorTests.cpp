#include "Debug/EntityInspector.h"

#include "Level/LevelData.h"
#include "World/GameWorld.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace AtomFramework;

namespace
{
    LevelData Parse(const char* json, const char* markers = "")
    {
        LevelParseResult result = ParseLevel(json, markers);
        INFO(result.error);
        REQUIRE(result.level.has_value());
        return std::move(*result.level);
    }

    const char* Street = R"({ "name": "street", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } },
        "entities": [ { "name": "Lamp_09", "position": [1,0,2], "model": "Kit/lamp.glb",
                        "collider": { "center": [0,1,0], "halfExtents": [0.2,1,0.2] } },
                      { "name": "door", "position": [0,0,-4],
                        "interactable": { "prompt": "Leave", "action": { "type": "changeLevel", "level": "shrine", "spawn": "gate" } } } ] })";

    bool Has(const std::vector<std::string>& lines, const std::string& words)
    {
        return std::any_of(lines.begin(), lines.end(), [&](const std::string& l) { return l.find(words) != std::string::npos; });
    }
}

TEST_CASE("Entity report (M114): a selection is a name, resolved again against each load")
{
    const LevelData first = Parse(Street);
    EntityInspector inspector;
    CHECK(inspector.Resolve("street", first) == EntityInspector::npos); // nothing picked

    inspector.Select("street", "door");
    CHECK(inspector.Resolve("street", first) == 1);

    // A reload builds new data: the same name is found in it, nothing old is kept.
    const LevelData reloaded = Parse(Street);
    CHECK(inspector.Resolve("street", reloaded) == 1);

    // Reloaded without the entity: the selection goes.
    const LevelData without = Parse(R"({ "name": "street", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } }, "entities": [ { "name": "Lamp_09", "position": [1,0,2] } ] })");
    CHECK(inspector.Resolve("street", without) == EntityInspector::npos);
    CHECK_FALSE(inspector.HasSelection());

    // Another level - even with an entity of that name - clears it.
    inspector.Select("street", "Lamp_09");
    CHECK(inspector.Resolve("shrine", first) == EntityInspector::npos);
    CHECK_FALSE(inspector.HasSelection());
}

TEST_CASE("Entity report: the filter matches any part of the name, any case")
{
    CHECK(EntityInspector::Matches("Lamp_09", ""));
    CHECK(EntityInspector::Matches("Lamp_09", "lamp"));
    CHECK(EntityInspector::Matches("Lamp_09", "P_0"));
    CHECK_FALSE(EntityInspector::Matches("Lamp_09", "door"));
}

TEST_CASE("Entity report: what it shows, from the level's data and the live entity")
{
    const LevelData data = Parse(Street);
    Entity live;
    live.name = "door";
    live.position = { 0.0f, 0.0f, -4.0f };
    live.hidden = true;

    const std::vector<std::string> lamp = EntityInspector::Describe(data, 0, nullptr);
    CHECK(Has(lamp, "json: /entities/0"));
    CHECK(Has(lamp, "placed by: file"));
    CHECK(Has(lamp, "model: Kit/lamp.glb"));
    CHECK(Has(lamp, "collider: centre 0.00 1.00 0.00, half 0.20 1.00 0.20"));
    CHECK(Has(lamp, "now: not in the world"));

    const std::vector<std::string> door = EntityInspector::Describe(data, 1, &live);
    CHECK(Has(door, "model: none, collider: none - a point"));
    CHECK(Has(door, "interactable: \"Leave\" -> changeLevel shrine : gate"));
    CHECK(Has(door, "look at: 0.00 1.20 -4.00 (focus 0.00 1.20 0.00), within 2.20 m")); // the defaults
    CHECK(Has(door, "now: at 0.00 0.00 -4.00, hidden"));
    CHECK(EntityInspector::Describe(data, 7, nullptr).empty());
}

TEST_CASE("Entity report: a marker the file overrides shows on that entity, and only there")
{
    const LevelData data = Parse(R"({ "name": "s", "model": "m", "collision": "c", "spawns": { "a": { "position": [0,0,0] } },
        "entities": [ { "name": "mill", "position": [1,0,1] }, { "name": "sign" }, { "name": "rock", "position": [2,0,2] } ] })",
        R"({ "entities": { "mill": { "position": [0,0,0] }, "sign": { "position": [3,0,3] } } })");
    REQUIRE(EntityInspector::WarningsFor(data, 0).size() == 1);
    CHECK(EntityInspector::WarningsFor(data, 0)[0]->path == "/entities/0/position");
    CHECK(EntityInspector::WarningsFor(data, 1).empty());
    CHECK(EntityInspector::WarningsFor(data, 2).empty());
    CHECK(Has(EntityInspector::Describe(data, 0, nullptr), "placed by: file (marker ignored)"));
    CHECK(Has(EntityInspector::Describe(data, 1, nullptr), "placed by: marker"));
}
