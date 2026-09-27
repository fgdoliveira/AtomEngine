#pragma once

#include "Core/SlotMap.h"
#include "Interaction/Actions.h"

#include <glm/vec3.hpp>

#include <optional>
#include <string>

namespace AtomGame
{
    using EntityId = Atom::Handle;

    // A capability: "the player can use this". Any entity can have it; the
    // player code only ever sees this, never what kind of object it is.
    struct Interactable
    {
        std::string prompt;             // shown as "[E] <prompt>"
        Action action;
        glm::vec3 focusOffset{ 0.0f };  // point the player looks at, from the entity
        float radius = 2.2f;            // metres from the eye to the focus point

        // Optional gate: without this flag, `lockedAction` runs instead.
        std::string requiresFlag;
        std::optional<Action> lockedAction;
    };

    // Composition, not inheritance: an entity is a name, a place and a set
    // of optional capabilities. New kinds of object add capabilities rather
    // than subclasses.
    struct Entity
    {
        std::string name;
        glm::vec3 position{ 0.0f };
        std::optional<Interactable> interactable;
    };

    // Owns the entities of the current world. Everything else refers to
    // them by EntityId, which fails safely once an entity is gone.
    class GameWorld
    {
    public:
        EntityId Spawn(Entity entity) { return m_entities.Insert(std::move(entity)); }
        bool Despawn(EntityId id) { return m_entities.Remove(id); }
        void Clear() { m_entities.Clear(); }

        Entity* Find(EntityId id) { return m_entities.Get(id); }
        const Entity* Find(EntityId id) const { return m_entities.Get(id); }
        std::size_t Count() const { return m_entities.Size(); }

        template <typename F>
        void ForEach(F&& f) const { m_entities.ForEach(std::forward<F>(f)); }

    private:
        Atom::SlotMap<Entity> m_entities;
    };
}
