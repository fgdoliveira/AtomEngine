#pragma once

#include "Level/Environment.h"

namespace AtomGame
{
    // Weather as authored state, not meteorology (M49): the environment now,
    // and optionally a transition toward another one over some seconds.
    // Deterministic - the same switches and time steps give the same states,
    // whatever the frame rate - and nothing but numbers, so it's tested
    // without a renderer.
    class EnvironmentController
    {
    public:
        // Jumps there: no transition.
        void Reset(const EnvironmentState& state);

        // Blends from what's showing now (mid-transition included) to
        // `target` over `seconds`; 0 switches at once.
        void SwitchTo(const EnvironmentState& target, float seconds);

        void Update(float deltaSeconds);

        const EnvironmentState& Current() const { return m_current; }
        const EnvironmentState& Target() const { return m_to; }
        bool IsTransitioning() const { return m_elapsed < m_duration; }
        float Progress() const; // 0..1; 1 when not transitioning

    private:
        EnvironmentState m_from;
        EnvironmentState m_to;
        EnvironmentState m_current;
        float m_duration = 0.0f;
        float m_elapsed = 0.0f;
    };
}
