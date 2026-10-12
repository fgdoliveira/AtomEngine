#include "Testing/BasicTestHooks.h"

#include "Character/PlayerController.h"
#include "Level/LevelData.h"
#include "Scene/Camera.h"

#include <doctest/doctest.h>

#include <glm/trigonometric.hpp>

using namespace AtomFramework;

namespace
{
    // The hooks of an app with only a player and a camera: all Arrival() reads.
    struct ArrivalRig
    {
        PlayerController player;
        Atom::Camera camera;
        BasicTestHooks hooks{ BasicTestHooks::View{ nullptr, nullptr, nullptr, &player, &camera, nullptr, "" } };

        void Place(const glm::vec3& eye, float yawDegrees)
        {
            camera.SetPosition(eye);
            camera.SetRotation(glm::radians(yawDegrees), 0.0f);
        }
    };

    const SpawnPoint Spawn{ { 2.0f, 0.0f, -3.0f }, 90.0f };
}

TEST_CASE("Arrival (M104): an app that never says where it lands always fails")
{
    ArrivalRig rig;
    rig.Place({ 0.0f, 0.0f, 0.0f }, 0.0f);
    const ArrivalError error = rig.hooks.Arrival();
    CHECK(error.distance > 1000.0f);
    CHECK(error.yawDegrees > 1000.0f);
}

TEST_CASE("Arrival: at the spawn's eye and yaw it passes; a wrong position or yaw shows")
{
    ArrivalRig rig;
    rig.hooks.ExpectSpawn(Spawn);
    const glm::vec3 eye = Spawn.position + glm::vec3{ 0.0f, rig.player.eyeHeight, 0.0f };

    rig.Place(eye, 90.0f);
    CHECK(rig.hooks.Arrival().distance == doctest::Approx(0.0f));
    CHECK(rig.hooks.Arrival().yawDegrees == doctest::Approx(0.0f).epsilon(0.001));

    rig.Place(eye + glm::vec3{ 0.5f, 0.0f, 0.0f }, 90.0f); // half a metre off
    CHECK(rig.hooks.Arrival().distance == doctest::Approx(0.5f));

    rig.Place(Spawn.position, 90.0f); // the feet, not the eye
    CHECK(rig.hooks.Arrival().distance == doctest::Approx(rig.player.eyeHeight));

    rig.Place(eye, 100.0f); // turned ten degrees
    CHECK(rig.hooks.Arrival().distance == doctest::Approx(0.0f));
    CHECK(rig.hooks.Arrival().yawDegrees == doctest::Approx(10.0f));
}

TEST_CASE("Arrival: yaw is compared the short way round")
{
    ArrivalRig rig;
    rig.hooks.ExpectArrival({ 0.0f, 1.0f, 0.0f }, glm::radians(359.0f));
    rig.Place({ 0.0f, 1.0f, 0.0f }, 1.0f);
    CHECK(rig.hooks.Arrival().yawDegrees == doctest::Approx(2.0f));
}

TEST_CASE("Arrival: a fixed view (ATOM ARENA's overview) is a pose like any other")
{
    ArrivalRig rig;
    const glm::vec3 overview{ 0.0f, 13.0f, 21.0f };
    rig.hooks.ExpectArrival(overview, 0.0f);
    rig.Place(overview, 0.0f);
    CHECK(rig.hooks.Arrival().distance == doctest::Approx(0.0f));
    CHECK(rig.hooks.Arrival().yawDegrees == doctest::Approx(0.0f));

    rig.Place(Spawn.position, 0.0f); // still walking: not where it was drawn from
    CHECK(rig.hooks.Arrival().distance > 1.0f);
}
