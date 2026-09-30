#pragma once

#include <glm/vec2.hpp>

#include <cstdint>
#include <vector>

namespace AtomGame
{
    // 2D rigid-body physics for the pachinko field (M30). Units are field
    // pixels (the 320x240 screen), y down, seconds.
    //
    // Shapes: balls (dynamic circles), nails (static circles) and segments
    // (static walls, rails, pocket lips). Each Step advances a fixed number
    // of equal substeps; each substep integrates gravity, then resolves every
    // contact by pushing the ball out along the contact normal and removing
    // the approaching velocity (restitution: the share that bounces back;
    // friction: the share of sliding speed lost). Balls also collide with
    // each other, as equal masses.
    //
    // Tunnelling: a fast ball could skip past a thin nail between two
    // substeps. Speeds are capped, and substeps are short enough that a ball
    // never moves more than a third of its radius per substep, so every
    // contact is seen.
    //
    // Deterministic: fixed iteration order, no dependence on frame time
    // (the caller steps it from a FixedStep). Same inputs, same bits.
    struct PhysicsSettings
    {
        glm::vec2 gravity{ 0.0f, 500.0f };
        float substep = 1.0f / 480.0f;
        float maxSpeed = 480.0f;
        float ballRadius = 3.0f;
        float nailRestitution = 0.45f;
        float wallRestitution = 0.35f;
        float ballRestitution = 0.6f;
        float friction = 0.0005f;   // per contact substep: a ball riding a rail touches it
                                    // 480 times a second, so this is ~20 % of its speed per second
        float gridCell = 16.0f;     // broad phase
    };

    struct Ball
    {
        std::uint32_t id = 0;
        glm::vec2 position{ 0.0f };
        glm::vec2 velocity{ 0.0f };
    };

    struct Nail
    {
        glm::vec2 position{ 0.0f };
        float radius = 1.0f;
    };

    struct Segment
    {
        glm::vec2 a{ 0.0f };
        glm::vec2 b{ 0.0f };
    };

    // A contact worth a sound: a ball hitting something at `speed`.
    struct Impact
    {
        enum class Kind { Nail, Wall, Ball };
        Kind kind = Kind::Nail;
        std::uint32_t ball = 0;
        float speed = 0.0f;       // approach speed along the normal
        glm::vec2 position{ 0.0f };
    };

    class World2D
    {
    public:
        explicit World2D(PhysicsSettings settings = {}) : m_settings(settings) {}

        void AddNail(const Nail& nail);
        std::size_t AddSegment(const Segment& segment); // returns its index
        // A segment switched off is ignored (a gate that opens, M31).
        void SetSegmentEnabled(std::size_t index, bool enabled) { m_segmentEnabled[index] = enabled ? 1 : 0; }
        bool IsSegmentEnabled(std::size_t index) const { return m_segmentEnabled[index] != 0; }

        std::uint32_t AddBall(glm::vec2 position, glm::vec2 velocity);
        void RemoveBall(std::uint32_t id);
        const std::vector<Ball>& GetBalls() const { return m_balls; }

        // Advances `seconds` in whole substeps (rounded to the nearest).
        void Step(float seconds);

        // Impacts during the last Step, faster than `minSpeed` (px/s).
        const std::vector<Impact>& GetImpacts() const { return m_impacts; }

        const std::vector<Nail>& GetNails() const { return m_nails; }
        const std::vector<Segment>& GetSegments() const { return m_segments; }
        const PhysicsSettings& GetSettings() const { return m_settings; }

        static constexpr float MinImpactSpeed = 40.0f;

    private:
        void Substep(float dt);
        void BuildGrid();
        void CollideStatic(Ball& ball);
        void CollideBalls();
        void Resolve(Ball& ball, glm::vec2 normal, float penetration, float restitution, Impact::Kind kind);

        PhysicsSettings m_settings;
        std::vector<Nail> m_nails;
        std::vector<Segment> m_segments;
        std::vector<char> m_segmentEnabled;
        std::vector<Ball> m_balls;
        std::vector<Impact> m_impacts;
        std::uint32_t m_nextId = 1;

        // Uniform grid over the static shapes: per cell, indices into nails
        // (>= 0) and segments (encoded as -1 - index).
        bool m_gridDirty = true;
        glm::vec2 m_gridMin{ 0.0f };
        int m_gridWidth = 0;
        int m_gridHeight = 0;
        std::vector<std::vector<int>> m_cells;
        std::vector<std::uint32_t> m_marks; // per static shape: last query that saw it
        std::uint32_t m_query = 0;
    };

    // The closest point to `p` on segment ab.
    glm::vec2 ClosestOnSegment(glm::vec2 p, glm::vec2 a, glm::vec2 b);
}
