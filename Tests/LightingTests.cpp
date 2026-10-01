#include "Renderer/SpotLight.h"

#include <doctest/doctest.h>

#include <glm/trigonometric.hpp>

#include <cmath>

using namespace Atom;

namespace
{
    // A spot at the origin pointing down -Z, as a flashlight held level.
    SpotLight Torch()
    {
        SpotLight light;
        light.position = glm::vec3{ 0.0f };
        light.direction = glm::vec3{ 0.0f, 0.0f, -1.0f };
        light.range = 10.0f;
        light.innerAngleDegrees = 10.0f;
        light.outerAngleDegrees = 20.0f;
        light.intensity = 5.0f;
        return light;
    }

    // A point `metres` away at `degrees` off the axis.
    glm::vec3 OffAxis(float metres, float degrees)
    {
        const float a = glm::radians(degrees);
        return glm::vec3{ std::sin(a), 0.0f, -std::cos(a) } * metres;
    }
}

TEST_CASE("The cone: full inside the inner angle, nothing past the outer one")
{
    const SpotLight light = Torch();
    CHECK(SpotMath::Cone(light, OffAxis(3.0f, 0.0f)) == doctest::Approx(1.0f));
    CHECK(SpotMath::Cone(light, OffAxis(3.0f, 9.0f)) == doctest::Approx(1.0f));
    CHECK(SpotMath::Cone(light, OffAxis(3.0f, 21.0f)) == doctest::Approx(0.0f));
    CHECK(SpotMath::Cone(light, OffAxis(3.0f, 90.0f)) == doctest::Approx(0.0f));
    CHECK(SpotMath::Cone(light, OffAxis(3.0f, 180.0f)) == doctest::Approx(0.0f)); // behind the lamp

    // Between the two it falls steadily: a soft edge, no step.
    float previous = 1.0f;
    for (float degrees = 10.5f; degrees < 20.0f; degrees += 0.5f)
    {
        const float cone = SpotMath::Cone(light, OffAxis(3.0f, degrees));
        CHECK(cone < previous);
        CHECK(cone > 0.0f);
        previous = cone;
    }
}

TEST_CASE("The falloff ends exactly at the range, and falls all the way there")
{
    CHECK(SpotMath::Falloff(0.0f, 10.0f) == doctest::Approx(1.0f));
    CHECK(SpotMath::Falloff(10.0f, 10.0f) == doctest::Approx(0.0f));
    CHECK(SpotMath::Falloff(12.0f, 10.0f) == doctest::Approx(0.0f));
    float previous = SpotMath::Falloff(0.0f, 10.0f);
    for (float d = 0.25f; d <= 10.0f; d += 0.25f)
    {
        const float falloff = SpotMath::Falloff(d, 10.0f);
        CHECK(falloff < previous);
        previous = falloff;
    }
    // Near the lamp it's close to inverse-square: twice as far, about a
    // quarter (the +1 and the window bend it a little).
    const float ratio = SpotMath::Falloff(4.0f, 100.0f) / SpotMath::Falloff(2.0f, 100.0f);
    CHECK(ratio == doctest::Approx(5.0f / 17.0f).epsilon(0.01));
}

TEST_CASE("Faces turned away from the spot get neither light nor highlight")
{
    const SpotLight light = Torch();
    const glm::vec3 point = OffAxis(3.0f, 0.0f);
    const glm::vec3 facing{ 0.0f, 0.0f, 1.0f };  // toward the lamp
    const glm::vec3 away{ 0.0f, 0.0f, -1.0f };
    const glm::vec3 eye{ 0.0f };

    const auto lit = SpotMath::Evaluate(light, point, facing, eye, 32.0f, 0.5f);
    CHECK(lit.diffuse > 0.0f);
    CHECK(lit.specular > 0.0f);

    const auto back = SpotMath::Evaluate(light, point, away, eye, 32.0f, 0.5f);
    CHECK(back.diffuse == 0.0f);
    CHECK(back.specular == 0.0f);

    // Beyond the range, or outside the cone: nothing.
    CHECK(SpotMath::Evaluate(light, OffAxis(11.0f, 0.0f), facing, eye, 32.0f, 0.5f).diffuse == 0.0f);
    CHECK(SpotMath::Evaluate(light, OffAxis(3.0f, 30.0f), facing, eye, 32.0f, 0.5f).diffuse == 0.0f);
}

