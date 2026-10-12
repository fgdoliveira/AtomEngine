#include "Assets/Animation.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

#include <cmath>

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
