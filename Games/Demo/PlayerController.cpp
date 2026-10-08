#include "PlayerController.h"

#include "Physics/CollisionWorld.h"
#include "Platform/Input.h"
#include "Scene/Camera.h"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <algorithm>
#include <cmath>

namespace Demo
{
    namespace
    {
        constexpr float BobFrequency = 1.9f;   // radians per metre walked
        constexpr float BobVertical = 0.035f;  // metres
        constexpr float BobLateral = 0.02f;    // metres

        // Longest horizontal move per collision step; well under the body
        // radius so thin fences cannot be tunnelled through.
        constexpr float MaxStepDistance = 0.1f;
        constexpr int ResolveIterations = 3;
        constexpr float StepSmoothing = 14.0f; // 1/s
        constexpr float KillHeight = -20.0f;
    }

    void PlayerController::Place(const glm::vec3& feet)
    {
        m_feetPosition = feet;
        m_spawnPosition = feet;
        m_visualFeetY = feet.y;
        m_velocity = glm::vec3{ 0.0f };
        m_grounded = false;
    }

    void PlayerController::Teleport(const glm::vec3& feet, Atom::Camera& camera)
    {
        m_feetPosition = feet;
        m_spawnPosition = feet;
        m_visualFeetY = feet.y;
        m_velocity = glm::vec3{ 0.0f };
        // The phase is kept: it is the footstep counter, and changing it
        // would play a step on arrival.
        m_bobWeight = 0.0f;
        m_grounded = false;
        camera.SetPosition(feet + glm::vec3{ 0.0f, eyeHeight, 0.0f });
    }

    void PlayerController::Update(
        const Atom::Input& input,
        const ActionInput& actions,
        Atom::Camera& camera,
        const Atom::CollisionWorld* world,
        float deltaSeconds
    )
    {
        camera.Rotate(
            input.GetMouseDeltaX() * mouseSensitivity,
            -input.GetMouseDeltaY() * mouseSensitivity
        );

        glm::vec2 move{ 0.0f };
        if (actions.Held(InputAction::MoveForward)) { move.y += 1.0f; }
        if (actions.Held(InputAction::MoveBack)) { move.y -= 1.0f; }
        if (actions.Held(InputAction::MoveRight)) { move.x += 1.0f; }
        if (actions.Held(InputAction::MoveLeft)) { move.x -= 1.0f; }

        if (glm::dot(move, move) > 1.0f)
        {
            move = glm::normalize(move);
        }

        const bool jogging = actions.Held(InputAction::Jog);
        const float speed = jogging ? jogSpeed : walkSpeed;
        const glm::vec3 targetVelocity =
            (camera.GetFlatForward() * move.y + camera.GetFlatRight() * move.x)
            * speed;

        Move(targetVelocity, false, world, deltaSeconds);

        // Bob follows distance walked, and fades in/out with movement.
        const float blend = 1.0f - std::exp(-acceleration * deltaSeconds);
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
            glm::vec3{ m_feetPosition.x, m_visualFeetY, m_feetPosition.z }
            + glm::vec3{ 0.0f, eyeHeight + vertical, 0.0f }
            + camera.GetFlatRight() * lateral
        );
    }

    void PlayerController::Move(const glm::vec3& targetVelocity, bool jump,
                                const Atom::CollisionWorld* world, float deltaSeconds)
    {
        // Frame-rate independent ease toward the target horizontal velocity.
        const float blend = 1.0f - std::exp(-acceleration * deltaSeconds);
        m_velocity.x += (targetVelocity.x - m_velocity.x) * blend;
        m_velocity.z += (targetVelocity.z - m_velocity.z) * blend;

        const glm::vec3 displacement{
            m_velocity.x * deltaSeconds,
            0.0f,
            m_velocity.z * deltaSeconds
        };

        if (world)
        {
            const glm::vec3 before = m_feetPosition;
            MoveHorizontally(*world, displacement);

            // Keep only the velocity that survived the walls, so sliding
            // along one does not build up speed pushing into it.
            if (deltaSeconds > 0.0f)
            {
                m_velocity.x = (m_feetPosition.x - before.x) / deltaSeconds;
                m_velocity.z = (m_feetPosition.z - before.z) / deltaSeconds;
            }
        }
        else
        {
            m_feetPosition += displacement;
        }

        if (jump && m_grounded)
        {
            m_velocity.y = jumpSpeed;
            m_grounded = false;
        }
        UpdateVertical(world, deltaSeconds);

        if (m_feetPosition.y < KillHeight)
        {
            m_feetPosition = m_spawnPosition;
            m_velocity = glm::vec3{ 0.0f };
            m_visualFeetY = m_spawnPosition.y;
        }

        // Ease the camera up steps; follow drops immediately.
        if (m_feetPosition.y > m_visualFeetY)
        {
            m_visualFeetY += (m_feetPosition.y - m_visualFeetY)
                * (1.0f - std::exp(-StepSmoothing * deltaSeconds));
        }
        else
        {
            m_visualFeetY = m_feetPosition.y;
        }
    }

    void PlayerController::MoveHorizontally(
        const Atom::CollisionWorld& world,
        const glm::vec3& displacement
    )
    {
        const float distance = glm::length(displacement);
        const int steps = std::max(
            1, static_cast<int>(std::ceil(distance / MaxStepDistance)));
        const glm::vec3 step = displacement / static_cast<float>(steps);

        for (int i = 0; i < steps; ++i)
        {
            m_feetPosition += step;
            ResolveWalls(world);
        }
    }

    void PlayerController::ResolveWalls(const Atom::CollisionWorld& world)
    {
        // Spheres from just above step height up to the head.
        const float lowest = stepHeight + radius;
        const float highest = bodyHeight - radius;
        const float heights[] = { lowest, (lowest + highest) * 0.5f, highest };

        for (int iteration = 0; iteration < ResolveIterations; ++iteration)
        {
            bool anyHit = false;
            for (const float height : heights)
            {
                glm::vec3 center = m_feetPosition + glm::vec3{ 0.0f, height, 0.0f };
                if (world.ResolveSphereHorizontal(center, radius))
                {
                    m_feetPosition.x = center.x;
                    m_feetPosition.z = center.z;
                    anyHit = true;
                }
            }
            if (!anyHit)
            {
                break;
            }
        }
    }

    void PlayerController::UpdateVertical(
        const Atom::CollisionWorld* world,
        float deltaSeconds
    )
    {
        const bool wasGrounded = m_grounded;
        const glm::vec3 probeOrigin =
            m_feetPosition + glm::vec3{ 0.0f, stepHeight, 0.0f };

        m_velocity.y -= gravity * deltaSeconds;
        m_feetPosition.y += m_velocity.y * deltaSeconds;

        std::optional<float> floor;
        if (world)
        {
            const float fallDistance = std::max(0.0f, -m_velocity.y * deltaSeconds);
            // While grounded, also look one step down so walking off a curb
            // sticks to the road instead of launching a tiny fall.
            const float reach = stepHeight + fallDistance
                + (wasGrounded ? stepHeight : 0.0f);
            floor = world->FindFloor(probeOrigin, reach);
        }
        else
        {
            floor = 0.0f;
        }

        m_grounded = false;
        if (floor)
        {
            const bool landing = m_feetPosition.y <= *floor;
            // Rising (a jump) never snaps back down to the floor.
            const bool snapping = wasGrounded && m_velocity.y <= 0.0f
                && m_feetPosition.y - *floor <= stepHeight;
            if (landing || snapping)
            {
                m_feetPosition.y = *floor;
                m_velocity.y = 0.0f;
                m_grounded = true;
            }
        }
    }
}
