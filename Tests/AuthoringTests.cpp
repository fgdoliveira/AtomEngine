#include "Dialogue/Dialogue.h"
#include "Level/FileWatcher.h"
#include "Level/LevelData.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace AtomGame;

namespace
{
    const std::string Assets = ATOM_SOURCE_DIR "/Assets/";

    constexpr const char* Base = R"({ "name": "x", "model": "m", "collision": "c", )";

    std::string Level(const std::string& rest)
    {
        return std::string(Base) + rest + " }";
    }

    nlohmann::json ReadJson(const std::string& path)
    {
        std::ifstream file(path);
        return nlohmann::json::parse(file);
    }
}

TEST_CASE("Syntax errors say which line and column")
{
    const auto result = ParseLevel("{\n  \"name\": \"x\",\n  \"model\" \"m\"\n}");
    REQUIRE_FALSE(result.level.has_value());
    INFO(result.error);
    CHECK(result.error.rfind("line 3, column", 0) == 0);

    const auto dialogue = ParseDialogue("{ \"id\": \"a\",\n\n  \"start\": }");
    INFO(dialogue.error);
    CHECK(dialogue.error.rfind("line 3, column", 0) == 0);
}

TEST_CASE("Content errors name the value by its JSON path")
{
    const auto action = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "entities": [ { "name": "e" }, { "name": "door", "interactable": { "action": { "type": "explode" } } } ])"));
    INFO(action.error);
    CHECK(action.error.rfind("/entities/1/interactable/action/type:", 0) == 0);

    // A value of the wrong type is an error now, not a silent default.
    const auto type = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "lighting": { "bakedLight": "high" })"));
    INFO(type.error);
    CHECK(type.error.rfind("/lighting/bakedLight: must be a number", 0) == 0);

    const auto zone = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "surfaces": { "zones": [ { "min": [0,0,0], "max": [1,0,1], "surface": "dirt" },
                                 { "min": [0,0], "max": [1,0,1], "surface": "dirt" } ] })"));
    INFO(zone.error);
    CHECK(zone.error.rfind("/surfaces/zones/1/min: must be [x, y, z]", 0) == 0);

    const auto link = ParseDialogue(R"({ "id": "d", "start": "a", "nodes": [
        { "id": "a", "text": "hi", "next": "b" },
        { "id": "b", "text": "?", "choices": [ { "text": "ok", "next": "end" }, { "text": "x", "next": "typo" } ] } ] })");
    INFO(link.error);
    CHECK(link.error.rfind("/nodes/1/choices/1/next:", 0) == 0);
}

TEST_CASE("Markers place spawns and entities; the level file still wins")
{
    const std::string level = Level(R"("spawns": { "gate": {}, "fixed": { "position": [1,2,3], "yaw": 45 } },
        "entities": [ { "name": "mill" }, { "name": "sign", "position": [9,0,9] } ])");
    const std::string markers = R"({
        "spawns": { "gate": { "position": [0,0,18], "yaw": 90 }, "fixed": { "position": [5,5,5], "yaw": 10 },
                    "extra": { "position": [7,0,7], "yaw": 0 } },
        "entities": { "mill": { "position": [0,0,-12], "yaw": 30 }, "sign": { "position": [1,1,1], "yaw": 60 } } })";
    const auto result = ParseLevel(level, markers);
    INFO(result.error);
    REQUIRE(result.level.has_value());
    const LevelData& data = *result.level;

    CHECK(data.FindSpawn("gate")->position.z == doctest::Approx(18.0f));
    CHECK(data.FindSpawn("gate")->yawDegrees == doctest::Approx(90.0f));
    CHECK(data.FindSpawn("fixed")->position.x == doctest::Approx(1.0f)); // written in the file
    CHECK(data.FindSpawn("fixed")->yawDegrees == doctest::Approx(45.0f));
    CHECK(data.FindSpawn("extra") != nullptr);                          // placement-only spawn
    CHECK(data.entities[0].position.z == doctest::Approx(-12.0f));
    CHECK(data.entities[0].yawDegrees == doctest::Approx(30.0f));
    CHECK(data.entities[1].position.x == doctest::Approx(9.0f));
    CHECK(data.entities[1].yawDegrees == doctest::Approx(60.0f));        // yaw not written: marker's

    const auto stray = ParseLevel(level, R"({ "spawns": { "gate": { "position": [0,0,0] } },
        "entities": { "mill": { "position": [0,0,0] }, "windmil": { "position": [0,0,0] } } })");
    INFO(stray.error);
    CHECK(stray.error.find("windmil") != std::string::npos);

    const auto unplaced = ParseLevel(level, R"({ "spawns": { "gate": { "position": [0,0,0] } } })");
    INFO(unplaced.error);
    CHECK(unplaced.error.rfind("/entities/0:", 0) == 0);

    const auto noMarkers = ParseLevel(level);
    INFO(noMarkers.error);
    CHECK(noMarkers.error.rfind("/spawns/gate:", 0) == 0);
}

TEST_CASE("Shipped levels and dialogues declare their schema, which knows their keys")
{
    const auto properties = [](const std::string& schema) {
        return ReadJson(Assets + "Schemas/" + schema)["properties"];
    };
    const nlohmann::json level = properties("level.schema.json");
    const nlohmann::json dialogue = properties("dialogue.schema.json");

    int files = 0;
    for (const auto& [folder, schema, keys] : {
             std::tuple{ "Levels", "../Schemas/level.schema.json", &level },
             std::tuple{ "Dialogue", "../Schemas/dialogue.schema.json", &dialogue } })
    {
        for (const auto& entry : std::filesystem::directory_iterator(Assets + folder))
        {
            const std::string name = entry.path().filename().string();
            if (entry.path().extension() != ".json" || name.ends_with(".markers.json"))
            {
                continue;
            }
            ++files;
            INFO(name);
            const nlohmann::json root = ReadJson(entry.path().string());
            CHECK(root.value("$schema", "") == schema);
            for (const auto& [key, value] : root.items())
            {
                INFO(key);
                CHECK(keys->contains(key)); // a key the schema doesn't know would be flagged in the editor
            }
        }
    }
    CHECK(files >= 5);
}

TEST_CASE("The file watcher reports each change once")
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "atom_watch_test.json";
    std::filesystem::remove(path);
    std::ofstream(path) << "{}";

    FileWatcher watcher;
    watcher.Watch({ path.string() });
    CHECK(watcher.Poll().empty());

    std::filesystem::last_write_time(path, std::filesystem::last_write_time(path) + std::chrono::seconds(2));
    const auto changed = watcher.Poll();
    REQUIRE(changed.size() == 1);
    CHECK(changed[0] == path.string());
    CHECK(watcher.Poll().empty()); // reported once

    std::filesystem::remove(path);
    CHECK(watcher.Poll().size() == 1); // disappearing counts
}
