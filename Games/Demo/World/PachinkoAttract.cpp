#include "World/PachinkoAttract.h"

#include "Pachinko/PixelDraw.h"
#include "UI/UIRenderer.h"

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>

namespace Demo
{
    namespace
    {
        // The playfield on the 320x240 screen (y down).
        constexpr float Left = 20.0f, Right = 300.0f, Top = 20.0f, Bottom = 200.0f;
        // The reel window in the middle, and the start pocket under it.
        constexpr float ReelX0 = 118.0f, ReelX1 = 202.0f, ReelY0 = 70.0f, ReelY1 = 116.0f;
        constexpr float PocketX0 = 150.0f, PocketX1 = 170.0f, PocketY = 132.0f;
        constexpr float Gravity = 0.07f;   // px / tick^2
        constexpr float MaxSpeed = 4.5f;   // px / tick
        constexpr float HitRadius = 3.0f;  // ball + pin
        constexpr int LaunchEvery = 18;    // ticks
        constexpr std::size_t MaxBalls = 16;


        const glm::vec4 ReelColors[10] = {
            { 1.0f, 0.2f, 0.3f, 1.0f }, { 1.0f, 0.8f, 0.1f, 1.0f }, { 0.3f, 0.9f, 1.0f, 1.0f },
            { 0.5f, 1.0f, 0.3f, 1.0f }, { 1.0f, 0.4f, 0.9f, 1.0f }, { 1.0f, 0.6f, 0.2f, 1.0f },
            { 0.6f, 0.5f, 1.0f, 1.0f }, { 1.0f, 0.95f, 0.8f, 1.0f }, { 0.2f, 1.0f, 0.7f, 1.0f },
            { 1.0f, 0.3f, 0.1f, 1.0f },
        };

    }

    PachinkoAttract::PachinkoAttract(std::uint32_t seed)
        : m_state(seed * 2654435761u + 1u)
    {
        // Staggered rows of pins, leaving the reel window and a lane into
        // the start pocket clear.
        int row = 0;
        for (float y = 40.0f; y <= 180.0f; y += 14.0f, ++row)
        {
            for (float x = Left + 10.0f + (row % 2) * 9.0f; x <= Right - 8.0f; x += 18.0f)
            {
                const bool inReel = x > ReelX0 - 6.0f && x < ReelX1 + 6.0f && y > ReelY0 - 6.0f && y < ReelY1 + 6.0f;
                const bool inLane = x > PocketX0 - 2.0f && x < PocketX1 + 2.0f && y > ReelY1 && y < PocketY + 4.0f;
                if (!inReel && !inLane)
                {
                    m_pins.emplace_back(x, y);
                }
            }
        }
    }

