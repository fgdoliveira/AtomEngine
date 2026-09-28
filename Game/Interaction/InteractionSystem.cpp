#include "Interaction/InteractionSystem.h"

#include "Physics/CollisionWorld.h"
#include "World/GameState.h"

#include <glm/geometric.hpp>

#include <algorithm>

namespace AtomGame
{
    EntityId InteractionSystem::FindTarget(
        const GameWorld& world,
        const Atom::CollisionWorld* collision,
        const glm::vec3& eye,
        const glm::vec3& forward,
        const Settings& settings
    )
    {
        EntityId best{};
        float bestScore = -1.0f;

        world.ForEach([&](EntityId id, const Entity& entity) {
            if (!entity.interactable)
            {
                return;
            }
            const Interactable& interactable = *entity.interactable;
            const glm::vec3 focus = entity.position + interactable.focusOffset;
            const glm::vec3 toFocus = focus - eye;
            const float distance = glm::length(toFocus);
            if (distance > interactable.radius || distance < 1e-3f)
            {
                return;
            }

            // Must be roughly where the player is looking; the closer it
            // is, the more off-centre it may be.
            const float alignment = glm::dot(toFocus / distance, forward);
            const float reach = std::clamp(
                (distance - settings.closeDistance)
                    / std::max(interactable.radius - settings.closeDistance, 0.01f),
                0.0f,
                1.0f);
            const float requiredAlignment = settings.closeConeCosine
                + (settings.coneCosine - settings.closeConeCosine) * reach;
            if (alignment < requiredAlignment)
            {
                return;
            }

            // No talking through walls. The target's own collision proxy
            // sits just in front of its focus point, so hits within the
            // slack of the target don't count as blocking.
            if (collision)
            {
                if (const auto hit = collision->Raycast(eye, focus);
                    hit && hit->distance < distance - settings.occlusionSlack)
                {
                    return;
                }
            }

            // Favour what's centred in view, then what's closer.
            const float score = alignment * 2.0f + (1.0f - distance / interactable.radius);
            if (score > bestScore)
            {
                bestScore = score;
                best = id;
            }
        });

        return best;
    }

    const Action& InteractionSystem::ResolveAction(
        const Interactable& interactable,
        const GameState& state
    )
    {
        if (!interactable.requiresFlag.empty()
            && !state.HasFlag(interactable.requiresFlag)
            && interactable.lockedAction)
        {
            return *interactable.lockedAction;
        }
        return interactable.action;
    }
}
