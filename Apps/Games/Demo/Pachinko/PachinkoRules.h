#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace Demo
{
    // A machine's rules as data (M32), from its machine file's "rules".
    struct RulesSettings
    {
        int odds = 99;               // a spin hits 1 time in `odds`
        int maxHeld = 4;             // spins waiting while the reels turn
        float reachChance = 0.12f;   // share of misses that tease a reach
        float spinSeconds = 2.4f;    // until the last reel stops
        float reachSeconds = 2.0f;   // extra suspense on a reach
        float resultSeconds = 1.2f;  // the result stays up
        int feverRounds = 8;         // gate openings in a fever
        int ballsPerRound = 9;       // a round closes after this many...
        float roundSeconds = 25.0f;  // ...or this long
        float intervalSeconds = 1.5f; // closed between rounds
    };

    // The rules of a classic pachinko machine as a state machine (M32):
    //
    //   Idle --start pocket--> Spinning --reels stop--> Result --miss--> Idle
    //                                                       \--hit--> Fever
    //   Fever: Round (gate open until N balls or T seconds) -> Interval ->
    //          Round ... after the last round -> Idle
    //
    // Balls into the start pocket are *held* (up to maxHeld); each held
    // spin plays in turn. Every spin's outcome is drawn from a seeded
    // generator the moment it starts, so a session replays exactly from its
    // seed and inputs; the reels only show what was drawn. Pure: pocket
    // events in, gate state and events out, no drawing or sound.
    class PachinkoRules
    {
    public:
        enum class State { Idle, Spinning, Result, Round, Interval };

        enum class Event
        {
            SpinStart, ReelStop, Reach, Hit, Miss,
            FeverStart, RoundStart, RoundEnd, FeverEnd,
        };

        PachinkoRules(RulesSettings settings, std::uint32_t seed);

        // Inputs from the playfield this tick.
        void OnStartPocket();
        void OnAttacker();

        // Advances the machine by one tick (seconds).
        void Update(float seconds);

        State GetState() const { return m_state; }
        bool IsGateOpen() const { return m_state == State::Round; }
        bool InFever() const { return m_state == State::Round || m_state == State::Interval; }
        int GetHeld() const { return m_held; }
        int GetRound() const { return m_round; }            // 1-based while in a fever
        int GetBallsThisRound() const { return m_ballsThisRound; }
        bool IsReach() const { return m_reach && m_state == State::Spinning; }
        // The three reels: shown digits, and whether each has stopped.
        const std::array<int, 3>& GetReels() const { return m_shown; }
        bool IsReelStopped(int reel) const { return m_stopped[static_cast<std::size_t>(reel)]; }
        const std::vector<Event>& GetEvents() const { return m_events; } // this tick

        std::uint32_t GetSpins() const { return m_spins; }
        std::uint32_t GetHits() const { return m_hits; }
        const RulesSettings& GetSettings() const { return m_settings; }

        // Draws one spin's outcome (exposed for the distribution test).
        bool DrawHit();

    private:
        float Random();
        void StartSpin();

        RulesSettings m_settings;
        std::uint32_t m_state_rng;
        State m_state = State::Idle;
        float m_time = 0.0f;        // in the current state
        int m_held = 0;
        bool m_hit = false;
        bool m_reach = false;
        std::array<int, 3> m_target{};  // what the reels will show
        std::array<int, 3> m_shown{ 7, 7, 7 };
        std::array<bool, 3> m_stopped{ true, true, true };
        int m_round = 0;
        int m_ballsThisRound = 0;
        std::uint32_t m_spins = 0;
        std::uint32_t m_hits = 0;
        std::uint32_t m_ticks = 0;
        std::vector<Event> m_events;
    };
}
