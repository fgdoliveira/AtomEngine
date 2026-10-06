#pragma once

#include "Flight.h"

#include "Assets/Model.h"
#include "Core/Application.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Drift
{
    // DRIFT on AtomEngine (M77): a second game, built from the engine and
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

        std::vector<std::string> m_arguments;
        std::string m_assetRoot;
        std::unique_ptr<Atom::Model> m_ship;
        Ship m_flight;
        float m_flow = 0.0f; // M78 feeds it; 0 for now
        float m_time = 0.0f;

        // Automated runs (ATOM_DRIFT_SECONDS=N): steer by a fixed pattern,
        // quit after N seconds with a summary; ATOM_DRIFT_CAPTURE=<png>
        // screenshots the last moment.
        std::optional<float> m_autopilotSeconds;
        std::string m_capturePath;
        bool m_captured = false;
    };
}
