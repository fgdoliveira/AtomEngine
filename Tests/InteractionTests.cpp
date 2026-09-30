#include "Interaction/ActionExecutor.h"
#include "Interaction/InteractionSystem.h"
#include "Interaction/MessageFeed.h"
#include "Physics/CollisionWorld.h"
#include "World/GameState.h"

#include <doctest/doctest.h>

using namespace AtomGame;

namespace
{
    Entity MakeInteractable(const char* name, glm::vec3 position, Action action)
    {
        Entity entity;
        entity.name = name;
        entity.position = position;
        entity.interactable = Interactable{ name, std::move(action) };
        return entity;
    }

    const glm::vec3 Eye{ 0.0f, 1.6f, 0.0f };
    const glm::vec3 LookingNorth{ 0.0f, 0.0f, -1.0f };
}

TEST_CASE("The interactable in front of the player is chosen")
{
    GameWorld world;
    const EntityId ahead = world.Spawn(MakeInteractable("ahead", { 0, 1.6f, -1.5f }, ShowMessage{ "a" }));
    world.Spawn(MakeInteractable("behind", { 0, 1.6f, 1.5f }, ShowMessage{ "b" }));

    CHECK(InteractionSystem::FindTarget(world, nullptr, Eye, LookingNorth) == ahead);
}

TEST_CASE("Out of reach or outside the view cone is ignored")
{
    GameWorld world;
    world.Spawn(MakeInteractable("far", { 0, 1.6f, -5.0f }, ShowMessage{ "" }));
    world.Spawn(MakeInteractable("aside", { 1.5f, 1.6f, -0.2f }, ShowMessage{ "" }));

    CHECK(InteractionSystem::FindTarget(world, nullptr, Eye, LookingNorth).IsNull());
}

TEST_CASE("Close up, an off-centre target still counts")
{
    // At arm's length, ~50 degrees off the view direction: outside the
    // far cone (~35 degrees) but inside the widened close-range cone.
    GameWorld world;
    const EntityId near = world.Spawn(MakeInteractable("near", { 0.4f, 1.2f, -0.5f }, ShowMessage{ "" }));

    CHECK(InteractionSystem::FindTarget(world, nullptr, Eye, LookingNorth) == near);
}

TEST_CASE("The more centred target wins")
{
    GameWorld world;
    world.Spawn(MakeInteractable("offset", { 0.6f, 1.6f, -1.5f }, ShowMessage{ "" }));
    const EntityId centred = world.Spawn(MakeInteractable("centred", { 0, 1.6f, -1.8f }, ShowMessage{ "" }));

    CHECK(InteractionSystem::FindTarget(world, nullptr, Eye, LookingNorth) == centred);
}

TEST_CASE("A wall between the player and the target blocks it")
{
    GameWorld world;
    world.Spawn(MakeInteractable("behind wall", { 0, 1.6f, -2.0f }, ShowMessage{ "" }));

    Atom::CollisionWorld collision;
    collision.AddTriangle({ -2, 0, -0.8f }, { 2, 0, -0.8f }, { 2, 3, -0.8f });
    collision.AddTriangle({ -2, 0, -0.8f }, { 2, 3, -0.8f }, { -2, 3, -0.8f });

    CHECK(InteractionSystem::FindTarget(world, &collision, Eye, LookingNorth).IsNull());
    // Without the wall it would have been chosen.
    CHECK_FALSE(InteractionSystem::FindTarget(world, nullptr, Eye, LookingNorth).IsNull());
}

TEST_CASE("A despawned target is no longer found and its id goes stale")
{
    GameWorld world;
    const EntityId id = world.Spawn(MakeInteractable("gone", { 0, 1.6f, -1.5f }, ShowMessage{ "" }));
    REQUIRE(world.Despawn(id));

    CHECK(world.Find(id) == nullptr);
    CHECK(InteractionSystem::FindTarget(world, nullptr, Eye, LookingNorth).IsNull());
}

TEST_CASE("A flag requirement switches to the locked action until met")
{
    Interactable gate{ "Open", ChangeLevel{ "shrine_grounds", "gate" } };
    gate.requiresFlag = "keeper_permission";
    gate.lockedAction = ShowMessage{ "It's locked." };

    GameState state;
    CHECK(std::holds_alternative<ShowMessage>(InteractionSystem::ResolveAction(gate, state)));

    state.SetFlag("keeper_permission");
    CHECK(std::holds_alternative<ChangeLevel>(InteractionSystem::ResolveAction(gate, state)));
}

TEST_CASE("Executing actions changes state and shows feedback")
{
    GameState state;
    MessageFeed messages;
    ActionContext context{ state, messages };

    ExecuteAction(SetFlag{ "bowed", "You bow." }, context);
    CHECK(state.HasFlag("bowed"));
    CHECK(messages.GetText() == "You bow.");

    ExecuteAction(ShowMessage{ "Nothing happens." }, context);
    CHECK(messages.GetText() == "Nothing happens.");
    CHECK(messages.IsVisible());
}

TEST_CASE("Counters: read 0 when missing, never go negative, spend only what's there")
{
    GameState state;
    CHECK(state.GetCounter("tokens") == 0);
    state.AddToCounter("tokens", 50);
    CHECK(state.Spend("tokens", 10));
    CHECK(state.GetCounter("tokens") == 40);
    CHECK_FALSE(state.Spend("tokens", 41));
    CHECK(state.GetCounter("tokens") == 40); // nothing taken
    state.AddToCounter("tokens", -100);
    CHECK(state.GetCounter("tokens") == 0);
}

TEST_CASE("addCounter gives once; exchange pays for a flag or explains why not")
{
    GameState state;
    MessageFeed messages;
    ActionContext context{ state, messages };

    const AddCounter welcome{ "tokens", 50, "Welcome.", "welcomed", "Already given." };
    ExecuteAction(welcome, context);
    ExecuteAction(welcome, context);
    CHECK(state.GetCounter("tokens") == 50);
    CHECK(state.HasFlag("welcomed"));
    CHECK(messages.GetText() == "Already given.");

    const Exchange prize{ "balls", 300, "prize", "Here.", "Not enough." };
    state.SetCounter("balls", 299);
    ExecuteAction(prize, context);
    CHECK_FALSE(state.HasFlag("prize"));
    CHECK(messages.GetText() == "Not enough.");
    state.SetCounter("balls", 320);
    ExecuteAction(prize, context);
    CHECK(state.HasFlag("prize"));
    CHECK(state.GetCounter("balls") == 20);
}
