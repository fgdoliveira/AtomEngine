#include "Assets/Animation.h"
#include "Assets/Model.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

#include <cmath>
#include <string>

using namespace Atom;

namespace
{
    AnimationChannel Translation(Interpolation interpolation)
    {
        AnimationChannel channel;
        channel.path = AnimationPath::Translation;
        channel.interpolation = interpolation;
        channel.times = { 0.0f, 1.0f, 3.0f };
        channel.values = { glm::vec4{ 0.0f }, glm::vec4{ 2.0f, 0.0f, 0.0f, 0.0f }, glm::vec4{ 2.0f, 4.0f, 0.0f, 0.0f } };
        return channel;
    }

    const std::string Assets = ATOM_SOURCE_DIR "/Assets/";
}

TEST_CASE("Linear channels interpolate between keys and clamp outside them")
{
    const AnimationChannel channel = Translation(Interpolation::Linear);
    CHECK(SampleChannel(channel, 0.5f).x == doctest::Approx(1.0f));
    CHECK(SampleChannel(channel, 2.0f).y == doctest::Approx(2.0f));
    CHECK(SampleChannel(channel, -1.0f).x == doctest::Approx(0.0f));
    CHECK(SampleChannel(channel, 9.0f).y == doctest::Approx(4.0f));
}

TEST_CASE("Step channels hold the previous key")
{
    const AnimationChannel channel = Translation(Interpolation::Step);
    CHECK(SampleChannel(channel, 0.99f).x == doctest::Approx(0.0f));
    CHECK(SampleChannel(channel, 1.5f).x == doctest::Approx(2.0f));
    CHECK(SampleChannel(channel, 1.5f).y == doctest::Approx(0.0f));
}

TEST_CASE("Rotations slerp along the short arc and stay unit length")
{
    AnimationChannel channel;
    channel.path = AnimationPath::Rotation;
    const float half = std::sqrt(0.5f);
    channel.times = { 0.0f, 1.0f };
    channel.values = { glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f },     // identity (x y z w)
                       glm::vec4{ 0.0f, half, 0.0f, half } };   // 90 degrees about Y
    const glm::vec4 middle = SampleChannel(channel, 0.5f);
    CHECK(glm::length(middle) == doctest::Approx(1.0f));
    // 45 degrees about Y: (0, sin 22.5, 0, cos 22.5).
    CHECK(middle.y == doctest::Approx(std::sin(0.3926991f)));
    CHECK(middle.w == doctest::Approx(std::cos(0.3926991f)));
}

TEST_CASE("Cubic splines pass through their keys")
{
    AnimationChannel channel;
    channel.interpolation = Interpolation::CubicSpline;
    channel.times = { 0.0f, 1.0f };
    // (in-tangent, value, out-tangent) per key.
    channel.values = { glm::vec4{ 0.0f }, glm::vec4{ 1.0f }, glm::vec4{ 0.0f },
                       glm::vec4{ 0.0f }, glm::vec4{ 3.0f }, glm::vec4{ 0.0f } };
    CHECK(SampleChannel(channel, 0.0f).x == doctest::Approx(1.0f));
    CHECK(SampleChannel(channel, 1.0f).x == doctest::Approx(3.0f));
    CHECK(SampleChannel(channel, 0.5f).x == doctest::Approx(2.0f)); // flat tangents: symmetric
}

TEST_CASE("The animated kit pieces ship their clips")
{
    const auto find = [](const std::string& file, const std::string& clip) {
        for (const AnimationClip& c : LoadModelAnimations(Assets + file))
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
    CHECK(LoadModelAnimations(Assets + "Kit/road.glb").empty());
}

TEST_CASE("Sway weights: cards move at their free end, solid pieces not at all")
{
    // Alpha = 1 - sway weight (Shaders/Sway.hlsli).
    float lowest = 1.0f;
    for (const PrimitiveGeometry& primitive : LoadModelGeometry(Assets + "Kit/grass_tuft.glb"))
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

    for (const PrimitiveGeometry& primitive : LoadModelGeometry(Assets + "Kit/road.glb"))
    {
        for (const Vertex& vertex : primitive.vertices)
        {
            REQUIRE(vertex.color[3] == 65535);
        }
    }
}
