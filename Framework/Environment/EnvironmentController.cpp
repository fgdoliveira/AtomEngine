#include "Environment/EnvironmentController.h"

#include <algorithm>

namespace AtomFramework
{
    void EnvironmentController::Reset(const EnvironmentState& state)
    {
        m_from = m_to = m_current = state;
        m_duration = m_elapsed = 0.0f;
    }

    void EnvironmentController::SwitchTo(const EnvironmentState& target, float seconds)
    {
        if (seconds <= 0.0f)
        {
            Reset(target);
            return;
        }
        m_from = m_current; // from wherever it is, so a change of mind doesn't jump
        m_to = target;
        m_duration = seconds;
        m_elapsed = 0.0f;
    }

    void EnvironmentController::Update(float deltaSeconds)
    {
        if (!IsTransitioning())
        {
            return;
        }
        m_elapsed = std::min(m_elapsed + std::max(deltaSeconds, 0.0f), m_duration);
        // Eased in and out: weather drifts in rather than starting and
        // stopping on a step.
        const float t = Progress();
        m_current = Blend(m_from, m_to, t * t * (3.0f - 2.0f * t));
        if (!IsTransitioning())
        {
            m_current = m_to; // exactly the target at the end
        }
    }

    float EnvironmentController::Progress() const
    {
        return m_duration > 0.0f ? std::clamp(m_elapsed / m_duration, 0.0f, 1.0f) : 1.0f;
    }
}
