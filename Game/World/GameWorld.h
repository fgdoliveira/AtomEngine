#pragma once

#include "Audio/AudioSystem.h"
#include "Core/SlotMap.h"
#include "Interaction/Actions.h"

#include <glm/vec3.hpp>

#include <optional>
#include <string>

namespace Atom
{
    class Model;
}

namespace AtomGame
{
    using EntityId = Atom::Handle;

    // A capability: "this is drawn". The model is shared and owned
    // elsewhere (by the game now, by the level in M12).
    struct Renderable
    {
        const Atom::Model* model = nullptr;
        float yaw = 0.0f; // radians around +Y
    };

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

    // A capability: "this moves" (M19). Plays one clip of the entity's
    // model; a looping clip can fire a sound a few times per loop (a
    // windmill's creak once per blade).
    struct Animated
    {
        int clip = -1;
        float duration = 0.0f;
        float time = 0.0f;
        float speed = 1.0f;
        bool loop = false;
        bool playing = false;
        Atom::SoundHandle sound;
        int soundsPerLoop = 0;
        glm::vec3 soundOffset{ 0.0f, 1.0f, 0.0f };
    };

    // Composition, not inheritance: an entity is a name, a place and a set
    // of optional capabilities. New kinds of object add capabilities rather
    // than subclasses.
    struct Entity
    {
        std::string name;
        glm::vec3 position{ 0.0f };
        std::optional<Renderable> renderable;
        std::optional<Interactable> interactable;
        std::optional<Animated> animated;
        bool hidden = false; // M26: not drawn (a bus before it arrives)
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
        template <typename F>
        void ForEach(F&& f) { m_entities.ForEach(std::forward<F>(f)); }

    private:
        Atom::SlotMap<Entity> m_entities;
    };
}
