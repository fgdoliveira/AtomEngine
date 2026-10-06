#pragma once

#include "Flight.h"
#include "World.h"

#include "Assets/Model.h"
#include "Core/Application.h"
#include "UI/Font.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Drift
{
    // DRIFT on AtomEngine (M77-): a second game, built from the engine and
    // the framework, sharing no code with the demo.
    class DriftApp : public Atom::Application
    {
    public:
        explicit DriftApp(std::vector<std::string> arguments);

    protected:
        bool OnInitialize() override;
        void OnUpdate(float deltaSeconds) override;
        void OnShutdown() override;

    private:
        ShipInput ReadInput() const;
        void ApplyAtmosphere(float flow);
        void SubmitWorld();
        void DrawHud(float dt);

        std::vector<std::string> m_arguments;
        std::string m_assetRoot;
        std::unique_ptr<Atom::Model> m_ship;
        std::unique_ptr<Atom::Model> m_ring;
        std::unique_ptr<Atom::Model> m_orb;
        std::unique_ptr<Atom::Model> m_rock;
        std::unique_ptr<Atom::Font> m_titleFont; // 88 px: DRIFT
        std::unique_ptr<Atom::Font> m_comboFont; // 42 px: the chain
        std::unique_ptr<Atom::Font> m_smallFont; // 12 px: labels
        Ship m_flight;
        std::unique_ptr<World> m_world;
        Flow m_flow;
        float m_time = 0.0f;

        // The title screen ("CLICK TO LAUNCH") over an idle glide; a click
        // starts the run, and the title fades out over 0.6 s.
        bool m_running = false;
        float m_titleFade = 1.0f;
        float m_chainPop = 0.0f; // the chain counter's pop, 0.25 s

        // Automated runs (ATOM_DRIFT_SECONDS=N): fly the path by a simple
        // pilot, quit after N seconds with a summary; ATOM_DRIFT_SEED fixes
        // the course; ATOM_DRIFT_CAPTURE=<png> screenshots the end.
        std::optional<float> m_autopilotSeconds;
        std::string m_capturePath;
        bool m_captured = false;
    };
}
