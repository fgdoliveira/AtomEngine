#include "Level/LevelData.h"

#include <doctest/doctest.h>

#include <filesystem>

using namespace AtomGame;

namespace
{
    constexpr const char* Minimal = R"({
        "name": "test",
        "model": "Street/street.glb",
        "collision": "Street/street_col.glb",
        "spawns": {
            "a": { "position": [1, 0, 2], "yaw": 90 },
            "b": { "position": [3, 0, 4] }
        },
        "defaultSpawn": "b",
        "surfaces": {
            "default": "dirt",
            "zones": [
                { "min": [0, 0, 0], "max": [1, 0, 1], "surface": "stone" },
                { "min": [0, 0, 0], "max": [5, 0, 5], "surface": "asphalt" }
            ]
        },
        "entities": [
            { "name": "gate", "position": [0, 0, 5], "yaw": 90,
              "collider": { "center": [0, 1, 0], "halfExtents": [1.5, 1, 0.2] },
              "interactable": {
                "prompt": "Open", "requires": "permission",
                "action": { "type": "changeLevel", "level": "shrine", "spawn": "gate" },
                "locked": { "type": "message", "text": "It's locked." }
              } },
            { "name": "note", "interactable": { "action": { "type": "setFlag", "flag": "read" } } },
            { "name": "keeper", "interactable": { "action": { "type": "dialogue", "id": "keeper" } } }
        ]
    })";

    LevelData ParseOrFail(const char* json)
    {
        LevelParseResult result = ParseLevel(json);
        INFO(result.error);
        REQUIRE(result.level.has_value());
        return std::move(*result.level);
    }
}

TEST_CASE("A level parses its spawns, surfaces and entities")
{
    const LevelData level = ParseOrFail(Minimal);
    CHECK(level.name == "test");
    CHECK(level.defaultSpawn == "b");
    REQUIRE(level.FindSpawn("a") != nullptr);
    CHECK(level.FindSpawn("a")->yawDegrees == doctest::Approx(90.0f));
    CHECK(level.FindSpawn("nope") == nullptr);
    CHECK(level.entities.size() == 3);
}

TEST_CASE("Actions of every type come out of level data")
{
    const LevelData level = ParseOrFail(Minimal);
    const Interactable& gate = *level.entities[0].interactable;
    REQUIRE(std::holds_alternative<ChangeLevel>(gate.action));
    CHECK(std::get<ChangeLevel>(gate.action).level == "shrine");
    CHECK(gate.requiresFlag == "permission");
    REQUIRE(gate.lockedAction.has_value());
    CHECK(std::holds_alternative<ShowMessage>(*gate.lockedAction));

    CHECK(std::holds_alternative<SetFlag>(level.entities[1].interactable->action));
    CHECK(std::holds_alternative<StartDialogue>(level.entities[2].interactable->action));

    REQUIRE(level.entities[0].collider.has_value());
    CHECK(level.entities[0].collider->halfExtents.x == doctest::Approx(1.5f));
}

TEST_CASE("Surface zones match in order, with a default")
{
    const LevelData level = ParseOrFail(Minimal);
    CHECK(level.SurfaceAt(0.5f, 0.5f) == "stone");   // first zone wins
    CHECK(level.SurfaceAt(3.0f, 3.0f) == "asphalt");
    CHECK(level.SurfaceAt(9.0f, 9.0f) == "dirt");
}

TEST_CASE("Broken level data is rejected with a reason")
{
    CHECK_FALSE(ParseLevel("{").level.has_value());
    CHECK_FALSE(ParseLevel(R"({ "name": "x" })").level.has_value());

    const auto noSpawn = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c", "spawns": {} })");
    CHECK_FALSE(noSpawn.level.has_value());

    const auto badDefault = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } }, "defaultSpawn": "zzz" })");
    CHECK_FALSE(badDefault.level.has_value());
    CHECK(badDefault.error.find("zzz") != std::string::npos);

    const auto badAction = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } },
        "entities": [ { "name": "e", "interactable": { "action": { "type": "explode" } } } ] })");
    CHECK_FALSE(badAction.level.has_value());
    CHECK(badAction.error.find("explode") != std::string::npos);

    const auto badVector = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0, 0] } } })");
    CHECK_FALSE(badVector.level.has_value());
}

TEST_CASE("Every shipped level file is valid")
{
    const std::filesystem::path folder = ATOM_SOURCE_DIR "/Assets/Levels";
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json")
        {
            continue;
        }
        const LevelParseResult result = LoadLevelFile(entry.path().string());
        INFO(entry.path().filename().string() << ": " << result.error);
        CHECK(result.level.has_value());
        ++count;
    }
    CHECK(count >= 1);
}

TEST_CASE("The street level keeps its content")
{
    const LevelParseResult result = LoadLevelFile(ATOM_SOURCE_DIR "/Assets/Levels/street.json");
    REQUIRE(result.level.has_value());
    const LevelData& street = *result.level;

    bool keeper = false;
    for (const EntityData& entity : street.entities)
    {
        keeper = keeper || (entity.name == "shrine_keeper" && entity.model == "Kit/keeper.glb");
    }
    CHECK(keeper);
    CHECK(street.emitters.size() == 3);
    CHECK(street.unease.figureSpots.size() == 6);
}
