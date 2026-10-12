#pragma once

// DRIFT's sense of speed (M79), ported from the three.js original
// (src/world.js makeStars, update): 1500 stars drifting past in a box
// that rides with the ship, and 120 speed streaks that stretch and show
// with forward speed. Pure; the renderer turns them into particles.

#include <glm/vec3.hpp>

#include <cstdint>
#include <random>
#include <vector>

namespace Drift
{
    class SpeedField
    {
    public:
        static constexpr int StarCount = 1500;
        static constexpr int StreakCount = 120;

        explicit SpeedField(std::uint32_t seed);

        // Advances by this frame's forward speed (m/s).
        void Update(float dt, float forward);

        // Positions relative to their box's origin: the stars' box sits at
        // (ship.x * 0.9, ship.y * 0.9, ship.z), the streaks' at the ship.
        const std::vector<glm::vec3>& Stars() const { return m_stars; }
        const std::vector<glm::vec3>& Streaks() const { return m_streaks; } // a streak's head

        // The streaks' length (m) and opacity at this forward speed.
        static float StreakLength(float forward) { return forward * 0.08f; }
        static float StreakOpacity(float forward);

    private:
        std::vector<glm::vec3> m_stars;
        std::vector<glm::vec3> m_streaks;
    };
}
