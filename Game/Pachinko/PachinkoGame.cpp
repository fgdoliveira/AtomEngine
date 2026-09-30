#include "Pachinko/PachinkoGame.h"

#include "Pachinko/PixelDraw.h"
#include "UI/UIRenderer.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace AtomGame
{
    namespace
    {
        const glm::vec4 PocketColors[] = {
            { 0.95f, 0.30f, 0.25f, 1.0f }, // start
            { 1.00f, 0.60f, 0.15f, 1.0f }, // attacker
            { 0.95f, 0.85f, 0.30f, 1.0f }, // side
            { 0.05f, 0.05f, 0.08f, 1.0f }, // out
            { 0.20f, 0.20f, 0.25f, 1.0f }, // foul
        };

        void DrawLine(Atom::UIRenderer& canvas, glm::vec2 a, glm::vec2 b, glm::vec4 color)
        {
            const glm::vec2 delta = b - a;
            const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(delta.x), std::abs(delta.y)))));
            for (int i = 0; i <= steps; ++i)
            {
                canvas.DrawRect(glm::floor(a + delta * (static_cast<float>(i) / steps)), { 1.0f, 1.0f }, color);
            }
        }
    }

    PachinkoGame::PachinkoGame(const Playfield& field, std::uint32_t seed)
        : m_field(field)
        , m_rules(field.rules, seed ^ 0x9E3779B9u)
        , m_state(seed * 2654435761u + 7u)
    {
        for (const Segment& wall : m_field.walls)
        {
            m_world.AddSegment(wall);
        }
        for (const Nail& nail : m_field.nails)
        {
            m_world.AddNail(nail);
        }
        if (m_field.gate)
        {
            m_gateSegment = m_world.AddSegment(*m_field.gate); // closed to begin with
        }
    }

    float PachinkoGame::Random()
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return static_cast<float>(m_state & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    void PachinkoGame::SetStrength(float strength)
    {
        m_strength = std::clamp(strength, 0.0f, 1.0f);
    }

    void PachinkoGame::SetGateOpen(bool open)
    {
        m_gateOpen = open;
        if (m_gateSegment)
        {
            m_world.SetSegmentEnabled(*m_gateSegment, !open);
        }
    }

    void PachinkoGame::Step(const PachinkoInput& input)
    {
        ++m_ticks;
        m_events.clear();
        SetStrength(m_strength + input.strengthDelta);

        // The handle: while held, balls leave at a steady rate (the first at
        // once); the knob sets their speed, with a little seeded jitter.
        const LaunchSettings& launch = m_field.launch;
        if (input.launch)
        {
            m_launchTimer -= Tick;
            if (m_launchTimer <= 0.0f && m_tray > 0)
            {
                const float speed = (launch.minSpeed + (launch.maxSpeed - launch.minSpeed) * m_strength)
                    * (1.0f + launch.jitter * (Random() * 2.0f - 1.0f));
                m_world.AddBall(launch.position, launch.direction * speed);
                --m_tray;
                ++m_launched;
                m_launchTimer += 1.0f / launch.perSecond;
            }
        }
        else
        {
            m_launchTimer = 0.0f;
        }

        m_world.Step(Tick);

        // Pockets catch balls; a ball that somehow leaves the board is lost.
        std::vector<std::uint32_t> caught;
        for (const Ball& ball : m_world.GetBalls())
        {
            for (std::size_t i = 0; i < m_field.pockets.size(); ++i)
            {
                const Pocket& pocket = m_field.pockets[i];
                if (!pocket.Contains(ball.position))
                {
                    continue;
                }
                int paid = pocket.payout;
                if (pocket.kind == Pocket::Kind::Foul)
                {
                    paid = 1; // fell back down the lane: the ball comes back
                }
                m_tray += paid;
                ++m_caught[static_cast<std::size_t>(pocket.kind)];
                m_events.push_back({ pocket.kind, i, paid });
                caught.push_back(ball.id);
                break;
            }
            const bool escaped = ball.position.x < m_field.fieldMin.x - 8.0f || ball.position.x > m_field.fieldMax.x + 8.0f
                || ball.position.y > m_field.fieldMax.y + 8.0f;
            if (escaped && (caught.empty() || caught.back() != ball.id))
            {
                ++m_caught[static_cast<std::size_t>(Pocket::Kind::Out)];
                caught.push_back(ball.id);
            }
        }
        for (const std::uint32_t id : caught)
        {
            m_world.RemoveBall(id);
        }

        // The rules read the pockets and decide the gate (M32).
        for (const PocketEvent& event : m_events)
        {
            if (event.kind == Pocket::Kind::Start)
            {
                m_rules.OnStartPocket();
            }
            else if (event.kind == Pocket::Kind::Attacker)
            {
                m_rules.OnAttacker();
            }
        }
        m_rules.Update(Tick);
        if (m_rules.IsGateOpen() != m_gateOpen)
        {
            SetGateOpen(m_rules.IsGateOpen());
        }
    }

    void PachinkoGame::Draw(Atom::UIRenderer& canvas) const
    {
        // Cabinet: dark panels either side of the board.
        canvas.DrawRect({ 0.0f, 0.0f }, { 320.0f, 240.0f }, { 0.06f, 0.04f, 0.07f, 1.0f });
        const glm::vec2 fieldSize = m_field.fieldMax - m_field.fieldMin;
        canvas.DrawRect(m_field.fieldMin, fieldSize, { 0.08f, 0.10f, 0.24f, 1.0f });
        // A painted glow behind the reels, like a machine's centre art.
        const glm::vec2 centre = (m_field.reelsMin + m_field.reelsMax) * 0.5f;
        for (int ring = 5; ring >= 1; --ring)
        {
            const float r = ring * 12.0f;
            canvas.DrawRect(centre - glm::vec2{ r, r * 0.8f }, { 2.0f * r, 1.6f * r },
                            { 0.12f + 0.03f * (5 - ring), 0.10f, 0.28f + 0.02f * (5 - ring), 1.0f });
        }

        for (const Pocket& pocket : m_field.pockets)
        {
            glm::vec4 color = PocketColors[static_cast<std::size_t>(pocket.kind)];
            if (pocket.kind == Pocket::Kind::Attacker && m_gateOpen)
            {
                color = { 1.0f, 0.95f, 0.6f, 1.0f }; // lit when open
            }
            if (pocket.kind != Pocket::Kind::Foul)
            {
                canvas.DrawRect(pocket.min, pocket.max - pocket.min, color);
            }
        }
        for (std::size_t i = 0; i < m_world.GetSegments().size(); ++i)
        {
            if (!m_world.IsSegmentEnabled(i))
            {
                continue;
            }
            const Segment& wall = m_world.GetSegments()[i];
            const bool gate = m_gateSegment && *m_gateSegment == i;
            DrawLine(canvas, wall.a, wall.b, gate ? glm::vec4{ 1.0f, 0.6f, 0.2f, 1.0f } : glm::vec4{ 0.80f, 0.66f, 0.30f, 1.0f });
        }
        for (const Nail& nail : m_field.nails)
        {
            canvas.DrawRect(glm::floor(nail.position) - glm::vec2{ 1.0f, 1.0f }, { 2.0f, 2.0f }, { 0.82f, 0.82f, 0.78f, 1.0f });
        }
        DrawReels(canvas);

        const float r = m_world.GetSettings().ballRadius;
        for (const Ball& ball : m_world.GetBalls())
        {
            const glm::vec2 corner = glm::floor(ball.position - glm::vec2{ r });
            canvas.DrawRect(corner, { 2.0f * r, 2.0f * r }, { 0.70f, 0.72f, 0.78f, 1.0f });
            canvas.DrawRect(corner + glm::vec2{ 1.0f, 1.0f }, { 2.0f, 2.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
        }

        // Left panel: balls in the tray and the launch knob.
        const glm::vec4 amber{ 1.0f, 0.65f, 0.15f, 1.0f };
        canvas.DrawRect({ 4.0f, 8.0f }, { 42.0f, 3.0f }, amber * glm::vec4{ 0.5f, 0.5f, 0.5f, 1.0f });
        DrawNumber(canvas, static_cast<std::uint32_t>(std::max(m_tray, 0)), 4, { 6.0f, 16.0f }, { 7.0f, 14.0f }, 2.0f, 3.0f, amber);
        const glm::vec2 knob{ 25.0f, 190.0f };
        canvas.DrawRect(knob - glm::vec2{ 16.0f }, { 32.0f, 32.0f }, { 0.18f, 0.16f, 0.2f, 1.0f });
        // The pointer turns from 7 o'clock (weak) to 5 o'clock (strong).
        const float angle = glm::radians(225.0f - 270.0f * m_strength);
        for (int i = 0; i < 12; ++i)
        {
            const glm::vec2 p = knob + glm::vec2{ std::cos(angle), -std::sin(angle) } * static_cast<float>(i);
            canvas.DrawRect(glm::floor(p), { 2.0f, 2.0f }, { 0.95f, 0.9f, 0.8f, 1.0f });
        }
        canvas.DrawRect({ 6.0f, 212.0f }, { 38.0f * m_strength, 3.0f }, amber);

        // Right panel: held spins as four lamps, the fever round.
        for (int i = 0; i < m_field.rules.maxHeld; ++i)
        {
            const bool lit = i < m_rules.GetHeld();
            canvas.DrawRect({ 280.0f + (i % 2) * 14.0f, 16.0f + (i / 2) * 14.0f }, { 10.0f, 10.0f },
                            lit ? glm::vec4{ 1.0f, 0.3f, 0.3f, 1.0f } : glm::vec4{ 0.25f, 0.08f, 0.08f, 1.0f });
        }
        if (m_rules.InFever())
        {
            const bool blink = (m_ticks / 15) % 2 == 0;
            canvas.DrawRect({ 274.0f, 60.0f }, { 42.0f, 3.0f }, blink ? amber : glm::vec4{ 1.0f, 0.2f, 0.5f, 1.0f });
            DrawNumber(canvas, static_cast<std::uint32_t>(m_rules.GetRound()), 2, { 278.0f, 70.0f }, { 7.0f, 14.0f }, 2.0f, 3.0f,
                       { 1.0f, 0.3f, 0.6f, 1.0f });
            DrawNumber(canvas, static_cast<std::uint32_t>(m_field.rules.feverRounds), 2, { 278.0f, 92.0f }, { 7.0f, 14.0f }, 2.0f, 3.0f,
                       { 0.6f, 0.2f, 0.4f, 1.0f });
            // Balls into the attacker this round, as a bar.
            const float share = static_cast<float>(m_rules.GetBallsThisRound()) / static_cast<float>(m_field.rules.ballsPerRound);
            canvas.DrawRect({ 278.0f, 114.0f }, { 34.0f * std::min(share, 1.0f), 4.0f }, amber);
        }
    }

    void PachinkoGame::DrawReels(Atom::UIRenderer& canvas) const
    {
        const glm::vec2 min = m_field.reelsMin, max = m_field.reelsMax;
        const bool fever = m_rules.InFever();
        const bool flash = (m_ticks / 6) % 2 == 0;
        glm::vec4 back{ 0.02f, 0.02f, 0.03f, 1.0f };
        if (m_rules.IsReach() && flash)
        {
            back = { 0.25f, 0.02f, 0.06f, 1.0f }; // the reach: the window pulses red
        }
        if (fever)
        {
            back = flash ? glm::vec4{ 0.35f, 0.18f, 0.02f, 1.0f } : glm::vec4{ 0.2f, 0.02f, 0.2f, 1.0f };
        }
        canvas.DrawRect(min, max - min, back);
        const float width = (max.x - min.x - 8.0f) / 3.0f;
        const glm::vec4 colors[3] = { { 1.0f, 0.8f, 0.2f, 1.0f }, { 1.0f, 0.35f, 0.7f, 1.0f }, { 0.4f, 0.85f, 1.0f, 1.0f } };
        for (int i = 0; i < 3; ++i)
        {
            const glm::vec2 at{ min.x + 2.0f + i * (width + 2.0f), min.y + 2.0f };
            const glm::vec2 size{ width, max.y - min.y - 4.0f };
            canvas.DrawRect(at, size, { 0.08f, 0.07f, 0.1f, 1.0f });
            const glm::vec4 color = m_rules.IsReelStopped(i) ? colors[i] : colors[i] * glm::vec4{ 0.6f, 0.6f, 0.6f, 1.0f };
            DrawDigit(canvas, m_rules.GetReels()[static_cast<std::size_t>(i)], at + glm::vec2{ 4.0f, 3.0f },
                      size - glm::vec2{ 8.0f, 6.0f }, 3.0f, color);
        }
    }
}
