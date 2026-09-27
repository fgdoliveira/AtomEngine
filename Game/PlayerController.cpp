#include "PlayerController.h"

#include "Platform/Input.h"
#include "Scene/Camera.h"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <algorithm>
#include <cmath>

namespace AtomGame
{
    namespace
    {
        constexpr float BobFrequency = 1.9f;   // radians per metre walked
        constexpr float BobVertical = 0.035f;  // metres
        constexpr float BobLateral = 0.02f;    // metres
    }

    void PlayerController::Update(
        const Atom::Input& input,
        Atom::Camera& camera,
        float deltaSeconds
    )
    {
        camera.Rotate(
            input.GetMouseDeltaX() * mouseSensitivity,
            -input.GetMouseDeltaY() * mouseSensitivity
        );

        glm::vec2 move{ 0.0f };
        if (input.IsKeyDown(SDL_SCANCODE_W)) { move.y += 1.0f; }
        if (input.IsKeyDown(SDL_SCANCODE_S)) { move.y -= 1.0f; }
        if (input.IsKeyDown(SDL_SCANCODE_D)) { move.x += 1.0f; }
        if (input.IsKeyDown(SDL_SCANCODE_A)) { move.x -= 1.0f; }

        if (glm::dot(move, move) > 1.0f)
        {
            move = glm::normalize(move);
        }

        const bool jogging = input.IsKeyDown(SDL_SCANCODE_LSHIFT);
        const float speed = jogging ? jogSpeed : walkSpeed;
        const glm::vec3 targetVelocity =
            (camera.GetFlatForward() * move.y + camera.GetFlatRight() * move.x)
            * speed;

        // Frame-rate independent ease toward the target horizontal velocity.
        const float blend = 1.0f - std::exp(-acceleration * deltaSeconds);
        m_velocity.x += (targetVelocity.x - m_velocity.x) * blend;
        m_velocity.z += (targetVelocity.z - m_velocity.z) * blend;

        m_velocity.y -= gravity * deltaSeconds;
        m_feetPosition += m_velocity * deltaSeconds;

        m_grounded = m_feetPosition.y <= 0.0f;
        if (m_grounded)
        {
            m_feetPosition.y = 0.0f;
            m_velocity.y = 0.0f;
        }

        m_feetPosition.x =
            std::clamp(m_feetPosition.x, -boundsHalfExtent, boundsHalfExtent);
        m_feetPosition.z =
            std::clamp(m_feetPosition.z, -boundsHalfExtent, boundsHalfExtent);

        // Bob follows distance walked, and fades in/out with movement.
        const float horizontalSpeed =
            std::sqrt(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);
        if (m_grounded)
        {
            m_bobPhase += horizontalSpeed * BobFrequency * deltaSeconds;
        }

        const float targetWeight =
            std::min(horizontalSpeed / walkSpeed, 1.5f);
        m_bobWeight += (targetWeight - m_bobWeight) * blend;

        const float vertical =
            std::sin(m_bobPhase * 2.0f) * BobVertical * m_bobWeight;
        const float lateral =
            std::sin(m_bobPhase) * BobLateral * m_bobWeight;

        camera.SetPosition(
            m_feetPosition
            + glm::vec3{ 0.0f, eyeHeight + vertical, 0.0f }
            + camera.GetFlatRight() * lateral
        );
    }
}
