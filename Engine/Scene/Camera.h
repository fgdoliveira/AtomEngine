#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Atom
{
    // First-person style camera in a right-handed, Y-up world.
    // Yaw 0 looks down -Z; positive yaw turns right, positive pitch looks up.
    class Camera
    {
    public:
        void SetPosition(const glm::vec3& position)
        {
            m_position = position;
        }

        const glm::vec3& GetPosition() const
        {
            return m_position;
        }

        void SetRotation(float yawRadians, float pitchRadians);
        void Rotate(float yawDeltaRadians, float pitchDeltaRadians);

        float GetYaw() const { return m_yaw; }
        float GetPitch() const { return m_pitch; }

        glm::vec3 GetForward() const;
        // Forward and right projected onto the ground plane.
        glm::vec3 GetFlatForward() const;
        glm::vec3 GetFlatRight() const;

        glm::mat4 GetViewMatrix() const;

        float verticalFov = 1.0472f; // 60 degrees
        float nearPlane = 0.05f;
        float farPlane = 200.0f;

    private:
        glm::vec3 m_position{ 0.0f };
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
    };
}
