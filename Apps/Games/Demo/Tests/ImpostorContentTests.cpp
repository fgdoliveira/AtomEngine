#include "TestAssets.h"
#include "World/Impostors.h"

#include <doctest/doctest.h>

#include <fstream>
#include <sstream>
#include <string>

using namespace AtomFramework;

// Shipped files checked: the "content" suite (Content.<owner> in ctest).
TEST_SUITE_BEGIN("content");

TEST_CASE("The demo's impostor descriptor parses")
{
    std::ifstream file(AtomTests::Asset("City/tower_impostor.json"));
    std::stringstream text;
    text << file.rdbuf();
    std::string error;
    const auto descriptor = ParseImpostorDescriptor(text.str(), error);
    INFO(error);
    REQUIRE(descriptor.has_value());
    CHECK(descriptor->views == 8);
    CHECK(descriptor->height > descriptor->width);
    CHECK(descriptor->fog < 1.0f);
}

TEST_SUITE_END();