TEST_CASE("Roughness sets the highlight: tight and strong when smooth, broad and faint when rough")
{
    CHECK(SpotMath::Shininess(0.0f) == doctest::Approx(256.0f));
    CHECK(SpotMath::Shininess(1.0f) == doctest::Approx(2.0f));
    CHECK(SpotMath::Shininess(0.3f) > SpotMath::Shininess(0.6f));
    CHECK(SpotMath::SpecularStrength(0.2f, -1.0f) > SpotMath::SpecularStrength(0.9f, -1.0f));
    CHECK(SpotMath::SpecularStrength(0.9f, 0.8f) == doctest::Approx(0.8f)); // authored wins

    // At the mirror angle the highlight peaks; a few degrees off, a smooth
    // surface has lost much more of it than a rough one.
    SpotLight light = Torch();
    const glm::vec3 point{ 0.0f, 0.0f, -3.0f };
    const glm::vec3 normal{ 0.0f, 0.0f, 1.0f };
    const glm::vec3 mirrorEye{ 0.0f, 0.0f, 0.0f };
    const glm::vec3 offEye = point + glm::normalize(glm::vec3{ 0.3f, 0.0f, 1.0f }) * 3.0f;
    const float smoothShine = SpotMath::Shininess(0.2f);
    const float roughShine = SpotMath::Shininess(0.8f);
    const auto smoothPeak = SpotMath::Evaluate(light, point, normal, mirrorEye, smoothShine, 1.0f).specular;
    const auto smoothOff = SpotMath::Evaluate(light, point, normal, offEye, smoothShine, 1.0f).specular;
    const auto roughPeak = SpotMath::Evaluate(light, point, normal, mirrorEye, roughShine, 1.0f).specular;
    const auto roughOff = SpotMath::Evaluate(light, point, normal, offEye, roughShine, 1.0f).specular;
    CHECK(smoothOff / smoothPeak < roughOff / roughPeak);
    CHECK(smoothPeak > roughPeak);
}

TEST_CASE("The spot's shadow matrix holds its cone and orders depth along each ray")
{
    const SpotLight light = Torch();
    const glm::mat4 viewProjection = SpotMath::ViewProjection(light);
    const auto project = [&](const glm::vec3& point) {
        const glm::vec4 clip = viewProjection * glm::vec4{ point, 1.0f };
        return glm::vec3{ clip } / clip.w;
    };

    // Everything the spot lights lands inside the map, at a depth in [0, 1].
    for (const float degrees : { 0.0f, 10.0f, 19.0f })
    {
        for (const float metres : { 0.5f, 5.0f, 9.5f })
        {
            const glm::vec3 ndc = project(OffAxis(metres, degrees));
            CHECK(std::abs(ndc.x) <= 1.0f);
            CHECK(std::abs(ndc.y) <= 1.0f);
            CHECK(ndc.z >= 0.0f);
            CHECK(ndc.z <= 1.0f);
        }
    }
    // Beyond the range: past the far plane, so unshadowed (and unlit).
    CHECK(project(OffAxis(12.0f, 0.0f)).z > 1.0f);

    // A wall at 3 m in front of a floor at 6 m along the same ray: the wall
    // is nearer, so the map keeps the wall's depth and the floor behind it
    // compares as further - in shadow. Same for any angle in the cone.
    for (const float degrees : { 0.0f, 7.0f, 15.0f })
    {
        const glm::vec3 wall = project(OffAxis(3.0f, degrees));
        const glm::vec3 floor = project(OffAxis(6.0f, degrees));
        CHECK(wall.x == doctest::Approx(floor.x).epsilon(1e-4));
        CHECK(wall.y == doctest::Approx(floor.y).epsilon(1e-4));
        CHECK(wall.z < floor.z);
    }

    // Pointing straight down still gives a valid matrix (no NaNs).
    SpotLight down = light;
    down.direction = glm::vec3{ 0.0f, -1.0f, 0.0f };
    const glm::mat4 vp = SpotMath::ViewProjection(down);
    const glm::vec4 clip = vp * glm::vec4{ 0.0f, -3.0f, 0.0f, 1.0f };
    CHECK(std::abs(clip.x / clip.w) < 1e-4f);
    CHECK(std::abs(clip.y / clip.w) < 1e-4f);
}
