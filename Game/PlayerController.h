#pragma once

#include "Input/InputContext.h"

#include <glm/vec3.hpp>

namespace Atom
{
    class Camera;
    class CollisionWorld;
    class Input;
}

namespace AtomGame
{
    // First-person walker. The body is a vertical stack of spheres that
    // slides along walls; the feet follow the floor found by a downward ray,
    // stepping up ledges no taller than stepHeight (curbs, plinths).
    class PlayerController
    {
    public:
        // Places the player at feet position with no momentum or head-bob
        // and moves the camera there at once, so the very next frame is
        // rendered from the new spot (level loads, spawns, test hooks).
        void Teleport(const glm::vec3& feet, Atom::Camera& camera);

        const glm::vec3& GetFeetPosition() const { return m_feetPosition; }
        bool IsGrounded() const { return m_grounded; }

        // Increments once per footfall (one head-bob cycle).
        int GetStepCount() const
        {
            return static_cast<int>(m_bobPhase / 3.14159265f);
        }

        // Applies mouse look and movement, then places the camera at eye
        // height including head-bob. With no world, walks on y = 0.
        void Update(
            const Atom::Input& input,
            const ActionInput& actions,
            Atom::Camera& camera,
            const Atom::CollisionWorld* world,
            float deltaSeconds
        );

        float mouseSensitivity = 0.0022f; // radians per pixel
        float walkSpeed = 1.4f;           // m/s, unhurried walk
        float jogSpeed = 3.2f;
        float acceleration = 10.0f;       // 1/s, exponential approach
        float eyeHeight = 1.6f;
        float gravity = 9.81f;
        float radius = 0.3f;
        float stepHeight = 0.35f;
        float bodyHeight = 1.8f;

    private:
        void MoveHorizontally(
            const Atom::CollisionWorld& world,
            const glm::vec3& displacement
        );
        void ResolveWalls(const Atom::CollisionWorld& world);
        void UpdateVertical(const Atom::CollisionWorld* world, float deltaSeconds);

        glm::vec3 m_feetPosition{ 0.0f };
        glm::vec3 m_spawnPosition{ 0.0f };
        glm::vec3 m_velocity{ 0.0f };

        // Feet height the camera follows; eases up steps instead of popping.
        float m_visualFeetY = 0.0f;

        float m_bobPhase = 0.0f;
        float m_bobWeight = 0.0f;
        bool m_grounded = false;
    };
}
