#include "Scene/Camera.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace
{
    // Where a world direction lands in view space (no translation).
    glm::vec3 InView(const Atom::Camera& camera, const glm::vec3& direction)
    {
        return glm::vec3(camera.GetViewMatrix() * glm::vec4(direction, 0.0f));
    }
}

TEST_CASE("A camera's roll tilts the view about its axis; 0 keeps the horizon level")
{
    Atom::Camera camera;
    camera.SetRotation(0.0f, 0.0f); // looking down -Z

    // Level: world up is view up.
    const glm::vec3 level = InView(camera, { 0.0f, 1.0f, 0.0f });
    CHECK(level.x == doctest::Approx(0.0f));
    CHECK(level.y == doctest::Approx(1.0f));

    // Leaning right (+roll): the view's up tips toward world right (+X), so
    // world up appears turned toward the view's left.
    camera.roll = 0.3f;
    const glm::vec3 leaning = InView(camera, { 0.0f, 1.0f, 0.0f });
    CHECK(leaning.x < -0.25f);
    CHECK(leaning.y == doctest::Approx(std::cos(0.3f)).epsilon(0.001));
    CHECK(glm::length(leaning) == doctest::Approx(1.0f));

    // The view axis itself doesn't move: still straight ahead.
    const glm::vec3 ahead = InView(camera, camera.GetForward());
    CHECK(ahead.z == doctest::Approx(-1.0f));

    // And the other way.
    camera.roll = -0.3f;
    CHECK(InView(camera, { 0.0f, 1.0f, 0.0f }).x > 0.25f);
}
