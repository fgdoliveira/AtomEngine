#include "Flashlight.h"

#include "Physics/CollisionWorld.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>

namespace AtomGame
{
    namespace
    {
        // A point counts as lit when the beam there is at least this bright
        // (cone x falloff x intensity): the edge of a usable light, not the
        // last faint glow.
        constexpr float LitThreshold = 0.08f;
        // Where the hand holds it, from the eye.
        constexpr float HandRight = 0.18f;
        constexpr float HandDown = 0.15f;
        constexpr float HandForward = 0.1f;
    }

    Flashlight::Flashlight()
    {
        // An old incandescent torch: warm, a tight hot centre and a wide
        // soft spill.
        m_light.range = 14.0f;
        m_light.innerAngleDegrees = 10.0f;
        m_light.outerAngleDegrees = 24.0f;
        m_light.color = glm::vec3{ 1.0f, 0.9f, 0.74f };
        m_light.intensity = 22.0f;
        m_light.specular = 1.0f;
    }

    void Flashlight::SetOwned(bool owned)
    {
        if (owned && !m_owned)
        {
            m_on = true; // found: it comes on in your hand
        }
        m_owned = owned;
        if (!owned)
        {
            m_on = false;
        }
    }

    bool Flashlight::Toggle()
    {
        if (!m_owned)
        {
            return false;
        }
        m_on = !m_on;
        return true;
    }

    void Flashlight::Update(const glm::vec3& eye, const glm::vec3& forward, const glm::vec3& right,
                            float deltaSeconds, bool snap)
    {
        m_time += deltaSeconds;
        // Lag: the aim eases toward the view (frame-rate independent).
        const float follow = snap ? 1.0f : 1.0f - std::exp(-followRate * deltaSeconds);
        m_aim = glm::normalize(m_aim + (forward - m_aim) * follow);

        // Sway: two slow sines at unrelated rates, so it never repeats
        // visibly - a hand, not a machine.
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));
        const float sway = glm::radians(swayDegrees);
        const glm::vec3 wobble = right * (std::sin(m_time * 1.3f) * sway)
            + up * (std::sin(m_time * 0.9f + 1.7f) * sway * 0.7f);

        m_light.position = eye + right * HandRight - up * HandDown + forward * HandForward;
        m_light.direction = glm::normalize(m_aim + wobble);
    }

    bool Flashlight::Lights(const glm::vec3& point, const Atom::CollisionWorld* world) const
    {
        if (!IsOn())
        {
            return false;
        }
        const glm::vec3 toPoint = point - m_light.position;
        const float distance = glm::length(toPoint);
        if (distance >= m_light.range || distance < 1e-3f)
        {
            return false;
        }
        const float strength = Atom::SpotMath::Cone(m_light, toPoint)
            * Atom::SpotMath::Falloff(distance, m_light.range) * m_light.intensity;
        if (strength < LitThreshold)
        {
            return false;
        }
        // The same question the shadow map answers, asked of the collision
        // world: does anything stand between the lamp and the point? Hits
        // just before the point are the surface it lies on.
        if (world)
        {
            if (const auto hit = world->Raycast(m_light.position, point); hit && hit->distance < distance - 0.25f)
            {
                return false;
            }
        }
        return true;
    }
}
