#include "Level/LevelData.h"
#include "TestAssets.h" // v0.0.14: source assets in two roots

#include <doctest/doctest.h>

#include <filesystem>

using namespace AtomFramework; // v0.0.14: the world layer

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

    // A misspelt surface would otherwise play dirt footsteps without a word.
    const auto badSurface = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } },
        "surfaces": { "default": "dirt", "zones": [ { "min": [0,0,0], "max": [1,0,1], "surface": "tatamii" } ] } })");
    CHECK_FALSE(badSurface.level.has_value());
    CHECK(badSurface.error.find("tatamii") != std::string::npos);

    const auto badDefaultSurface = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } }, "surfaces": { "default": "gravel" } })");
    CHECK_FALSE(badDefaultSurface.level.has_value());
}

TEST_CASE("A lightmap is optional, and needs a texture when given")
{
    const auto withLightmap = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } },
        "lightmap": { "texture": "Interior/lm.png", "intensity": 0.8 } })");
    REQUIRE(withLightmap.level.has_value());
    REQUIRE(withLightmap.level->lightmap.has_value());
    CHECK(withLightmap.level->lightmap->texture == "Interior/lm.png");
    CHECK(withLightmap.level->lightmap->intensity == doctest::Approx(0.8f));

    const auto without = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } } })");
    REQUIRE(without.level.has_value());
    CHECK_FALSE(without.level->lightmap.has_value());

    const auto noTexture = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } }, "lightmap": { "intensity": 1 } })");
    CHECK_FALSE(noTexture.level.has_value());
    CHECK(noTexture.error.find("lightmap") != std::string::npos);
}

TEST_CASE("A lab names its subject, and entities may be scaled")
{
    const auto lab = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } },
        "lab": { "subject": "rudy", "distance": 4, "yaw": 30 },
        "entities": [ { "name": "rudy", "position": [0,0,0], "model": "r.glb", "scale": 0.8 } ] })");
    REQUIRE(lab.level.has_value());
    REQUIRE(lab.level->lab.has_value());
    CHECK(lab.level->lab->subject == "rudy");
    CHECK(lab.level->lab->distance == doctest::Approx(4.0f));
    CHECK(lab.level->lab->yawDegrees == doctest::Approx(30.0f));
    CHECK(lab.level->entities.front().scale == doctest::Approx(0.8f));

    const auto noSubject = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } }, "lab": { "subject": "nobody" } })");
    CHECK_FALSE(noSubject.level.has_value());
    CHECK(noSubject.error.find("/lab/subject") != std::string::npos);

    const auto badScale = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } },
        "entities": [ { "name": "e", "position": [0,0,0], "scale": 0 } ] })");
    CHECK_FALSE(badScale.level.has_value());
}

TEST_CASE("Dust and the flashlight's beam are per level")
{
    const auto dusty = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } },
        "particles": { "leaves": false, "fogBanks": false, "dust": true, "beam": 0.06 } })");
    REQUIRE(dusty.level.has_value());
    CHECK(dusty.level->dust);
    CHECK(dusty.level->beam == doctest::Approx(0.06f));

    const auto plain = ParseLevel(R"({ "name": "x", "model": "m", "collision": "c",
        "spawns": { "a": { "position": [0,0,0] } } })");
    REQUIRE(plain.level.has_value());
    CHECK_FALSE(plain.level->dust);
    CHECK(plain.level->beam == 0.0f);
}

TEST_CASE("Every shipped level file is valid")
{
    const std::filesystem::path folder = AtomTests::Asset("Levels");
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json"
            || entry.path().filename().string().ends_with(".markers.json"))
        {
            continue; // markers are read with their level
        }
        const LevelParseResult result = LoadLevelFile(entry.path().string());
        INFO(entry.path().filename().string() << ": " << result.error);
        CHECK(result.level.has_value());
        // Referenced files exist (a level naming a missing lightmap would
        // fail to load in the game).
        if (result.level)
        {
            CHECK(std::filesystem::exists(AtomTests::Asset(result.level->model)));
            CHECK(std::filesystem::exists(AtomTests::Asset(result.level->collision)));
            if (result.level->lightmap)
            {
                CHECK(std::filesystem::exists(AtomTests::Asset(result.level->lightmap->texture)));
            }
        }
        ++count;
    }
    CHECK(count >= 1);
}

TEST_CASE("The street level keeps its content")
{
    const LevelParseResult result = LoadLevelFile(AtomTests::Asset("Levels/street.json"));
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
