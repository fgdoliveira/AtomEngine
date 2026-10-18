#include "Settings/SaveGame.h"
#include "World/GameState.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace AtomFramework;

namespace
{
    SaveGame Sample()
    {
        SaveGame s;
        s.level = "street";
        s.spawn = "east_end";
        s.position = { -12.5f, 0.0f, 7.25f };
        s.yawRadians = 1.5f;
        s.pitchRadians = -0.2f;
        s.flags = { "keeper_permission", "bowed_at_torii" };
        s.counters = { { "tokens", 3 }, { "balls", 120 } };
        s.flashlightOn = true;
        s.headlightOn = false;
        s.place = "the roadside shrine";
        s.savedAt = "2026-10-18 21:05";
        return s;
    }
}

TEST_CASE("A save round-trips, and the same progress writes the same bytes")
{
    const SaveGame original = Sample();
    const std::string text = WriteSave(original);
    const SaveLoad load = ParseSave(text);
    INFO(load.warning);
    REQUIRE(load.found);
    CHECK(load.warning.empty());
    const SaveGame& s = load.save;
    CHECK(s.level == "street");
    CHECK(s.spawn == "east_end");
    CHECK(s.position.x == doctest::Approx(-12.5f));
    CHECK(s.position.z == doctest::Approx(7.25f));
    CHECK(s.yawRadians == doctest::Approx(1.5f));
    CHECK(s.pitchRadians == doctest::Approx(-0.2f));
    REQUIRE(s.flags.size() == 2);
    CHECK(s.flags[0] == "bowed_at_torii"); // sorted
    CHECK(s.counters.at("balls") == 120);
    CHECK(s.flashlightOn);
    CHECK_FALSE(s.headlightOn);
    CHECK(s.place == "the roadside shrine");
    CHECK(s.savedAt == "2026-10-18 21:05");

    // Flags in another order, written again: identical bytes.
    SaveGame shuffled = original;
    std::swap(shuffled.flags[0], shuffled.flags[1]);
    CHECK(WriteSave(shuffled) == text);
    CHECK(WriteSave(s) == text);
}

TEST_CASE("No file is no save; a broken, levelless or newer file is a warning and no save")
{
    const SaveLoad none = ParseSave("");
    CHECK_FALSE(none.found);
    CHECK(none.warning.empty());

    const std::string text = WriteSave(Sample());
    const SaveLoad truncated = ParseSave(std::string_view(text).substr(0, text.size() / 2));
    CHECK_FALSE(truncated.found);
    CHECK_FALSE(truncated.warning.empty());

    CHECK_FALSE(ParseSave(R"({ "schema": 1 })").found);              // no level
    const SaveLoad newer = ParseSave(R"({ "schema": 2, "level": "street" })");
    CHECK_FALSE(newer.found);
    CHECK(newer.warning.find("schema 2") != std::string::npos);
}

TEST_CASE("A save tolerates what it doesn't know and drops what it can't use")
{
    const SaveLoad load = ParseSave(R"({ "schema": 1, "level": "shore", "future": { "x": 1 },
        "position": [1, "two", 3], "pitch": 9, "flags": ["a", 7, "a", "b"],
        "counters": { "tokens": -4, "name": "x" } })");
    REQUIRE(load.found);
    CHECK(load.save.level == "shore");
    CHECK(load.save.position.x == 0.0f);          // not three numbers: unused
    CHECK(load.save.pitchRadians == doctest::Approx(1.5f)); // clamped
    REQUIRE(load.save.flags.size() == 2);         // strings, once each
    CHECK(load.save.counters.at("tokens") == 0);  // never below 0
    CHECK(load.save.counters.count("name") == 0);
    CHECK(load.save.headlightOn);                 // missing: its default
}

TEST_CASE("A file written atomically replaces the old one whole")
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "atom_save_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "save.json").string();

    REQUIRE(WriteFileAtomically(path, "first"));
    REQUIRE(WriteFileAtomically(path, "second, longer"));
    std::stringstream read;
    {
        std::ifstream file(path, std::ios::binary);
        read << file.rdbuf();
    } // closed: Windows won't delete an open file
    CHECK(read.str() == "second, longer");
    CHECK_FALSE(std::filesystem::exists(path + ".tmp")); // nothing left beside it

    CHECK_FALSE(WriteFileAtomically((dir / "missing" / "save.json").string(), "x")); // no folder: false, no throw
    std::filesystem::remove_all(dir);
}

TEST_CASE("GameState restores a save's progress, and clears for a new game")
{
    GameState state;
    state.SetFlag("old");
    state.SetCounter("tokens", 9);
    const SaveGame s = Sample();
    state.Restore(s.flags, s.counters);
    CHECK_FALSE(state.HasFlag("old"));
    CHECK(state.HasFlag("keeper_permission"));
    CHECK(state.GetCounter("tokens") == 3);
    state.Clear();
    CHECK(state.FlagCount() == 0);
    CHECK(state.GetCounter("balls") == 0);
}
