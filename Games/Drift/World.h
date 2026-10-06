#pragma once

// DRIFT's world and rules (M78), ported from the three.js original
// (src/world.js World, src/interact.js Flow) with every constant kept.
// Pure and seeded: the same seed gives the same course, for tests; play
// seeds from the clock.

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <random>
#include <vector>

namespace Drift
{
    inline constexpr float SegmentLength = 80.0f; // between ring gates
    inline constexpr float Ahead = 720.0f;        // how far ahead content exists
    inline constexpr float Behind = 20.0f;        // recycled past this

    enum class Kind { Ring, Orb, Rock };

    struct Thing
    {
        Kind kind = Kind::Ring;
        glm::vec3 position{ 0.0f };
        glm::vec3 rotation{ 0.0f }; // Euler XYZ
        float scale = 1.0f;         // the authored size (rocks vary)
        float drawScale = 1.0f;     // this frame's, with animation
        float radius = 0.0f;        // rocks: collision radius
        glm::vec3 spin{ 0.0f };     // rocks: radians per second
        float pop = 0.0f;           // rings: 1 on a pass, decays; orbs: burst progress
        bool hit = false;
        bool visible = true;
        glm::mat4 Matrix() const;
    };

    class World
    {
    public:
        explicit World(std::uint32_t seed);

        // Spawns ahead of `shipZ`, recycles what's behind, animates.
        void Update(float dt, float time, float shipZ);

        std::vector<Thing>& Things() { return m_things; }
        const std::vector<Thing>& Things() const { return m_things; }
        int Segments() const { return m_segment; }

    private:
        void SpawnSegment(float z);
        float Random() { return m_unit(m_random); } // 0..1, like Math.random

        std::vector<Thing> m_things;
        float m_nextZ = -60.0f;
        int m_segment = 0;
        std::mt19937 m_random;
        std::uniform_real_distribution<float> m_unit{ 0.0f, 1.0f };
    };

    // What a frame's collisions did, for sound (M80), the HUD and the camera.
    struct FlowEvents
    {
        int ringsPassed = 0;
        int ringsMissed = 0;
        int orbs = 0;        // collected this frame
        bool rockHit = false;
    };

    // The one value everything follows (0..1), and the orb chain.
    class Flow
    {
    public:
        // Rules for the frame the ship moved from `previousZ` to `ship`.
        FlowEvents Check(World& world, const glm::vec3& ship, float previousZ, float dt);

        float value = 0.0f;
        int chain = 0;
        // Totals, for the autopilot's summary and tests.
        int ringsPassed = 0;
        int ringsMissed = 0;
        int orbsCollected = 0;
        int rocksHit = 0;

    private:
        void Add(float amount);
    };
}
