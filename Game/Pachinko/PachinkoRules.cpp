#include "Pachinko/PachinkoRules.h"

#include <algorithm>

namespace AtomGame
{
    PachinkoRules::PachinkoRules(RulesSettings settings, std::uint32_t seed)
        : m_settings(settings)
        , m_state_rng(seed * 2246822519u + 3u)
    {
    }

    float PachinkoRules::Random()
    {
        m_state_rng ^= m_state_rng << 13;
        m_state_rng ^= m_state_rng >> 17;
        m_state_rng ^= m_state_rng << 5;
        return static_cast<float>(m_state_rng & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    bool PachinkoRules::DrawHit()
    {
        return Random() * static_cast<float>(m_settings.odds) < 1.0f;
    }

    void PachinkoRules::OnStartPocket()
    {
        // Up to maxHeld spins wait their turn; past that a ball still pays
        // its balls, but its spin is lost - as on real machines.
        m_held = std::min(m_held + 1, m_settings.maxHeld);
    }

    void PachinkoRules::OnAttacker()
    {
        if (m_state == State::Round)
        {
            ++m_ballsThisRound;
        }
    }

    void PachinkoRules::StartSpin()
    {
        --m_held;
        ++m_spins;
        m_hit = DrawHit();
        const auto digit = [&] { return std::min(9, static_cast<int>(Random() * 10.0f)); };
        if (m_hit)
        {
            const int d = digit();
            m_target = { d, d, d };
            m_reach = true; // a hit always goes through a reach
            ++m_hits;
        }
        else
        {
            m_reach = Random() < m_settings.reachChance;
            const int a = digit();
            // A reach shows the first two matching and the last one off by
            // one; a plain miss never matches the first two.
            const int b = m_reach ? a : (a + 1 + std::min(8, static_cast<int>(Random() * 9.0f))) % 10;
            const int c = m_reach ? (a + (Random() < 0.5f ? 1 : 9)) % 10 : digit();
            m_target = { a, b, c };
        }
        m_stopped = { false, false, false };
        m_state = State::Spinning;
        m_time = 0.0f;
        m_events.push_back(Event::SpinStart);
    }

    void PachinkoRules::Update(float seconds)
    {
        m_events.clear();
        m_time += seconds;
        ++m_ticks;
        const RulesSettings& s = m_settings;

        switch (m_state)
        {
        case State::Idle:
            if (m_held > 0)
            {
                StartSpin();
            }
            break;

        case State::Spinning:
        {
            // Reels stop left to right; on a reach the last one hangs on.
            const float stops[3] = { s.spinSeconds * 0.4f, s.spinSeconds * 0.65f,
                                     s.spinSeconds + (m_reach ? s.reachSeconds : 0.0f) };
            for (int i = 0; i < 3; ++i)
            {
                if (!m_stopped[i] && m_time >= stops[i])
                {
                    m_stopped[i] = true;
                    m_shown[i] = m_target[i];
                    m_events.push_back(Event::ReelStop);
                    if (i == 1 && m_reach)
                    {
                        m_events.push_back(Event::Reach);
                    }
                }
                else if (!m_stopped[i])
                {
                    // Spinning: the digit rolls every few ticks (slower on a reach).
                    const std::uint32_t every = (i == 2 && m_reach && m_stopped[1]) ? 8u : 3u;
                    if (m_ticks % every == 0)
                    {
                        m_shown[i] = (m_shown[i] + 1) % 10;
                    }
                }
            }
            if (m_stopped[2])
            {
                m_events.push_back(m_hit ? Event::Hit : Event::Miss);
                m_state = State::Result;
                m_time = 0.0f;
            }
            break;
        }

        case State::Result:
            if (m_time >= s.resultSeconds)
            {
                if (m_hit)
                {
                    m_events.push_back(Event::FeverStart);
                    m_round = 1;
                    m_ballsThisRound = 0;
                    m_state = State::Round;
                    m_events.push_back(Event::RoundStart);
                }
                else
                {
                    m_state = State::Idle;
                }
                m_time = 0.0f;
            }
            break;

        case State::Round:
            if (m_ballsThisRound >= s.ballsPerRound || m_time >= s.roundSeconds)
            {
                m_events.push_back(Event::RoundEnd);
                m_time = 0.0f;
                if (m_round >= s.feverRounds)
                {
                    m_events.push_back(Event::FeverEnd);
                    m_round = 0;
                    m_state = State::Idle;
                }
                else
                {
                    m_state = State::Interval;
                }
            }
            break;

        case State::Interval:
            if (m_time >= s.intervalSeconds)
            {
                ++m_round;
                m_ballsThisRound = 0;
                m_state = State::Round;
                m_time = 0.0f;
                m_events.push_back(Event::RoundStart);
            }
            break;
        }
    }
}