    float PachinkoAttract::Random()
    {
        // xorshift32: tiny and the same on every platform.
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return static_cast<float>(m_state & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    void PachinkoAttract::Step()
    {
        ++m_ticks;
        if (m_ticks % LaunchEvery == 0 && m_balls.size() < MaxBalls)
        {
            // Out of the rail's top, spread across the upper field.
            m_balls.push_back({ { Left + 30.0f + Random() * 220.0f, Top + 2.0f },
                                { (Random() - 0.5f) * 1.6f, 0.2f } });
            ++m_launched;
        }

        for (Ball& ball : m_balls)
        {
            ball.velocity.y += Gravity;
            ball.position += ball.velocity;

            for (const glm::vec2& pin : m_pins)
            {
                const glm::vec2 offset = ball.position - pin;
                const float distance = glm::length(offset);
                if (distance < HitRadius && distance > 1e-4f)
                {
                    const glm::vec2 normal = offset / distance;
                    const float into = glm::dot(ball.velocity, normal);
                    if (into < 0.0f)
                    {
                        ball.velocity -= 1.55f * into * normal; // loses some energy
                        ball.velocity.x += (Random() - 0.5f) * 0.4f;
                    }
                    ball.position = pin + normal * HitRadius;
                }
            }
            // The reel window's top deflects balls to either side.
            if (ball.position.x > ReelX0 && ball.position.x < ReelX1
                && ball.position.y > ReelY0 && ball.position.y < ReelY1)
            {
                ball.position.y = ReelY0;
                ball.velocity.y = -std::abs(ball.velocity.y) * 0.3f;
                ball.velocity.x += ball.position.x < (ReelX0 + ReelX1) * 0.5f ? -0.6f : 0.6f;
            }
            if (ball.position.x < Left || ball.position.x > Right)
            {
                ball.position.x = std::clamp(ball.position.x, Left, Right);
                ball.velocity.x = -ball.velocity.x * 0.6f;
            }
            const float speed = glm::length(ball.velocity);
            if (speed > MaxSpeed)
            {
                ball.velocity *= MaxSpeed / speed;
            }
        }

        // Balls leave through the start pocket (a win) or the bottom.
        std::erase_if(m_balls, [&](const Ball& ball) {
            const bool pocket = ball.position.y >= PocketY - 3.0f && ball.position.y <= PocketY + 3.0f
                && ball.position.x >= PocketX0 && ball.position.x <= PocketX1;
            if (pocket)
            {
                m_score += 150;
                m_jackpotTicks = 150;
            }
            const bool gone = pocket || ball.position.y > Bottom;
            m_finished += gone ? 1u : 0u;
            return gone;
        });

        // The score counter rolls up; the reels spin while a win plays.
        if (m_shownScore < m_score)
        {
            m_shownScore = std::min(m_score, m_shownScore + 3);
        }
        if (m_jackpotTicks > 0)
        {
            --m_jackpotTicks;
            for (int i = 0; i < 3; ++i)
            {
                // Each reel stops a little after the one before it.
                if (m_jackpotTicks > 30 * i && m_ticks % (3 + i) == 0)
                {
                    m_reel[i] = (m_reel[i] + 1) % 10;
                }
            }
        }
    }

    bool PachinkoAttract::BallsInBounds() const
    {
        return std::all_of(m_balls.begin(), m_balls.end(), [](const Ball& ball) {
            return ball.position.x >= Left && ball.position.x <= Right
                && ball.position.y >= 0.0f && ball.position.y <= Bottom + MaxSpeed + 1.0f;
        });
    }

    void PachinkoAttract::Draw(Atom::UIRenderer& canvas) const
    {
        // First rectangle covers the whole target: nothing old shows.
        canvas.DrawRect({ 0.0f, 0.0f }, { Width, Height }, { 0.04f, 0.03f, 0.10f, 1.0f });
        canvas.DrawRect({ Left - 4.0f, Top - 4.0f }, { Right - Left + 8.0f, Bottom - Top + 8.0f },
                        { 0.10f, 0.12f, 0.28f, 1.0f });

        // Chasing bulbs around the edge; all flash together on a win.
        const bool flash = m_jackpotTicks > 0 && (m_ticks / 6) % 2 == 0;
        int bulb = 0;
        const auto bulbAt = [&](float x, float y) {
            const bool lit = m_jackpotTicks > 0 ? flash : ((bulb + static_cast<int>(m_ticks / 5)) % 4 == 0);
            canvas.DrawRect({ x, y }, { 5.0f, 5.0f }, lit ? glm::vec4{ 1.0f, 0.9f, 0.4f, 1.0f } : glm::vec4{ 0.45f, 0.2f, 0.1f, 1.0f });
            ++bulb;
        };
        for (float x = 4.0f; x < Width - 8.0f; x += 12.0f) bulbAt(x, 4.0f);
        for (float y = 16.0f; y < Height - 8.0f; y += 12.0f) bulbAt(Width - 10.0f, y);
        for (float x = Width - 16.0f; x > 4.0f; x -= 12.0f) bulbAt(x, Height - 10.0f);
        for (float y = Height - 22.0f; y > 10.0f; y -= 12.0f) bulbAt(4.0f, y);

        for (const glm::vec2& pin : m_pins)
        {
            canvas.DrawRect(pin - glm::vec2{ 1.0f }, { 2.0f, 2.0f }, { 0.85f, 0.85f, 0.75f, 1.0f });
        }

        // The reels, and the start pocket under them.
        canvas.DrawRect({ ReelX0, ReelY0 }, { ReelX1 - ReelX0, ReelY1 - ReelY0 }, { 0.02f, 0.02f, 0.03f, 1.0f });
        for (int i = 0; i < 3; ++i)
        {
            const glm::vec2 at{ ReelX0 + 6.0f + i * 26.0f, ReelY0 + 6.0f };
            canvas.DrawRect(at, { 22.0f, 34.0f }, ReelColors[m_reel[i]] * glm::vec4{ 0.35f, 0.35f, 0.35f, 1.0f });
            DrawDigit(canvas, m_reel[i], at + glm::vec2{ 5.0f, 5.0f }, { 12.0f, 24.0f }, 3.0f, ReelColors[m_reel[i]]);
        }
        canvas.DrawRect({ PocketX0, PocketY - 2.0f }, { PocketX1 - PocketX0, 5.0f },
                        flash ? glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f } : glm::vec4{ 0.9f, 0.3f, 0.2f, 1.0f });

        for (const Ball& ball : m_balls)
        {
            canvas.DrawRect(ball.position - glm::vec2{ 2.0f }, { 4.0f, 4.0f }, { 0.92f, 0.93f, 0.97f, 1.0f });
        }

        // Score: six amber 7-segment digits along the bottom.
        std::uint32_t value = m_shownScore;
        for (int i = 5; i >= 0; --i)
        {
            DrawDigit(canvas, static_cast<int>(value % 10), { 110.0f + i * 17.0f, 207.0f }, { 11.0f, 20.0f }, 2.0f,
                      { 1.0f, 0.65f, 0.15f, 1.0f });
            value /= 10;
        }
    }
}
