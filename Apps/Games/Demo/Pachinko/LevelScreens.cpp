#include "Pachinko/LevelScreens.h"

#include "Pachinko/PachinkoAttract.h"
#include "Pachinko/PachinkoGame.h"
#include "Pachinko/Playfield.h"

namespace Demo
{
    using namespace AtomFramework; // v0.0.14: the world layer (levels, world, interaction) lives there

    namespace
    {
        // The attract loop (M27): balls through the pins, reels, a score.
        class AttractScreen final : public ScreenProgram
        {
        public:
            explicit AttractScreen(std::uint32_t seed) : m_attract(seed) {}
            int Width() const override { return PachinkoAttract::Width; }
            int Height() const override { return PachinkoAttract::Height; }
            void Step() override { m_attract.Step(); }
            void Draw(Atom::UIRenderer& canvas) const override { m_attract.Draw(canvas); }

        private:
            PachinkoAttract m_attract;
        };

        // The real game playing itself (M32), on the same virtual screen.
        class SelfPlayingScreen final : public ScreenProgram
        {
        public:
            SelfPlayingScreen(const Playfield& field, std::uint32_t seed) : m_game(field, seed) {}
            int Width() const override { return PachinkoAttract::Width; }
            int Height() const override { return PachinkoAttract::Height; }
            void Step() override
            {
                // A player who never tires: the handle held, the knob in the
                // sweet spot, the tray topped up.
                if (m_game.GetTray() < 20)
                {
                    m_game.AddToTray(200);
                }
                m_game.Step({ true, 0.0f });
            }
            void Draw(Atom::UIRenderer& canvas) const override { m_game.Draw(canvas); }

        private:
            PachinkoGame m_game;
        };
    }

    ScreenFactory MakePachinkoScreens()
    {
        return [](const ScreenData& screen, const AtomFramework::AssetRoots& assets,
                  std::string& error) -> std::unique_ptr<ScreenProgram> {
            if (screen.machine.empty())
            {
                return std::make_unique<AttractScreen>(screen.seed);
            }
            const PlayfieldParseResult field = LoadPlayfieldFile(assets.Resolve(screen.machine));
            if (!field.playfield)
            {
                error = field.error;
                return nullptr;
            }
            return std::make_unique<SelfPlayingScreen>(*field.playfield, screen.seed);
        };
    }
}
