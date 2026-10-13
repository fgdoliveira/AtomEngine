// The level format's authoring errors and tools. The apps' shipped files
// (their schemas, the demo's dialogue and lightmaps): their own tests.
#include "Level/FileWatcher.h"
#include "Level/LevelData.h"

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace AtomFramework; // v0.0.14: the world layer

namespace
{
    constexpr const char* Base = R"({ "name": "x", "model": "m", "collision": "c", )";

    std::string Level(const std::string& rest)
    {
        return std::string(Base) + rest + " }";
    }
}

TEST_CASE("Syntax errors say which line and column")
{
    const auto result = ParseLevel("{\n  \"name\": \"x\",\n  \"model\" \"m\"\n}");
    REQUIRE_FALSE(result.level.has_value());
    INFO(result.error);
    CHECK(result.error.rfind("line 3, column", 0) == 0);
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

TEST_CASE("Where a position came from is kept, and a marker the file overrides is a warning (M111)")
{
    const std::string level = Level(R"("spawns": { "gate": {}, "fixed": { "position": [1,2,3] } },
        "entities": [ { "name": "mill" }, { "name": "sign", "position": [9,0,9], "yaw": 5 },
                      { "name": "rock", "position": [2,0,2] } ])");
    const std::string markers = R"({
        "spawns": { "gate": { "position": [0,0,18], "yaw": 90 }, "fixed": { "position": [5,5,5], "yaw": 10 },
                    "extra": { "position": [7,0,7] } },
        "entities": { "mill": { "position": [0,0,-12] }, "sign": { "position": [1,1,1], "yaw": 60 },
                      "rock": { "position": [2,0,2] } } })";
    const auto result = ParseLevel(level, markers);
    INFO(result.error);
    REQUIRE(result.level.has_value());
    const LevelData& data = *result.level;

    CHECK(data.FindSpawn("gate")->placement == PlacementSource::Marker);
    CHECK(data.FindSpawn("fixed")->placement == PlacementSource::FileOverMarker);
    CHECK(data.FindSpawn("extra")->placement == PlacementSource::Marker); // a placement-only spawn
    CHECK(data.entities[0].placement == PlacementSource::Marker);
    CHECK(data.entities[1].placement == PlacementSource::FileOverMarker);
    CHECK(data.entities[2].placement == PlacementSource::FileOverMarker); // even at the same spot
    CHECK(std::string(PlacementSourceName(PlacementSource::FileOverMarker)) == "file (marker ignored)");

    // One warning per overridden marker, at the value that wins, naming the marker.
    std::vector<std::string> paths;
    for (const LevelWarning& warning : data.warnings)
    {
        paths.push_back(warning.path);
        CHECK(warning.message.find(" marker") != std::string::npos);
    }
    CHECK(paths == std::vector<std::string>{ "/spawns/fixed/position", "/entities/1/position", "/entities/2/position" });
    CHECK(data.warnings[1].message.find("entity:sign") != std::string::npos);

    // Marker-placed with a yaw written in the file: the file's yaw wins, and says so.
    const auto yaw = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "entities": [ { "name": "mill", "yaw": 45 } ])"), R"({ "entities": { "mill": { "position": [1,0,1], "yaw": 90 } } })");
    REQUIRE(yaw.level.has_value());
    CHECK(yaw.level->entities[0].yawDegrees == doctest::Approx(45.0f));
    REQUIRE(yaw.level->warnings.size() == 1);
    CHECK(yaw.level->warnings[0].path == "/entities/0/yaw");

    // No markers file: every position is the file's, no warning - unless
    // something drawn or solid was left at the origin.
    const auto plain = ParseLevel(Level(R"("spawns": { "a": { "position": [0,0,0] } },
        "entities": [ { "name": "note" }, { "name": "crate", "model": "Kit/crate.glb" } ])"));
    REQUIRE(plain.level.has_value());
    CHECK(plain.level->entities[0].placement == PlacementSource::File);
    REQUIRE(plain.level->warnings.size() == 1);
    CHECK(plain.level->warnings[0].path == "/entities/1");
    CHECK(plain.level->warnings[0].message.find("origin") != std::string::npos);
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
