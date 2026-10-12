#include "LevelChecks.h"

#include <doctest/doctest.h>

// Shipped files checked: the "content" suite (Content.<owner> in ctest).
TEST_SUITE_BEGIN("content");

TEST_CASE("ATOM ARENA's level file is valid")
{
    CHECK(AtomTests::CheckShippedLevels() >= 1);
}

TEST_SUITE_END();
