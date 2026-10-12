#include "Flight.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace Drift
{
    namespace
    {
        // Frame-rate independent easing: 1 - e^(-rate * dt).
        float Ease(float dt, float rate) { return 1.0f - std::exp(-dt * rate); }
    }

    glm::vec2 Path(float z)
    {
        return { std::sin(z * 0.012f) * 14.0f + std::sin(z * 0.031f) * 4.0f,
                 std::cos(z * 0.009f) * 6.0f };
    }

    glm::mat4 CameraPose::View() const
    {
        // three.js: camera.lookAt(...), then rotation.z += roll - with Euler
        // order XYZ the Z rotation is innermost, i.e. about the view axis.
        const glm::mat4 look = glm::lookAt(position, target, glm::vec3{ 0.0f, 1.0f, 0.0f });
        return glm::rotate(glm::mat4{ 1.0f }, -roll, glm::vec3{ 0.0f, 0.0f, 1.0f }) * look;
    }

    Ship::Ship(std::uint32_t seed) : m_random(seed)
    {
        m_camera.position = m_cameraPosition;
    }

    float Ship::Update(float dt, float flow, const ShipInput& input)
    {
        // Inertia: velocity eases toward the input.
        const float k = Ease(dt, 5.0f);
        velocity.x += (input.x * 16.0f - velocity.x) * k;
        velocity.y += (input.y * 12.0f - velocity.y) * k;

        boost += ((input.boost ? 1.0f : 0.0f) - boost) * Ease(dt, 4.0f);
        const float target = 38.0f + flow * 34.0f;
        speed += (target - speed) * Ease(dt, 1.5f);
        const float forward = speed * (1.0f + boost * 0.7f);

        glm::vec3& p = position;
        p.z -= forward * dt;
        p.x += velocity.x * dt;
        p.y += velocity.y * dt;
        // Soft leash to the flight path.
        const glm::vec2 c = Path(p.z);
        const float dx = p.x - c.x;
        const float dy = p.y - c.y;
        const float d = std::hypot(dx, dy);
        if (d > Bound)
        {
            p.x = c.x + dx / d * Bound;
            p.y = c.y + dy / d * Bound;
        }

        // Bank and pitch into turns (three.js rotation x, y, z).
        glm::vec3& r = modelRotation;
        r.z += (-velocity.x * 0.05f - r.z) * k;
        r.x += (velocity.y * 0.03f - r.x) * k;
        r.y += (-velocity.x * 0.015f - r.y) * k;

        // Squash and stretch on boost.
        const float s = boost;
        modelScale = { 1.0f - s * 0.1f, 1.0f - s * 0.1f, 1.0f + s * 0.22f };

        // Spring camera.
        const glm::vec3 want{ p.x * 0.85f + c.x * 0.15f, p.y + 1.8f, p.z + 7.5f + s * 1.5f };
        m_cameraPosition += (want - m_cameraPosition) * Ease(dt, 6.0f);
        m_camera.position = m_cameraPosition;
        shake = std::max(0.0f, shake - dt * 2.0f);
        if (shake > 0.0f)
        {
            // A random direction, as three.js's randomDirection().
            std::uniform_real_distribution<float> u(-1.0f, 1.0f);
            std::uniform_real_distribution<float> a(0.0f, glm::two_pi<float>());
            const float zz = u(m_random);
            const float t = a(m_random);
            const float rr = std::sqrt(1.0f - zz * zz);
            m_camera.position += glm::vec3{ rr * std::cos(t), rr * std::sin(t), zz } * (shake * 0.4f);
        }
        m_camera.target = { p.x, p.y + 0.4f, p.z - 12.0f };
        m_camera.roll = -velocity.x * 0.006f;
        m_camera.fovDegrees += (70.0f + s * 18.0f + flow * 6.0f - m_camera.fovDegrees) * k;
        return forward;
    }

    glm::mat4 Ship::ModelMatrix() const
    {
        // three.js Euler order XYZ: R = Rx * Ry * Rz.
        glm::mat4 m = glm::translate(glm::mat4{ 1.0f }, position);
        m = glm::rotate(m, modelRotation.x, glm::vec3{ 1.0f, 0.0f, 0.0f });
        m = glm::rotate(m, modelRotation.y, glm::vec3{ 0.0f, 1.0f, 0.0f });
        m = glm::rotate(m, modelRotation.z, glm::vec3{ 0.0f, 0.0f, 1.0f });
        return glm::scale(m, modelScale);
    }
}
