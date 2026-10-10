#include "ArenaSim.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Arena
{
    ArenaSim::ArenaSim(const ArenaConfig& config)
    {
        Reset(config);
    }

    void ArenaSim::Reset(const ArenaConfig& config)
    {
        m_config = config;
        m_random = config.seed ? config.seed : 1u;
        m_enemies.clear();
        m_projectiles.clear();
        m_enemyContacts.clear();
        m_projectileHits.clear();
        m_totalHits = 0;
        m_steps = 0;
        for (int i = 0; i < config.enemies; ++i)
        {
            m_enemies.push_back(NewEnemy());
        }
        for (int i = 0; i < config.projectiles; ++i)
        {
            m_projectiles.push_back(NewProjectile());
        }
    }

    float ArenaSim::Random01()
    {
        // xorshift32: tiny, and the same sequence on every platform.
        m_random ^= m_random << 13;
        m_random ^= m_random >> 17;
        m_random ^= m_random << 5;
        return static_cast<float>(m_random >> 8) / static_cast<float>(1u << 24);
    }

    glm::vec2 ArenaSim::EdgePoint()
    {
        // A point on the floor's border, any side.
        const float half = m_config.arenaHalf - m_config.enemyRadius;
        const float along = (Random01() * 2.0f - 1.0f) * half;
        switch (static_cast<int>(Random01() * 4.0f) & 3)
        {
        case 0: return { along, -half };
        case 1: return { along, half };
        case 2: return { -half, along };
        default: return { half, along };
        }
    }

    Enemy ArenaSim::NewEnemy()
    {
        Enemy enemy;
        enemy.position = EdgePoint();
        return enemy;
    }

    Projectile ArenaSim::NewProjectile()
    {
        // From near the centre, in a random direction.
        const float angle = Random01() * 6.2831853f;
        Projectile projectile;
        projectile.velocity = glm::vec2{ std::cos(angle), std::sin(angle) } * m_config.projectileSpeed;
        projectile.position = projectile.velocity * (Random01() * 0.05f);
        return projectile;
    }

    void ArenaSim::SpawnBatch(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            m_enemies.push_back(NewEnemy());
        }
    }

    void ArenaSim::Step(float dt)
    {
        ++m_steps;
        if (m_config.updateEnemies)
        {
            UpdateEnemies(dt);
        }
        if (m_config.updateProjectiles)
        {
            UpdateProjectiles(dt);
        }
        if (m_config.collide)
        {
            Collide();
        }
        else
        {
            m_enemyContacts.clear(); // none looked for this step
            m_projectileHits.clear();
        }
    }

    void ArenaSim::UpdateEnemies(float dt)
    {
        // Seek the target (the centre), stop short of it.
        const float stopAt = 2.0f;
        for (Enemy& enemy : m_enemies)
        {
            const float distance = glm::length(enemy.position);
            const glm::vec2 toward = distance > 1e-4f ? -enemy.position / distance : glm::vec2{ 0.0f };
            const float speed = distance > stopAt ? m_config.enemySpeed : 0.0f;
            enemy.velocity = toward * speed;
            enemy.position += enemy.velocity * dt;
        }
    }

    void ArenaSim::UpdateProjectiles(float dt)
    {
        const float half = m_config.arenaHalf;
        for (Projectile& projectile : m_projectiles)
        {
            projectile.position += projectile.velocity * dt;
            if (std::abs(projectile.position.x) > half || std::abs(projectile.position.y) > half)
            {
                projectile = NewProjectile(); // out of the arena: fired again
            }
        }
    }

    void ArenaSim::BuildGrid(float cellSize) const
    {
        // Enemies sorted into cells by a counting sort: two passes, no
        // allocation once the vectors have grown.
        m_gridCell = cellSize;
        m_gridSide = std::max(1, static_cast<int>(std::ceil(m_config.arenaHalf * 2.0f / cellSize)) + 2);
        const int cells = m_gridSide * m_gridSide;
        m_cellStart.assign(static_cast<std::size_t>(cells) + 1, 0);
        m_itemCell.resize(m_enemies.size());
        const auto cellOf = [&](const glm::vec2& p) {
            const int x = std::clamp(static_cast<int>((p.x + m_config.arenaHalf) / cellSize) + 1, 0, m_gridSide - 1);
            const int y = std::clamp(static_cast<int>((p.y + m_config.arenaHalf) / cellSize) + 1, 0, m_gridSide - 1);
            return y * m_gridSide + x;
        };
        for (std::size_t i = 0; i < m_enemies.size(); ++i)
        {
            m_itemCell[i] = cellOf(m_enemies[i].position);
            ++m_cellStart[static_cast<std::size_t>(m_itemCell[i]) + 1];
        }
        for (int c = 0; c < cells; ++c)
        {
            m_cellStart[static_cast<std::size_t>(c) + 1] += m_cellStart[static_cast<std::size_t>(c)];
        }
        // Second pass: each enemy into its cell's next free slot.
        m_cellItems.resize(m_enemies.size());
        m_cursor.assign(m_cellStart.begin(), m_cellStart.end() - 1);
        for (std::size_t i = 0; i < m_enemies.size(); ++i)
        {
            const int slot = m_cursor[static_cast<std::size_t>(m_itemCell[i])]++;
            m_cellItems[static_cast<std::size_t>(slot)] = static_cast<int>(i);
        }
    }

    void ArenaSim::FindEnemyContacts(std::vector<std::pair<int, int>>& out) const
    {
        out.clear();
        const float reach = m_config.enemyRadius * 2.0f;
        const float reach2 = reach * reach;
        const int count = static_cast<int>(m_enemies.size());
        if (m_config.collision == CollisionMode::Naive)
        {
            for (int i = 0; i < count; ++i)
            {
                for (int j = i + 1; j < count; ++j)
                {
                    const glm::vec2 d = m_enemies[j].position - m_enemies[i].position;
                    if (glm::dot(d, d) < reach2)
                    {
                        out.emplace_back(i, j);
                    }
                }
            }
            return; // already in (i, j) order
        }
        BuildGrid(reach);
        for (int i = 0; i < count; ++i)
        {
            const int cell = m_itemCell[static_cast<std::size_t>(i)];
            const int cx = cell % m_gridSide;
            const int cy = cell / m_gridSide;
            for (int y = std::max(cy - 1, 0); y <= std::min(cy + 1, m_gridSide - 1); ++y)
            {
                for (int x = std::max(cx - 1, 0); x <= std::min(cx + 1, m_gridSide - 1); ++x)
                {
                    const int c = y * m_gridSide + x;
                    for (int k = m_cellStart[static_cast<std::size_t>(c)]; k < m_cellStart[static_cast<std::size_t>(c) + 1]; ++k)
                    {
                        const int j = m_cellItems[static_cast<std::size_t>(k)];
                        if (j <= i)
                        {
                            continue;
                        }
                        const glm::vec2 d = m_enemies[static_cast<std::size_t>(j)].position - m_enemies[static_cast<std::size_t>(i)].position;
                        if (glm::dot(d, d) < reach2)
                        {
                            out.emplace_back(i, j);
                        }
                    }
                }
            }
        }
        // The same order as the naive search, so both resolve identically.
        std::sort(out.begin(), out.end());
    }

    void ArenaSim::FindProjectileHits(std::vector<std::pair<int, int>>& out) const
    {
        out.clear();
        const float reach = m_config.enemyRadius + m_config.projectileRadius;
        const float reach2 = reach * reach;
        const int enemies = static_cast<int>(m_enemies.size());
        const int projectiles = static_cast<int>(m_projectiles.size());
        if (m_config.collision == CollisionMode::Naive)
        {
            for (int p = 0; p < projectiles; ++p)
            {
                for (int e = 0; e < enemies; ++e)
                {
                    const glm::vec2 d = m_enemies[static_cast<std::size_t>(e)].position - m_projectiles[static_cast<std::size_t>(p)].position;
                    if (glm::dot(d, d) < reach2)
                    {
                        out.emplace_back(p, e);
                    }
                }
            }
            return;
        }
        BuildGrid(m_config.enemyRadius * 2.0f); // cells wider than `reach`: neighbours suffice
        for (int p = 0; p < projectiles; ++p)
        {
            const glm::vec2 at = m_projectiles[static_cast<std::size_t>(p)].position;
            const int cx = std::clamp(static_cast<int>((at.x + m_config.arenaHalf) / m_gridCell) + 1, 0, m_gridSide - 1);
            const int cy = std::clamp(static_cast<int>((at.y + m_config.arenaHalf) / m_gridCell) + 1, 0, m_gridSide - 1);
            for (int y = std::max(cy - 1, 0); y <= std::min(cy + 1, m_gridSide - 1); ++y)
            {
                for (int x = std::max(cx - 1, 0); x <= std::min(cx + 1, m_gridSide - 1); ++x)
                {
                    const int c = y * m_gridSide + x;
                    for (int k = m_cellStart[static_cast<std::size_t>(c)]; k < m_cellStart[static_cast<std::size_t>(c) + 1]; ++k)
                    {
                        const int e = m_cellItems[static_cast<std::size_t>(k)];
                        const glm::vec2 d = m_enemies[static_cast<std::size_t>(e)].position - at;
                        if (glm::dot(d, d) < reach2)
                        {
                            out.emplace_back(p, e);
                        }
                    }
                }
            }
        }
        std::sort(out.begin(), out.end());
    }

    void ArenaSim::Collide()
    {
        // Enemies push each other apart, half the overlap each.
        FindEnemyContacts(m_enemyContacts);
        const float reach = m_config.enemyRadius * 2.0f;
        for (const auto& [i, j] : m_enemyContacts)
        {
            Enemy& a = m_enemies[static_cast<std::size_t>(i)];
            Enemy& b = m_enemies[static_cast<std::size_t>(j)];
            glm::vec2 d = b.position - a.position;
            float distance = glm::length(d);
            if (distance < 1e-5f)
            {
                d = { 1.0f, 0.0f }; // exactly together: part along x
                distance = 1e-5f;
            }
            const glm::vec2 push = d / distance * ((reach - distance) * 0.5f);
            a.position -= push;
            b.position += push;
        }
        // The walls: a crowd pushed outward stays on the floor.
        const float limit = m_config.arenaHalf - m_config.enemyRadius;
        for (Enemy& enemy : m_enemies)
        {
            enemy.position = glm::clamp(enemy.position, glm::vec2{ -limit }, glm::vec2{ limit });
        }
        // A projectile that hits an enemy sends it back to the edge and is
        // fired again. Each projectile counts its first hit only.
        FindProjectileHits(m_projectileHits);
        int lastProjectile = -1;
        for (const auto& [p, e] : m_projectileHits)
        {
            if (p == lastProjectile)
            {
                continue;
            }
            lastProjectile = p;
            m_enemies[static_cast<std::size_t>(e)] = NewEnemy();
            m_projectiles[static_cast<std::size_t>(p)] = NewProjectile();
            ++m_totalHits;
        }
    }

    std::uint64_t ArenaSim::StateHash() const
    {
        // FNV-1a over the raw floats: equal only if every bit is.
        std::uint64_t hash = 1469598103934665603ull;
        const auto mix = [&](const void* data, std::size_t size) {
            const auto* bytes = static_cast<const unsigned char*>(data);
            for (std::size_t i = 0; i < size; ++i)
            {
                hash = (hash ^ bytes[i]) * 1099511628211ull;
            }
        };
        if (!m_enemies.empty()) mix(m_enemies.data(), m_enemies.size() * sizeof(Enemy));
        if (!m_projectiles.empty()) mix(m_projectiles.data(), m_projectiles.size() * sizeof(Projectile));
        mix(&m_totalHits, sizeof(m_totalHits));
        return hash;
    }
}
