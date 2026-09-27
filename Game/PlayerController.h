#pragma once

#include <glm/vec3.hpp>

namespace Atom
{
    class Camera;
    class Input;
}

namespace AtomGame
{
    // Walks a first-person camera over a flat ground at y = 0.
    // Collision against level geometry arrives with the street (M6).
    class PlayerController
    {
    public:
        void SetFeetPosition(const glm::vec3& position)
        {
            m_feetPosition = position;
        }

        // Applies mouse look and movement, then places the camera at eye
        // height including head-bob.
        void Update(
            const Atom::Input& input,
            Atom::Camera& camera,
            float deltaSeconds
        );

        float mouseSensitivity = 0.0022f; // radians per pixel
        float walkSpeed = 1.4f;           // m/s, unhurried walk
        float jogSpeed = 3.2f;
        float acceleration = 10.0f;       // 1/s, exponential approach
        float eyeHeight = 1.6f;
        float gravity = 9.81f;
        float boundsHalfExtent = 19.5f;   // stay on the test ground

    private:
        glm::vec3 m_feetPosition{ 0.0f };
        glm::vec3 m_velocity{ 0.0f };

        float m_bobPhase = 0.0f;
        float m_bobWeight = 0.0f;
        bool m_grounded = false;
    };
}
