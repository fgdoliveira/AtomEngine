#pragma once

#include <glm/vec2.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace Arena
{
    // How enemies and projectiles find who they touch.
    enum class CollisionMode
    {
        Naive, // every pair: O(n²) - the measured baseline, not the default
        Grid,  // a uniform grid of cells one contact distance wide
    };

    // What a run simulates. Every workload can be switched on its own, so
    // each can be measured apart from the others.
    struct ArenaConfig
    {
        std::uint32_t seed = 42;
        int enemies = 100;
        int projectiles = 0;
        bool updateEnemies = true;  // A: steering toward the target
        bool collide = true;        // B: enemy-enemy and projectile-enemy contacts
        bool updateProjectiles = true; // C: projectiles fly, hit, respawn
        CollisionMode collision = CollisionMode::Grid;
        float arenaHalf = 12.0f;    // the floor is [-half, half]² metres (inside the stage's walls)
        float enemyRadius = 0.4f;
        float projectileRadius = 0.1f;
        float enemySpeed = 2.5f;    // m/s
        float projectileSpeed = 14.0f;
    };

    struct Enemy
    {
        glm::vec2 position{ 0.0f };
        glm::vec2 velocity{ 0.0f };
    };

    struct Projectile
    {
        glm::vec2 position{ 0.0f };
        glm::vec2 velocity{ 0.0f };
    };

    // ATOM ARENA's simulation (v0.0.15, M93): enemies on a square floor
    // close in on a target at the centre and push each other apart;
    // projectiles fly out from the centre, and an enemy hit is sent back to
    // the edge. Pure and seeded: no GPU, no clock, no allocation per step
    // once warm - the same seed and steps give the same state, bit for bit,
    // whichever collision mode finds the contacts.
    class ArenaSim
    {
    public:
        explicit ArenaSim(const ArenaConfig& config = {});

        void Reset(const ArenaConfig& config);
        void Step(float dt);
        // D: the spike - n more enemies at once, at the edge.
        void SpawnBatch(int count);
        // Back to `count` enemies (the newest go): the spike's end.
        void Truncate(int count);

        const ArenaConfig& Config() const { return m_config; }
        ArenaConfig& EditConfig() { return m_config; } // workload switches, live
        const std::vector<Enemy>& Enemies() const { return m_enemies; }
        const std::vector<Projectile>& Projectiles() const { return m_projectiles; }
        // The last step's contacts (index pairs, sorted) and the hits so far.
        const std::vector<std::pair<int, int>>& EnemyContacts() const { return m_enemyContacts; }
        std::size_t LastHits() const { return m_projectileHits.size(); }
        std::uint64_t TotalHits() const { return m_totalHits; }
        std::uint64_t Steps() const { return m_steps; }
        // A digest of every position and velocity, to compare runs.
        std::uint64_t StateHash() const;

        // Exposed for tests: the contacts as each mode finds them.
        void FindEnemyContacts(std::vector<std::pair<int, int>>& out) const;
        void FindProjectileHits(std::vector<std::pair<int, int>>& out) const;

    private:
        float Random01();                // xorshift32, seeded
        glm::vec2 EdgePoint();
        Enemy NewEnemy();
        Projectile NewProjectile();
        void UpdateEnemies(float dt);
        void UpdateProjectiles(float dt);
        void Collide();
        void BuildGrid(float cellSize) const;

        ArenaConfig m_config;
        std::uint32_t m_random = 1;
        std::vector<Enemy> m_enemies;
        std::vector<Projectile> m_projectiles;
        std::vector<std::pair<int, int>> m_enemyContacts;
        std::vector<std::pair<int, int>> m_projectileHits; // (projectile, enemy)
        std::uint64_t m_totalHits = 0;
        std::uint64_t m_steps = 0;
        // The grid, rebuilt each step: enemies sorted by cell (counting sort).
        mutable int m_gridSide = 0;
        mutable float m_gridCell = 1.0f;
        mutable std::vector<int> m_cellStart; // per cell, into m_cellItems; size side²+1
        mutable std::vector<int> m_cellItems;
        mutable std::vector<int> m_itemCell;  // per enemy, its cell
        mutable std::vector<int> m_cursor;    // per cell, the next slot to fill
        mutable std::vector<int> m_neighbours; // one enemy's contacts, sorted (M96)
    };
}
