#pragma once

#include "Renderer/SpotLight.h"

#include <glm/vec3.hpp>

namespace Atom
{
    class CollisionWorld;
}

namespace AtomGame
{
    // The flashlight (M44): the player's spot light. Found once (a story
    // flag says so), then F switches it on and off in any level.
    //
    // It is held, not bolted to the eye: the beam starts low and to the
    // right of the view and follows where you look a moment late, with a
    // slight sway - so turning sweeps the light across the room instead of
    // the light simply being the screen's centre.
    //
    // Pure logic: the renderer gets its SpotLight, gameplay asks Lights()
    // whether a point is in the beam (for things only the light reveals).
    class Flashlight
    {
    public:
        Flashlight();

        void SetOwned(bool owned);
        bool IsOwned() const { return m_owned; }
        bool IsOn() const { return m_owned && m_on; }
        // False if it can't be switched (not found yet).
        bool Toggle();
        void SetOn(bool on) { m_on = on && m_owned; }

        // Follows the view. `snap` puts it straight on target (arriving in
        // a level, teleports) instead of swinging in from where it was.
        void Update(const glm::vec3& eye, const glm::vec3& forward, const glm::vec3& right,
                    float deltaSeconds, bool snap = false);

        const Atom::SpotLight& GetLight() const { return m_light; }
        Atom::SpotLight& EditLight() { return m_light; } // tuning (developer tools)

        // Is `point` lit by the beam: on, inside the cone, within reach,
        // and no wall in between (`world` may be null: no occlusion test).
        bool Lights(const glm::vec3& point, const Atom::CollisionWorld* world) const;

        // How hard it chases the view (1/s), and how far it sways (degrees).
        float followRate = 14.0f;
        float swayDegrees = 0.35f;

    private:
        Atom::SpotLight m_light;
        glm::vec3 m_aim{ 0.0f, 0.0f, -1.0f };
        float m_time = 0.0f;
        bool m_owned = false;
        bool m_on = false;
    };
}
