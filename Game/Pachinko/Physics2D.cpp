#include "Pachinko/Physics2D.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace AtomGame
{
    glm::vec2 ClosestOnSegment(glm::vec2 p, glm::vec2 a, glm::vec2 b)
    {
        const glm::vec2 ab = b - a;
        const float length2 = glm::dot(ab, ab);
        if (length2 <= 1e-12f)
        {
            return a;
        }
        const float t = std::clamp(glm::dot(p - a, ab) / length2, 0.0f, 1.0f);
        return a + ab * t;
    }

    void World2D::AddNail(const Nail& nail)
    {
        m_nails.push_back(nail);
        m_gridDirty = true;
    }

    std::size_t World2D::AddSegment(const Segment& segment)
    {
        m_segments.push_back(segment);
        m_segmentEnabled.push_back(1);
        m_gridDirty = true;
        return m_segments.size() - 1;
    }

    std::uint32_t World2D::AddBall(glm::vec2 position, glm::vec2 velocity)
    {
        const std::uint32_t id = m_nextId++;
        m_balls.push_back({ id, position, velocity });
        return id;
    }

    void World2D::RemoveBall(std::uint32_t id)
    {
        std::erase_if(m_balls, [id](const Ball& ball) { return ball.id == id; });
    }

    void World2D::Step(float seconds)
    {
        m_impacts.clear();
        if (m_gridDirty)
        {
            BuildGrid();
        }
        const int substeps = std::max(0, static_cast<int>(std::lround(seconds / m_settings.substep)));
        for (int i = 0; i < substeps; ++i)
        {
            Substep(m_settings.substep);
        }
    }

    void World2D::Substep(float dt)
    {
        for (Ball& ball : m_balls)
        {
            ball.velocity += m_settings.gravity * dt;
            const float speed = glm::length(ball.velocity);
            if (speed > m_settings.maxSpeed)
            {
                ball.velocity *= m_settings.maxSpeed / speed;
            }
            ball.position += ball.velocity * dt;
        }
        for (Ball& ball : m_balls)
        {
            CollideStatic(ball);
        }
        CollideBalls();
    }

    void World2D::Resolve(Ball& ball, glm::vec2 normal, float penetration, float restitution, Impact::Kind kind)
    {
        ball.position += normal * penetration;
        const float approach = glm::dot(ball.velocity, normal);
        if (approach >= 0.0f)
        {
            return; // already separating
        }
        // A ball resting on a rail approaches by about one substep of
        // gravity every substep; bouncing that back would make it jitter.
        const float restThreshold = glm::length(m_settings.gravity) * m_settings.substep * 3.0f;
        const float bounce = -approach > restThreshold ? restitution : 0.0f;
        ball.velocity -= (1.0f + bounce) * approach * normal;
        // Friction: lose a little of the sliding speed on every contact.
        const glm::vec2 tangent = ball.velocity - glm::dot(ball.velocity, normal) * normal;
        ball.velocity -= tangent * m_settings.friction;
        if (-approach >= MinImpactSpeed)
        {
            m_impacts.push_back({ kind, ball.id, -approach, ball.position - normal * m_settings.ballRadius });
        }
    }

    void World2D::CollideStatic(Ball& ball)
    {
        const float r = m_settings.ballRadius;
        const int x0 = static_cast<int>(std::floor((ball.position.x - r - m_gridMin.x) / m_settings.gridCell));
        const int y0 = static_cast<int>(std::floor((ball.position.y - r - m_gridMin.y) / m_settings.gridCell));
        const int x1 = static_cast<int>(std::floor((ball.position.x + r - m_gridMin.x) / m_settings.gridCell));
        const int y1 = static_cast<int>(std::floor((ball.position.y + r - m_gridMin.y) / m_settings.gridCell));
        ++m_query;
        for (int y = std::max(y0, 0); y <= std::min(y1, m_gridHeight - 1); ++y)
        {
            for (int x = std::max(x0, 0); x <= std::min(x1, m_gridWidth - 1); ++x)
            {
                for (const int shape : m_cells[static_cast<std::size_t>(y * m_gridWidth + x)])
                {
                    // A shape spanning several cells is tested once.
                    const std::size_t mark = shape >= 0 ? static_cast<std::size_t>(shape)
                                                        : m_nails.size() + static_cast<std::size_t>(-1 - shape);
                    if (m_marks[mark] == m_query)
                    {
                        continue;
                    }
                    m_marks[mark] = m_query;

                    glm::vec2 closest;
                    float reach;
                    Impact::Kind kind;
                    float restitution;
                    if (shape >= 0)
                    {
                        const Nail& nail = m_nails[static_cast<std::size_t>(shape)];
                        closest = nail.position;
                        reach = r + nail.radius;
                        kind = Impact::Kind::Nail;
                        restitution = m_settings.nailRestitution;
                    }
                    else
                    {
                        if (!m_segmentEnabled[static_cast<std::size_t>(-1 - shape)])
                        {
                            continue;
                        }
                        const Segment& segment = m_segments[static_cast<std::size_t>(-1 - shape)];
                        closest = ClosestOnSegment(ball.position, segment.a, segment.b);
                        reach = r;
                        kind = Impact::Kind::Wall;
                        restitution = m_settings.wallRestitution;
                    }
                    const glm::vec2 offset = ball.position - closest;
                    const float distance = glm::length(offset);
                    if (distance >= reach)
                    {
                        continue;
                    }
                    // Dead centre: push straight up, the likeliest way out.
                    const glm::vec2 normal = distance > 1e-5f ? offset / distance : glm::vec2{ 0.0f, -1.0f };
                    Resolve(ball, normal, reach - distance, restitution, kind);
                }
            }
        }
    }

    void World2D::CollideBalls()
    {
        const float reach = 2.0f * m_settings.ballRadius;
        for (std::size_t i = 0; i < m_balls.size(); ++i)
        {
            for (std::size_t j = i + 1; j < m_balls.size(); ++j)
            {
                Ball& a = m_balls[i];
                Ball& b = m_balls[j];
                const glm::vec2 offset = a.position - b.position;
                const float distance = glm::length(offset);
                if (distance >= reach || distance < 1e-5f)
                {
                    continue;
                }
                const glm::vec2 normal = offset / distance;
                const float penetration = reach - distance;
                a.position += normal * (penetration * 0.5f);
                b.position -= normal * (penetration * 0.5f);
                // Equal masses: exchange the normal velocity difference.
                const float approach = glm::dot(a.velocity - b.velocity, normal);
                if (approach < 0.0f)
                {
                    const float impulse = -(1.0f + m_settings.ballRestitution) * approach * 0.5f;
                    a.velocity += impulse * normal;
                    b.velocity -= impulse * normal;
                    if (-approach >= MinImpactSpeed)
                    {
                        m_impacts.push_back({ Impact::Kind::Ball, a.id, -approach, (a.position + b.position) * 0.5f });
                    }
                }
            }
        }
    }

    void World2D::BuildGrid()
    {
        m_gridDirty = false;
        glm::vec2 low{ 1e9f }, high{ -1e9f };
        const auto grow = [&](glm::vec2 p, float r) {
            low = glm::min(low, p - glm::vec2{ r });
            high = glm::max(high, p + glm::vec2{ r });
        };
        for (const Nail& nail : m_nails) grow(nail.position, nail.radius);
        for (const Segment& segment : m_segments)
        {
            grow(segment.a, 0.0f);
            grow(segment.b, 0.0f);
        }
        if (m_nails.empty() && m_segments.empty())
        {
            low = high = glm::vec2{ 0.0f };
        }
        const float margin = m_settings.ballRadius + 1.0f;
        m_gridMin = low - glm::vec2{ margin };
        const glm::vec2 size = high - low + glm::vec2{ 2.0f * margin };
        m_gridWidth = std::max(1, static_cast<int>(std::ceil(size.x / m_settings.gridCell)));
        m_gridHeight = std::max(1, static_cast<int>(std::ceil(size.y / m_settings.gridCell)));
        m_cells.assign(static_cast<std::size_t>(m_gridWidth * m_gridHeight), {});
        m_marks.assign(m_nails.size() + m_segments.size(), 0);
        m_query = 0;

        const auto insert = [&](glm::vec2 lo, glm::vec2 hi, int shape) {
            const int cx0 = std::max(0, static_cast<int>(std::floor((lo.x - m_gridMin.x) / m_settings.gridCell)));
            const int cy0 = std::max(0, static_cast<int>(std::floor((lo.y - m_gridMin.y) / m_settings.gridCell)));
            const int cx1 = std::min(m_gridWidth - 1, static_cast<int>(std::floor((hi.x - m_gridMin.x) / m_settings.gridCell)));
            const int cy1 = std::min(m_gridHeight - 1, static_cast<int>(std::floor((hi.y - m_gridMin.y) / m_settings.gridCell)));
            for (int y = cy0; y <= cy1; ++y)
            {
                for (int x = cx0; x <= cx1; ++x)
                {
                    m_cells[static_cast<std::size_t>(y * m_gridWidth + x)].push_back(shape);
                }
            }
        };
        for (std::size_t i = 0; i < m_nails.size(); ++i)
        {
            const Nail& nail = m_nails[i];
            insert(nail.position - glm::vec2{ nail.radius }, nail.position + glm::vec2{ nail.radius }, static_cast<int>(i));
        }
        for (std::size_t i = 0; i < m_segments.size(); ++i)
        {
            const Segment& s = m_segments[i];
            insert(glm::min(s.a, s.b), glm::max(s.a, s.b), -1 - static_cast<int>(i));
        }
    }
}
