#pragma once

#include "Core/FrameStatsWindow.h"
#include "Testing/TestScript.h"

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace AtomGame
{
    // What measures and drives the game rather than plays it (M57, audit
    // ARCH-002): the frame-time log (M46), the scripted test (M13) and the
    // fixed step for captures. Owned by DemoApp, which still decides the
    // frame's order; DemoApp no longer holds this state itself.
    class GameDiagnostics
    {
    public:
        GameDiagnostics();
        ~GameDiagnostics(); // where std::ofstream is complete (the CSV file)

        // The frame-time log: after a warm-up (pipelines, caches,
        // allocations - not heat), one line per block of frames.
        struct PerfSettings
        {
            bool enabled = false;
            std::size_t block = 240;
            int warmup = 300;
        };

        // From the environment: ATOM_PERF_LOG, ATOM_PERF_BLOCK,
        // ATOM_PERF_CSV and ATOM_TEST_SCRIPT. Returns the exit code to stop
        // with when the run can't go on (a script unreadable or invalid: 2).
        std::optional<int> InitializeFromEnvironment();

        // Explicit set-up (what the environment does; tests use it).
        void ConfigurePerf(const PerfSettings& settings, std::ostream& out, std::ostream* csv = nullptr);
        void SetTestScript(std::vector<TestCommand> commands);

        // A real frame time, before any fixed step; `label` names what is
        // measured (the level).
        void RecordFrameTime(float realSeconds, const std::string& label);
        double RealFrameMs() const { return m_lastRealFrameMs; }

        // The step the game advances by: the fixed one when set (captures,
        // reproducible scenarios), else the real one.
        float Step(float realSeconds) const { return m_fixedStep > 0.0f ? m_fixedStep : realSeconds; }
        void SetFixedStep(float seconds) { m_fixedStep = seconds; }
        float GetFixedStep() const { return m_fixedStep; }

        bool HasTestScript() const { return m_testRunner != nullptr; }
        // One frame of the script; once it's finished, the process exit
        // code (0 passed, 1 failed) - reported a single time.
        std::optional<int> UpdateTestScript(float deltaSeconds, TestHooks& game);

        // "PERF block 12 samples 240 median 6.310 p95 7.040 mean 6.390 label night_street"
        static std::string FormatPerfLine(int block, const Atom::FrameStatsWindow& window, const std::string& label);

    private:
        PerfSettings m_perf;
        int m_warmupLeft = 0;
        int m_blockIndex = 0;
        Atom::FrameStatsWindow m_window;
        std::ostream* m_out = nullptr;
        std::ostream* m_csv = nullptr;
        std::unique_ptr<std::ofstream> m_csvFile; // ATOM_PERF_CSV
        double m_lastRealFrameMs = 0.0;
        float m_fixedStep = 0.0f; // 0 = real time
        std::unique_ptr<TestRunner> m_testRunner;
        bool m_reported = false;
    };
}
