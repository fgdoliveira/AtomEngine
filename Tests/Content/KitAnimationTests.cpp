#include "Assets/Animation.h"
#include "Assets/Model.h"
#include "TestAssets.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>

using namespace Atom;

// Shipped files checked: the "content" suite (Content.<owner> in ctest).
TEST_SUITE_BEGIN("content");

TEST_CASE("The animated kit pieces ship their clips")
{
    const auto find = [](const std::string& file, const std::string& clip) {
        for (const AnimationClip& c : LoadModelAnimations(AtomTests::Asset(file)))
        {
            if (c.name == clip)
            {
                return c.duration;
            }
        }
        return -1.0f;
    };
    CHECK(find("Kit/windmill.glb", "spin") == doctest::Approx(8.0f));
    CHECK(find("Kit/shed.glb", "open") == doctest::Approx(1.5f));
    CHECK(find("Kit/hanging_sign.glb", "swing") == doctest::Approx(3.0f));
    CHECK(LoadModelAnimations(AtomTests::Asset("Kit/road.glb")).empty());
}

TEST_CASE("Sway weights: cards move at their free end, solid pieces not at all")
{
    // Alpha = 1 - sway weight (Shaders/Sway.hlsli).
    float lowest = 1.0f;
    for (const PrimitiveGeometry& primitive : LoadModelGeometry(AtomTests::Asset("Kit/grass_tuft.glb")))
    {
        for (const Vertex& vertex : primitive.vertices)
        {
            lowest = std::min(lowest, vertex.color[3] / 65535.0f);
            if (vertex.position.y < 0.01f)
            {
                CHECK(vertex.color[3] == 65535); // roots stay planted
            }
        }
    }
    CHECK(lowest < 0.1f); // blade tips swing fully

    for (const PrimitiveGeometry& primitive : LoadModelGeometry(AtomTests::Asset("Kit/road.glb")))
    {
        for (const Vertex& vertex : primitive.vertices)
        {
            REQUIRE(vertex.color[3] == 65535);
        }
    }
}

TEST_SUITE_END();
