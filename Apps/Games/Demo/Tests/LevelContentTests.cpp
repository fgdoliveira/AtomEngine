#include "Level/LevelData.h"
#include "LevelChecks.h"
#include "TestAssets.h"

#include <doctest/doctest.h>

using namespace AtomFramework;

// Shipped files checked: the "content" suite (Content.<owner> in ctest).
TEST_SUITE_BEGIN("content");

TEST_CASE("Every demo level file is valid")
{
    CHECK(AtomTests::CheckShippedLevels() >= 7);
}

TEST_CASE("The street level keeps its content")
{
    const LevelParseResult result = LoadLevelFile(AtomTests::Asset("Levels/street.json"));
    REQUIRE(result.level.has_value());
    const LevelData& street = *result.level;

    bool keeper = false;
    for (const EntityData& entity : street.entities)
    {
        keeper = keeper || (entity.name == "shrine_keeper" && entity.model == "ThirdParty/old_man_psx.glb");
    }
    CHECK(keeper);
    CHECK(street.emitters.size() == 3);
    CHECK(street.unease.figureSpots.size() == 6);
}

TEST_SUITE_END();
