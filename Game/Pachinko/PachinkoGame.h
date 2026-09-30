#pragma once

#include "Pachinko/Physics2D.h"
#include "Pachinko/Playfield.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Atom
{
    class UIRenderer;
}

namespace AtomGame
{
    // What the player does this tick: hold the handle, turn the knob.
    struct PachinkoInput
    {
        bool launch = false;
        float strengthDelta = 0.0f; // added to the knob, 0..1 range
    };

    // A ball caught by a pocket this tick.
    struct PocketEvent
    {
        Pocket::Kind kind = Pocket::Kind::Out;
        std::size_t pocket = 0; // index into the playfield's pockets
        int paid = 0;           // balls paid into the tray
    };

    // The playable pachinko machine (M31): a playfield in a physics world,
    // the launcher and the tray of balls. Rules (lottery, fever) sit on top
    // in M32 and read the pocket events. Runs at a fixed 1/60 s tick and is
    // deterministic for a seed and an input sequence.
    class PachinkoGame
    {
    public:
        static constexpr float Tick = 1.0f / 60.0f;

        PachinkoGame(const Playfield& field, std::uint32_t seed);

        void Step(const PachinkoInput& input);
        void Draw(Atom::UIRenderer& canvas) const;

        int GetTray() const { return m_tray; }
        void AddToTray(int balls) { m_tray += balls; }
        float GetStrength() const { return m_strength; }
        void SetStrength(float strength);
        void SetGateOpen(bool open);
        bool IsGateOpen() const { return m_gateOpen; }

        const std::vector<PocketEvent>& GetEvents() const { return m_events; } // this tick
        const std::vector<Impact>& GetImpacts() const { return m_world.GetImpacts(); }
        std::size_t GetBallsInPlay() const { return m_world.GetBalls().size(); }
        std::uint32_t GetLaunched() const { return m_launched; }
        std::uint32_t GetCaught(Pocket::Kind kind) const { return m_caught[static_cast<std::size_t>(kind)]; }
        const Playfield& GetPlayfield() const { return m_field; }
        const World2D& GetWorld() const { return m_world; }
        std::uint64_t GetTicks() const { return m_ticks; }

    private:
        float Random(); // 0..1, seeded xorshift

        Playfield m_field;
        World2D m_world;
        std::optional<std::size_t> m_gateSegment;
        bool m_gateOpen = false;
        std::uint32_t m_state;
        float m_strength = 0.6f;
        float m_launchTimer = 0.0f;
        int m_tray = 0;
        std::uint32_t m_launched = 0;
        std::uint32_t m_caught[5]{};
        std::uint64_t m_ticks = 0;
        std::vector<PocketEvent> m_events;
    };
}
