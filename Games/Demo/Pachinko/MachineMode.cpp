#include "Pachinko/MachineMode.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace AtomGame
{
    namespace
    {
        float Smooth(float t)
        {
            t = std::clamp(t, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        // Shortest way round, so a turn from -170 to 170 degrees is 20, not 340.
        float LerpAngle(float a, float b, float t)
        {
            const float difference = std::remainder(b - a, glm::two_pi<float>());
            return a + difference * t;
        }
    }

    void MachineMode::Enter(const CameraPose& from, const CameraPose& to)
    {
        if (m_phase != Phase::Inactive)
        {
            return;
        }
        m_from = from;
        m_to = to;
        m_phase = Phase::Entering;
        m_time = 0.0f;
    }

    void MachineMode::Leave()
    {
        if (m_phase == Phase::Playing)
        {
            m_phase = Phase::Leaving;
            m_time = 0.0f;
        }
    }

    void MachineMode::Update(float deltaSeconds)
    {
        if (m_phase == Phase::Inactive || m_phase == Phase::Playing)
        {
            return;
        }
        m_time += deltaSeconds;
        if (m_time >= MoveSeconds + FadeSeconds)
        {
            m_phase = m_phase == Phase::Entering ? Phase::Playing : Phase::Inactive;
            m_time = 0.0f;
        }
    }

    float MachineMode::Progress() const
    {
        switch (m_phase)
        {
        case Phase::Entering: return Smooth(m_time / MoveSeconds);
        case Phase::Playing: return 1.0f;
        case Phase::Leaving: return 1.0f - Smooth((m_time - FadeSeconds) / MoveSeconds); // fade out first
        default: return 0.0f;
        }
    }

    CameraPose MachineMode::GetCamera() const
    {
        const float t = Progress();
        return CameraPose{
            m_from.position + (m_to.position - m_from.position) * t,
            LerpAngle(m_from.yaw, m_to.yaw, t),
            m_from.pitch + (m_to.pitch - m_from.pitch) * t,
        };
    }

    float MachineMode::GetFade() const
    {
        switch (m_phase)
        {
        case Phase::Entering: return std::clamp((m_time - MoveSeconds) / FadeSeconds, 0.0f, 1.0f);
        case Phase::Playing: return 1.0f;
        case Phase::Leaving: return 1.0f - std::clamp(m_time / FadeSeconds, 0.0f, 1.0f);
        default: return 0.0f;
        }
    }

    PixelLayout FitIntegerScale(glm::vec2 window, int width, int height)
    {
        PixelLayout layout;
        layout.scale = std::max(1, static_cast<int>(std::min(window.x / width, window.y / height)));
        layout.size = { static_cast<float>(width * layout.scale), static_cast<float>(height * layout.scale) };
        layout.position = { std::floor((window.x - layout.size.x) * 0.5f), std::floor((window.y - layout.size.y) * 0.5f) };
        return layout;
    }
}
