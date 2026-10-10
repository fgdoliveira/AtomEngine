#pragma once

#include <glm/vec3.hpp>

#include <functional>
#include <optional>

namespace AtomFramework
{
    // A third-person camera on a spring arm (M38): it hangs behind and
    // above a pivot (the character's shoulders) at the end of an arm the
    // player swings around. If a wall comes between the pivot and the
    // camera, the arm shortens at once so the view never goes through it;
    // when the wall is gone, it eases back out instead of jumping.
    class SpringArm
    {
    public:
        // Hit distance along the segment, or nothing (CollisionWorld::Raycast).
        using Raycast = std::function<std::optional<float>(const glm::vec3& from, const glm::vec3& to)>;

        float length = 3.2f;       // resting arm length, metres
        float minLength = 0.5f;    // never closer than this
        float margin = 0.25f;      // kept between the camera and a wall
        float returnRate = 4.0f;   // 1/s: how fast it eases back out
        float minPitch = -15.0f;   // degrees of elevation
        float maxPitch = 65.0f;

        void Reset(float yawDegrees, float pitchDegrees);
        void Orbit(float yawDegrees, float pitchDegrees);
        void Update(const glm::vec3& pivot, const Raycast& raycast, float deltaSeconds);

        float GetYawDegrees() const { return m_yaw; }
        float GetPitchDegrees() const { return m_pitch; }
        float GetCurrentLength() const { return m_current; }
        // Unit vector from the pivot toward the camera.
        glm::vec3 GetDirection() const;
        glm::vec3 GetEye() const { return m_pivot + GetDirection() * m_current; }
        // Atom::Camera yaw/pitch (radians) looking back at the pivot.
        float GetCameraYaw() const;
        float GetCameraPitch() const;

    private:
        float m_yaw = 0.0f;   // 0: camera on +Z of the pivot
        float m_pitch = 15.0f;
        float m_current = 3.2f;
        glm::vec3 m_pivot{ 0.0f };
    };
}
