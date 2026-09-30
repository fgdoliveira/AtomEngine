#include "Dialogue/Dialogue.h"
#include "Level/FileWatcher.h"
#include "Level/LevelData.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
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
    const nlohmann::json machine = properties("machine.schema.json");

    int files = 0;
    for (const auto& [folder, schema, keys] : {
             std::tuple{ "Levels", "../Schemas/level.schema.json", &level },
             std::tuple{ "Dialogue", "../Schemas/dialogue.schema.json", &dialogue },
             std::tuple{ "Machines", "../Schemas/machine.schema.json", &machine } })
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
    CHECK(files >= 7);
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

TEST_CASE("Chunks and cells parse, with defaults by layer and checked names")
{
    const auto result = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "cells": [ { "name": "street", "min": [-10,0,-5], "max": [10,0,5], "neighbours": ["alley"] },
                   { "name": "alley", "min": [10,0,-2], "max": [20,0,2], "neighbours": ["street", "yard"] },
                   { "name": "yard", "min": [20,0,-8], "max": [30,0,8] } ],
        "chunks": [ { "name": "shops", "model": "City/shops.glb", "cell": "street" },
                    { "name": "blocks", "model": "City/blocks.glb", "layer": "mid" },
                    { "name": "skyline", "model": "City/skyline.glb", "layer": "far", "castsShadow": true } ])"));
    INFO(result.error);
    REQUIRE(result.level.has_value());
    const LevelData& data = *result.level;
    REQUIRE(data.chunks.size() == 3);
    CHECK(data.chunks[0].layer == ChunkLayer::Near);
    CHECK(data.chunks[0].castsShadow);          // near: casts by default
    CHECK(data.chunks[1].layer == ChunkLayer::Mid);
    CHECK_FALSE(data.chunks[1].castsShadow);    // mid: doesn't by default
    CHECK(data.chunks[2].castsShadow);          // unless told so
    CHECK(data.cells.size() == 3);

    const auto badLayer = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "chunks": [ { "name": "x", "model": "m", "layer": "middle" } ])"));
    CHECK(badLayer.error.rfind("/chunks/0/layer:", 0) == 0);

    const auto badCell = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "chunks": [ { "name": "x", "model": "m", "cell": "nowhere" } ])"));
    CHECK(badCell.error.rfind("/chunks/0/cell:", 0) == 0);

    const auto farInCell = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "cells": [ { "name": "c", "min": [0,0,0], "max": [1,0,1] } ],
        "chunks": [ { "name": "x", "model": "m", "layer": "far", "cell": "c" } ])"));
    CHECK(farInCell.error.rfind("/chunks/0/cell:", 0) == 0);

    const auto badNeighbour = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "cells": [ { "name": "c", "min": [0,0,0], "max": [1,0,1], "neighbours": ["d"] } ])"));
    CHECK(badNeighbour.error.rfind("/cells/0/neighbours/0:", 0) == 0);
}

TEST_CASE("Visible cells: the player's cell and its neighbours, or everything when lost")
{
    std::vector<CellData> cells{
        { "street", { -10, -5 }, { 10, 5 }, { "alley" } },
        { "alley", { 10, -2 }, { 20, 2 }, { "street", "yard" } },
        { "yard", { 20, -8 }, { 30, 8 }, { "alley" } },
    };
    const auto inStreet = VisibleCells(cells, { 0, 0, 0 });
    CHECK(inStreet == std::vector<bool>{ true, true, false }); // the yard is behind the alley

    const auto inAlley = VisibleCells(cells, { 15, 0, 0 });
    CHECK(inAlley == std::vector<bool>{ true, true, true });

    const auto outside = VisibleCells(cells, { 100, 0, 100 });
    CHECK(outside == std::vector<bool>{ true, true, true });

    CHECK(VisibleCells({}, { 0, 0, 0 }).empty());
}

TEST_CASE("Night settings parse: sky, halos and glow, with checked values")
{
    const auto result = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "lighting": { "glow": { "strength": 0.9, "threshold": 0.6 } },
        "sky": { "panorama": "Sky/night_sky.png", "intensity": 0.8 },
        "halos": [ { "position": [1,4,2], "size": 1.5, "color": [1,0.8,0.5], "intensity": 0.3, "flicker": 0.5 },
                   { "position": [0,1,0] } ])"));
    INFO(result.error);
    REQUIRE(result.level.has_value());
    const LevelData& data = *result.level;
    CHECK(data.lighting.glowStrength == doctest::Approx(0.9f));
    CHECK(data.lighting.glowThreshold == doctest::Approx(0.6f));
    REQUIRE(data.sky.has_value());
    CHECK(data.sky->panorama == "Sky/night_sky.png");
    REQUIRE(data.halos.size() == 2);
    CHECK(data.halos[0].flicker == doctest::Approx(0.5f));
    CHECK(data.halos[1].size == doctest::Approx(1.0f)); // defaults

    const auto day = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } })"));
    REQUIRE(day.level.has_value());
    CHECK_FALSE(day.level->sky.has_value());
    CHECK(day.level->lighting.glowStrength > 0.0f); // gentle glow by default

    const auto noPosition = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "halos": [ { "size": 1 } ])"));
    CHECK(noPosition.error.rfind("/halos/0:", 0) == 0);

    const auto badGlow = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "lighting": { "glow": { "threshold": 0 } })"));
    CHECK(badGlow.error.rfind("/lighting/glow:", 0) == 0);
}

TEST_CASE("Committed lightmaps were baked on the CPU (byte-identical builds)")
{
    // build_assets --gpu marks its lightmaps: fast to iterate with, but not
    // reproducible, so they must be rebuilt on the CPU before committing.
    int lightmaps = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(Assets))
    {
        const std::string name = entry.path().filename().string();
        if (!name.ends_with("_lm.png"))
        {
            continue;
        }
        ++lightmaps;
        std::ifstream file(entry.path(), std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        INFO(name);
        CHECK(bytes.find("atom-gpu-bake") == std::string::npos);
    }
    CHECK(lightmaps >= 6);
}
