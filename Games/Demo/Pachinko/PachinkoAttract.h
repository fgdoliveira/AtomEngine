#pragma once

#include <glm/vec2.hpp>

#include <cstdint>
#include <vector>

namespace Atom
{
    class UIRenderer;
}

namespace Demo
{
    // A pachinko machine's attract loop (M27): what the screen shows while
    // nobody plays. Balls launched up the left rail fall through a field of
    // pins; one landing in the centre pocket spins the reels and rolls the
    // score up. Runs at a fixed step on a 320x240 virtual screen and draws
    // with plain rectangles (7-segment digits, square bulbs), so the same
    // code can later draw the playable game (v0.0.5) fullscreen.
    //
    // Deterministic: the same seed and the same number of steps give the
    // same picture, whatever the frame rate.
    class PachinkoAttract
    {
    public:
        static constexpr int Width = 320;
        static constexpr int Height = 240;

        explicit PachinkoAttract(std::uint32_t seed = 1);

        void Step();                          // one 1/60 s tick
        void Draw(Atom::UIRenderer& canvas) const;

        std::uint32_t GetScore() const { return m_score; }
        std::uint32_t GetLaunched() const { return m_launched; }
        std::uint32_t GetFinished() const { return m_finished; }
        std::uint64_t GetTicks() const { return m_ticks; }
        std::size_t GetBallCount() const { return m_balls.size(); }
        bool BallsInBounds() const;

    private:
        struct Ball
        {
            glm::vec2 position;
            glm::vec2 velocity;
        };

        float Random(); // 0..1, from the seeded generator

        std::uint32_t m_state;
        std::vector<glm::vec2> m_pins;
        std::vector<Ball> m_balls;
        std::uint64_t m_ticks = 0;
        std::uint32_t m_launched = 0;
        std::uint32_t m_finished = 0;
        std::uint32_t m_score = 0;
        std::uint32_t m_shownScore = 0; // rolls up toward m_score
        int m_jackpotTicks = 0;         // reels spinning, lights flashing
        int m_reel[3] = { 1, 4, 7 };
    };
}
