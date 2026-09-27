#include "Scene/Camera.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Atom
{
    namespace
    {
        // Just short of straight up/down so lookAt never degenerates.
        constexpr float MaxPitch = glm::half_pi<float>() - 0.01f;
    }

    void Camera::SetRotation(float yawRadians, float pitchRadians)
    {
        m_yaw = std::remainder(yawRadians, glm::two_pi<float>());
        m_pitch = std::clamp(pitchRadians, -MaxPitch, MaxPitch);
    }

    void Camera::Rotate(float yawDeltaRadians, float pitchDeltaRadians)
    {
        SetRotation(m_yaw + yawDeltaRadians, m_pitch + pitchDeltaRadians);
    }

    glm::vec3 Camera::GetForward() const
    {
        const float cosPitch = std::cos(m_pitch);
        return glm::vec3{
            std::sin(m_yaw) * cosPitch,
            std::sin(m_pitch),
            -std::cos(m_yaw) * cosPitch
        };
    }

    glm::vec3 Camera::GetFlatForward() const
    {
        return glm::vec3{ std::sin(m_yaw), 0.0f, -std::cos(m_yaw) };
    }

    glm::vec3 Camera::GetFlatRight() const
    {
        return glm::vec3{ std::cos(m_yaw), 0.0f, std::sin(m_yaw) };
    }

    glm::mat4 Camera::GetViewMatrix() const
    {
        return glm::lookAt(
            m_position,
            m_position + GetForward(),
            glm::vec3{ 0.0f, 1.0f, 0.0f }
        );
    }
}
