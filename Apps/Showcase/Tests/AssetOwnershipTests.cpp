#include "TestAssets.h"

#include <doctest/doctest.h>

#include <filesystem>

// M106: the Showcase's tests see its own assets and the shared Content -
// never the demo's, though both ship a Levels/ folder.
TEST_CASE("The Showcase's tests find only its assets and the shared content")
{
    CHECK(std::filesystem::exists(AtomTests::Asset("Levels/showcase.json"))); // its own
    CHECK(std::filesystem::exists(AtomTests::Asset("Kit/road.glb")));         // shared
    // The demo ships these; the Showcase doesn't.
    REQUIRE(std::filesystem::exists(ATOM_SOURCE_DIR "/Apps/Games/Demo/Assets/Levels/street.json"));
    CHECK_FALSE(std::filesystem::exists(AtomTests::Asset("Levels/street.json")));
    REQUIRE(std::filesystem::exists(ATOM_SOURCE_DIR "/Apps/Games/Demo/Assets/Street/street_col.glb"));
    CHECK_FALSE(std::filesystem::exists(AtomTests::Asset("Street/street_col.glb")));
}
