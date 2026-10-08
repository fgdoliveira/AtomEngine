#pragma once

#include <functional>

#include "World/GameWorld.h"

#include <glm/vec3.hpp>

namespace Atom
{
    class CollisionWorld;
}

namespace AtomGame
{
    class GameState;

    // Picks what the player would interact with, and resolves which action
    // that means right now. Knows about capabilities, never object types.
    class InteractionSystem
    {
    public:
        struct Settings
        {
            // The view cone widens as the target gets closer: something right
            // in front of you is off-centre even when you're clearly facing it.
            float coneCosine = 0.82f;      // ~35 degrees at full reach
            float closeConeCosine = 0.35f; // ~70 degrees at arm's length
            float closeDistance = 0.5f;    // metres
            float occlusionSlack = 0.6f;  // metres; ignore hits this close to the target

            // M44: is a point lit by the flashlight? Interactables marked
            // requiresLight are only found while it is. Unset: nothing is.
            std::function<bool(const glm::vec3&)> isLit;
        };

        // Best interactable in reach and in view with a clear line of
        // sight, or a null id.
        static EntityId FindTarget(
            const GameWorld& world,
            const Atom::CollisionWorld* collision,
            const glm::vec3& eye,
            const glm::vec3& forward,
            const Settings& settings
        );

        static EntityId FindTarget(
            const GameWorld& world,
            const Atom::CollisionWorld* collision,
            const glm::vec3& eye,
            const glm::vec3& forward
        )
        {
            return FindTarget(world, collision, eye, forward, Settings{});
        }

        // The prompt to show for it given the flags (M45: a door that
        // becomes a way down says so).
        static const std::string& ResolvePrompt(
            const Interactable& interactable,
            const GameState& state
        );

        // The action to perform for this interactable given the flags.
        static const Action& ResolveAction(
            const Interactable& interactable,
            const GameState& state
        );
    };
}
