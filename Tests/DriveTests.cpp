#include "Character/Animator.h"
#include "Character/SpringArm.h"
#include "Character/PlayerController.h"
#include "Physics/CollisionWorld.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

#include <map>
#include <string>

using namespace AtomFramework; // v0.0.14: the world layer

namespace
{
    // A wall `distance` metres behind the pivot (along the arm).
    SpringArm::Raycast WallAt(float distance)
    {
        return [distance](const glm::vec3& from, const glm::vec3& to) -> std::optional<float> {
            const float length = glm::length(to - from);
            return distance < length ? std::optional<float>{ distance } : std::nullopt;
        };
    }

    void AddQuad(Atom::CollisionWorld& world, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d)
    {
        world.AddTriangle(a, b, c);
        world.AddTriangle(a, c, d);
    }

    // A floor, and a 0.15 m step up to a platform from x = 2 on.
    Atom::CollisionWorld FloorWithStep()
    {
        Atom::CollisionWorld world;
        AddQuad(world, { -10, 0, -10 }, { -10, 0, 10 }, { 10, 0, 10 }, { 10, 0, -10 });
        AddQuad(world, { 2, 0.15f, -10 }, { 2, 0.15f, 10 }, { 10, 0.15f, 10 }, { 10, 0.15f, -10 });
        AddQuad(world, { 2, 0, -10 }, { 2, 0.15f, -10 }, { 2, 0.15f, 10 }, { 2, 0, 10 });
        return world;
    }

    std::optional<Animator::ClipInfo> Clips(std::string_view name)
    {
        static const std::map<std::string, Animator::ClipInfo, std::less<>> clips{
            { "Idle", { 0, 2.0f } }, { "Jump", { 1, 1.4f } }, { "Run", { 2, 0.7f } }, { "Walk", { 3, 1.1f } } };
        const auto found = clips.find(name);
        return found != clips.end() ? std::optional{ found->second } : std::nullopt;
    }
}

TEST_CASE("The spring arm pulls in at once and eases back out")
{
    SpringArm arm;
    arm.Reset(0.0f, 20.0f);
    arm.Update(glm::vec3{ 0.0f }, WallAt(100.0f), 0.016f);
    CHECK(arm.GetCurrentLength() == doctest::Approx(arm.length));

    // A wall 1.5 m behind: the camera jumps in front of it this frame.
    arm.Update(glm::vec3{ 0.0f }, WallAt(1.5f), 0.016f);
    CHECK(arm.GetCurrentLength() == doctest::Approx(1.5f - arm.margin));

    // Never into the character's head, however close the wall.
    arm.Update(glm::vec3{ 0.0f }, WallAt(0.1f), 0.016f);
    CHECK(arm.GetCurrentLength() == doctest::Approx(arm.minLength));

    // Wall gone: back out smoothly, not in one frame.
    arm.Update(glm::vec3{ 0.0f }, WallAt(100.0f), 0.016f);
    CHECK(arm.GetCurrentLength() < arm.length * 0.5f);
    for (int i = 0; i < 120; ++i)
    {
        arm.Update(glm::vec3{ 0.0f }, WallAt(100.0f), 0.016f);
    }
    CHECK(arm.GetCurrentLength() == doctest::Approx(arm.length).epsilon(0.01));

    // The camera looks back at the pivot.
    const glm::vec3 eye = arm.GetEye();
    const float yaw = arm.GetCameraYaw();
    const float pitch = arm.GetCameraPitch();
    const glm::vec3 forward{ std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch) };
    CHECK(glm::dot(forward, glm::normalize(-eye)) == doctest::Approx(1.0f).epsilon(1e-4));
}

TEST_CASE("The body jumps only from the ground, and lands")
{
    const Atom::CollisionWorld world = FloorWithStep();
    PlayerController body;
    body.Place({ 0.0f, 0.0f, 0.0f });
    body.Move(glm::vec3{ 0.0f }, false, &world, 1.0f / 60.0f);
    REQUIRE(body.IsGrounded());

    body.Move(glm::vec3{ 0.0f }, true, &world, 1.0f / 60.0f);
    CHECK_FALSE(body.IsGrounded());
    float peak = 0.0f;
    int frames = 0;
    for (; frames < 120 && !body.IsGrounded(); ++frames)
    {
        // Asking again mid-air does nothing: no double jumps.
        body.Move(glm::vec3{ 0.0f }, true, &world, 1.0f / 60.0f);
        peak = std::max(peak, body.GetFeetPosition().y);
    }
    CHECK(body.IsGrounded());
    // v^2 / 2g: about a metre, in the air for about 0.9 s.
    CHECK(peak == doctest::Approx(body.jumpSpeed * body.jumpSpeed / (2.0f * body.gravity)).epsilon(0.05));
    CHECK(frames == doctest::Approx(60.0f * 2.0f * body.jumpSpeed / body.gravity).epsilon(0.08));
}

TEST_CASE("The body walks up a step")
{
    const Atom::CollisionWorld world = FloorWithStep();
    PlayerController body;
    body.Place({ 0.0f, 0.0f, 0.0f });
    for (int i = 0; i < 120; ++i)
    {
        body.Move({ 2.0f, 0.0f, 0.0f }, false, &world, 1.0f / 60.0f);
    }
    CHECK(body.GetFeetPosition().x > 3.0f);
    CHECK(body.GetFeetPosition().y == doctest::Approx(0.15f));
    CHECK(body.IsGrounded());
}

TEST_CASE("Animation events fire once each time the clip passes them")
{
    AnimatorData data;
    data.initial = "walk";
    data.states["walk"].clip = "Walk";
    data.states["jump"].clip = "Jump";
    data.states["jump"].inPlace = "hips";
    data.events = { { "Walk", "foot", 0.45f }, { "Walk", "foot", 0.98f } };

    Animator animator;
    const auto noJoints = animator.Bind(data, Clips);
    REQUIRE(noJoints); // "hips" can't be found without a node lookup
    REQUIRE_FALSE(animator.Bind(data, Clips, [](std::string_view name) { return name == "hips" ? 20 : -1; }));

    // Two full walk cycles in 1/60 s steps: four footfalls, never doubled.
    int feet = 0;
    for (int i = 0; i < 132; ++i)
    {
        animator.Update(1.0f / 60.0f);
        for (const std::string& event : animator.TakeEvents())
        {
            CHECK(event == "foot");
            ++feet;
        }
    }
    CHECK(feet == 4);

    // One big step that wraps past the end still finds the event there.
    animator.ForceState("walk");
    animator.Update(1.0f);   // passes 0.45 and 0.98
    CHECK(animator.TakeEvents().size() == 2);

    // The in-place state pins its joint in every sample it gives.
    animator.ForceState("jump");
    for (const Atom::ClipSample& sample : animator.GetSamples())
    {
        CHECK(sample.pinNode == 20);
    }
}
