#include "TestAssets.h"

#include <doctest/doctest.h>

#include <filesystem>

// M106: the demo's tests see the demo's assets and the shared Content -
// never another app's. A level naming a file only the Showcase ships
// would fail here, as it would in the demo's own build.
TEST_CASE("The demo's tests find only the demo's assets and the shared content")
{
    CHECK(std::filesystem::exists(AtomTests::Asset("Levels/street.json"))); // its own
    CHECK(std::filesystem::exists(AtomTests::Asset("Kit/road.glb")));       // shared
    // The Showcase ships these; the demo doesn't.
    REQUIRE(std::filesystem::exists(ATOM_SOURCE_DIR "/Apps/Showcase/Assets/ThirdParty/rudy.glb"));
    CHECK_FALSE(std::filesystem::exists(AtomTests::Asset("ThirdParty/rudy.glb")));
    REQUIRE(std::filesystem::exists(ATOM_SOURCE_DIR "/Apps/Showcase/Assets/Levels/showcase.json"));
    CHECK_FALSE(std::filesystem::exists(AtomTests::Asset("Levels/showcase.json")));
}
