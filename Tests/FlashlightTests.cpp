#include "Flashlight.h"
#include "Interaction/InteractionSystem.h"
#include "Physics/CollisionWorld.h"

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

using namespace AtomGame;

namespace
{
    const glm::vec3 Eye{ 0.0f, 1.6f, 0.0f };
    const glm::vec3 North{ 0.0f, 0.0f, -1.0f };
    const glm::vec3 East{ 1.0f, 0.0f, 0.0f };

    // A found, switched-on flashlight aimed north from the eye.
    Flashlight HeldNorth()
    {
        Flashlight light;
        light.SetOwned(true);
        light.Update(Eye, North, East, 0.0f, true);
        return light;
    }
}

TEST_CASE("The flashlight only switches once it has been found")
{
    Flashlight light;
    CHECK_FALSE(light.IsOn());
    CHECK_FALSE(light.Toggle());
    light.SetOn(true);
    CHECK_FALSE(light.IsOn());

    light.SetOwned(true); // picking it up switches it on in your hand
    CHECK(light.IsOn());
    CHECK(light.Toggle());
    CHECK_FALSE(light.IsOn());
    CHECK(light.Toggle());
    CHECK(light.IsOn());
}

TEST_CASE("The beam follows the view a moment late")
{
    Flashlight light = HeldNorth();
    CHECK(glm::dot(light.GetLight().direction, North) == doctest::Approx(1.0f).epsilon(0.001));

    // Turn to face east: one frame later the beam has only started to swing.
    light.Update(Eye, East, -North, 1.0f / 60.0f);
    const float started = glm::dot(light.GetLight().direction, East);
    CHECK(started < 0.5f);
    CHECK(started > 0.0f);
    // Half a second later it's there.
    for (int i = 0; i < 30; ++i)
    {
        light.Update(Eye, East, -North, 1.0f / 60.0f);
    }
    CHECK(glm::dot(light.GetLight().direction, East) == doctest::Approx(1.0f).epsilon(0.001));

    // Held in the hand: right of and below the eye.
    CHECK(light.GetLight().position.y < Eye.y);
}

TEST_CASE("A point is lit only in the beam, in reach, and in sight of the lamp")
{
    Flashlight light = HeldNorth();
    const glm::vec3 ahead = Eye + North * 4.0f;
    CHECK(light.Lights(ahead, nullptr));
    CHECK_FALSE(light.Lights(Eye + East * 4.0f, nullptr));        // beside, outside the cone
    CHECK_FALSE(light.Lights(Eye + North * 20.0f, nullptr));      // beyond its range
    CHECK_FALSE(light.Lights(Eye - North * 4.0f, nullptr));       // behind

    // A wall at 2 m stands between the lamp and the point at 4 m.
    Atom::CollisionWorld wall;
    wall.AddTriangle({ -3, -1, -2 }, { 3, -1, -2 }, { 3, 4, -2 });
    wall.AddTriangle({ -3, -1, -2 }, { 3, 4, -2 }, { -3, 4, -2 });
    CHECK_FALSE(light.Lights(ahead, &wall));
    // A mark on the wall itself is lit: the hit is the surface it lies on.
    CHECK(light.Lights(glm::vec3{ Eye.x, Eye.y, -2.0f }, &wall));

    light.Toggle(); // off: nothing is lit
    CHECK_FALSE(light.Lights(ahead, nullptr));
}

TEST_CASE("Things that need light are found only while the beam is on them")
{
    GameWorld world;
    Entity marks;
    marks.name = "chalk_marks";
    marks.position = Eye + North * 1.5f;
    marks.interactable = Interactable{ "Look", ShowMessage{ "an arrow" } };
    marks.interactable->requiresLight = true;
    const EntityId id = world.Spawn(std::move(marks));

    // No light given at all: not there.
    CHECK(InteractionSystem::FindTarget(world, nullptr, Eye, North).IsNull());

    Flashlight light = HeldNorth();
    InteractionSystem::Settings settings;
    settings.isLit = [&](const glm::vec3& point) { return light.Lights(point, nullptr); };
    CHECK(InteractionSystem::FindTarget(world, nullptr, Eye, North, settings) == id);

    light.Toggle(); // switched off in front of it
    CHECK(InteractionSystem::FindTarget(world, nullptr, Eye, North, settings).IsNull());
}
