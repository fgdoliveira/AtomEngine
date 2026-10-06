#include "World.h"

#include "Flight.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace Drift
{
    glm::mat4 Thing::Matrix() const
    {
        glm::mat4 m = glm::translate(glm::mat4{ 1.0f }, position);
        m = glm::rotate(m, rotation.x, glm::vec3{ 1.0f, 0.0f, 0.0f });
        m = glm::rotate(m, rotation.y, glm::vec3{ 0.0f, 1.0f, 0.0f });
        m = glm::rotate(m, rotation.z, glm::vec3{ 0.0f, 0.0f, 1.0f });
        return glm::scale(m, glm::vec3{ drawScale });
    }

    World::World(std::uint32_t seed) : m_random(seed)
    {
    }

    void World::SpawnSegment(float z)
    {
        const glm::vec2 c = Path(z);
        Thing ring;
        ring.kind = Kind::Ring;
        ring.position = { c.x, c.y, z };
        m_things.push_back(ring);

        // An orb trail weaving around the path toward the next gate.
        const float phase = Random() * glm::two_pi<float>();
        const float amplitude = 1.0f + Random() * 3.0f;
        for (int i = 1; i <= 7; ++i)
        {
            const float oz = z - static_cast<float>(i) * (SegmentLength / 8.0f);
            const glm::vec2 p = Path(oz);
            Thing orb;
            orb.kind = Kind::Orb;
            orb.position = { p.x + std::sin(phase + static_cast<float>(i) * 0.6f) * amplitude,
                             p.y + std::cos(phase + static_cast<float>(i) * 0.6f) * amplitude * 0.6f, oz };
            m_things.push_back(orb);
        }

        // Rocks drift off the racing line, more of them further in.
        const int rocks = 2 + std::min(4, m_segment / 4);
        for (int i = 0; i < rocks; ++i)
        {
            const float rz = z - Random() * SegmentLength;
            const glm::vec2 p = Path(rz);
            const float angle = Random() * glm::two_pi<float>();
            const float distance = 4.5f + Random() * 9.0f;
            Thing rock;
            rock.kind = Kind::Rock;
            rock.scale = rock.drawScale = 0.8f + Random() * 1.8f;
            rock.radius = rock.scale * 1.05f;
            // three.js randomDirection() * 0.6
            const float sz = Random() * 2.0f - 1.0f;
            const float st = Random() * glm::two_pi<float>();
            const float sr = std::sqrt(1.0f - sz * sz);
            rock.spin = glm::vec3{ sr * std::cos(st), sr * std::sin(st), sz } * 0.6f;
            rock.position = { p.x + std::cos(angle) * distance, p.y + std::sin(angle) * distance, rz };
            rock.rotation = { Random() * 6.0f, Random() * 6.0f, 0.0f };
            m_things.push_back(rock);
        }
        ++m_segment;
    }

    void World::Update(float dt, float time, float shipZ)
    {
        while (m_nextZ > shipZ - Ahead)
        {
            SpawnSegment(m_nextZ);
            m_nextZ -= SegmentLength;
        }
        // Recycle what's behind.
        std::erase_if(m_things, [&](const Thing& t) { return t.position.z > shipZ + Behind; });

        for (Thing& t : m_things)
        {
            switch (t.kind)
            {
            case Kind::Ring:
                t.pop = std::max(0.0f, t.pop - dt * 2.5f);
                t.drawScale = 1.0f + (1.0f - t.pop) * t.pop * 1.6f;
                t.rotation.z += dt * (0.2f + t.pop * 6.0f);
                break;
            case Kind::Orb:
                if (t.hit)
                {
                    t.pop += dt * 5.0f;
                    t.drawScale = 1.0f + t.pop * 2.5f;
                    t.visible = t.pop < 1.0f;
                }
                else
                {
                    t.rotation.y += dt * 2.0f;
                    t.rotation.x += dt;
                    t.drawScale = 1.0f + std::sin(time * 6.0f + t.position.z * 0.3f) * 0.12f;
                }
                break;
            case Kind::Rock:
                t.rotation.x += t.spin.x * dt;
                t.rotation.y += t.spin.y * dt;
                break;
            }
        }
    }

    void Flow::Add(float amount)
    {
        value = std::clamp(value + amount, 0.0f, 1.0f);
    }

    FlowEvents Flow::Check(World& world, const glm::vec3& ship, float previousZ, float dt)
    {
        FlowEvents events;
        // Rings, then orbs, then rocks, as the original's three loops: in a
        // frame with an orb and a rock, the rock's chain reset comes last.
        for (const Kind pass : { Kind::Ring, Kind::Orb, Kind::Rock })
        for (Thing& t : world.Things())
        {
            if (t.hit || t.kind != pass)
            {
                continue;
            }
            switch (t.kind)
            {
            case Kind::Ring:
                // Crossed the ring's plane this frame.
                if (t.position.z <= previousZ && t.position.z > ship.z)
                {
                    t.hit = true;
                    if (std::hypot(ship.x - t.position.x, ship.y - t.position.y) < 3.1f)
                    {
                        t.pop = 1.0f;
                        Add(0.1f);
                        ++events.ringsPassed;
                        ++ringsPassed;
                    }
                    else
                    {
                        Add(-0.08f);
                        ++events.ringsMissed;
                        ++ringsMissed;
                    }
                }
                break;
            case Kind::Orb:
            {
                const glm::vec3 d = t.position - ship;
                if (glm::dot(d, d) < 2.2f)
                {
                    t.hit = true;
                    Add(0.025f);
                    ++chain;
                    ++events.orbs;
                    ++orbsCollected;
                }
                break;
            }
            case Kind::Rock:
                if (glm::distance(t.position, ship) < t.radius + 0.9f)
                {
                    t.hit = true;
                    Add(-0.25f);
                    chain = 0;
                    events.rockHit = true;
                    ++rocksHit;
                }
                break;
            }
        }
        Add(-dt * 0.012f); // gentle decay keeps you engaged
        return events;
    }
}
