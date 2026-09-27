#include "Core/Time.h"

#include <SDL3/SDL.h>

#include <algorithm>

namespace Atom
{
    void Time::Reset()
    {
        m_lastCounter = SDL_GetPerformanceCounter();
        m_deltaSeconds = 0.0f;
        m_elapsedSeconds = 0.0;
    }

    float Time::Tick()
    {
        const std::uint64_t counter = SDL_GetPerformanceCounter();
        const double frequency =
            static_cast<double>(SDL_GetPerformanceFrequency());
        const double delta =
            static_cast<double>(counter - m_lastCounter) / frequency;

        m_lastCounter = counter;
        m_deltaSeconds = std::min(static_cast<float>(delta), MaxDeltaSeconds);
        m_elapsedSeconds += m_deltaSeconds;

        return m_deltaSeconds;
    }
}
