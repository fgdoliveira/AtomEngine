#include "SpeedField.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace Drift
{
    SpeedField::SpeedField(std::uint32_t seed)
    {
        std::mt19937 random(seed);
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        m_stars.reserve(StarCount);
        for (int i = 0; i < StarCount; ++i)
        {
            m_stars.push_back({ (unit(random) - 0.5f) * 200.0f, (unit(random) - 0.5f) * 140.0f, -unit(random) * 400.0f });
        }
        m_streaks.reserve(StreakCount);
        for (int i = 0; i < StreakCount; ++i)
        {
            const float angle = unit(random) * glm::two_pi<float>();
            const float radius = 5.0f + unit(random) * 20.0f;
            m_streaks.push_back({ std::cos(angle) * radius, std::sin(angle) * radius, -unit(random) * 120.0f });
        }
    }

    void SpeedField::Update(float dt, float forward)
    {
        for (glm::vec3& star : m_stars)
        {
            star.z += forward * dt * 0.15f;
            if (star.z > 0.0f)
            {
                star.z -= 400.0f;
            }
        }
        for (glm::vec3& streak : m_streaks)
        {
            streak.z += forward * dt * 1.2f;
            if (streak.z > 10.0f)
            {
                streak.z -= 130.0f;
            }
        }
    }

    float SpeedField::StreakOpacity(float forward)
    {
        return std::min(0.55f, std::max(0.0f, (forward - 45.0f) / 60.0f));
    }
}
