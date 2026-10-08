#include "Character/SpringArm.h"

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace AtomFramework
{
    void SpringArm::Reset(float yawDegrees, float pitchDegrees)
    {
        m_yaw = std::remainder(yawDegrees, 360.0f);
        m_pitch = std::clamp(pitchDegrees, minPitch, maxPitch);
        m_current = length;
    }

    void SpringArm::Orbit(float yawDegrees, float pitchDegrees)
    {
        m_yaw = std::remainder(m_yaw + yawDegrees, 360.0f);
        m_pitch = std::clamp(m_pitch + pitchDegrees, minPitch, maxPitch);
    }

    glm::vec3 SpringArm::GetDirection() const
    {
        const float yaw = glm::radians(m_yaw);
        const float pitch = glm::radians(m_pitch);
        return { std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch) };
    }

    void SpringArm::Update(const glm::vec3& pivot, const Raycast& raycast, float deltaSeconds)
    {
        m_pivot = pivot;
        // How long the arm may be: up to the first wall behind, less a margin.
        float allowed = length;
        if (raycast)
        {
            if (const auto hit = raycast(pivot, pivot + GetDirection() * length))
            {
                allowed = std::max(minLength, *hit - margin);
            }
        }
        if (allowed < m_current)
        {
            m_current = allowed; // pull in at once: never look through a wall
        }
        else
        {
            m_current += (allowed - m_current) * (1.0f - std::exp(-returnRate * deltaSeconds));
        }
    }

    float SpringArm::GetCameraYaw() const
    {
        return glm::radians(-m_yaw);
    }

    float SpringArm::GetCameraPitch() const
    {
        return glm::radians(-m_pitch);
    }
}
