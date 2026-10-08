#include "Testing/GameDiagnostics.h"
#include "Core/DevSwitch.h" // M82: ATOM_* switches, compiled out of packages

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string_view>

namespace Demo
{
    GameDiagnostics::GameDiagnostics() = default;
    GameDiagnostics::~GameDiagnostics() = default;

    std::optional<int> GameDiagnostics::InitializeFromEnvironment()
    {
        PerfSettings perf;
        const char* enabled = Atom::DevSwitch("ATOM_PERF_LOG");
        perf.enabled = enabled && *enabled && std::string_view(enabled) != "0";
        if (const char* block = Atom::DevSwitch("ATOM_PERF_BLOCK"))
        {
            perf.block = static_cast<std::size_t>(std::max(30, std::atoi(block)));
        }
        if (const char* path = Atom::DevSwitch("ATOM_PERF_CSV"); perf.enabled && path && *path)
        {
            m_csvFile = std::make_unique<std::ofstream>(path);
        }
        ConfigurePerf(perf, std::cout, m_csvFile.get());

        const char* path = Atom::DevSwitch("ATOM_TEST_SCRIPT");
        if (!path)
        {
            return std::nullopt;
        }
        size_t size = 0;
        void* text = SDL_LoadFile(path, &size);
        if (!text)
        {
            std::cerr << "[test] cannot read script '" << path << "'\n";
            return 2;
        }
        TestScriptParseResult parsed = ParseTestScript(std::string_view(static_cast<const char*>(text), size));
        SDL_free(text);
        if (!parsed.error.empty())
        {
            std::cerr << "[test] invalid script: " << parsed.error << '\n';
            return 2;
        }
        std::cout << "[test] running '" << path << "' (" << parsed.commands.size() << " commands)\n";
        SetTestScript(std::move(parsed.commands));
        return std::nullopt;
    }

    void GameDiagnostics::ConfigurePerf(const PerfSettings& settings, std::ostream& out, std::ostream* csv)
    {
        m_perf = settings;
        m_warmupLeft = settings.warmup;
        m_blockIndex = 0;
        m_window.Clear();
        m_out = &out;
        m_csv = csv;
        if (m_csv)
        {
            *m_csv << "block,samples,median_ms,p95_ms,mean_ms,label\n";
        }
    }

    void GameDiagnostics::SetTestScript(std::vector<TestCommand> commands)
    {
        m_testRunner = std::make_unique<TestRunner>(std::move(commands));
        m_reported = false;
    }

    std::string GameDiagnostics::FormatPerfLine(int block, const Atom::FrameStatsWindow& window, const std::string& label)
    {
        char line[256];
        std::snprintf(line, sizeof(line), "PERF block %d samples %zu median %.3f p95 %.3f mean %.3f label %s",
            block, window.Count(), window.Median(), window.Percentile(0.95), window.Mean(), label.c_str());
        return line;
    }

    void GameDiagnostics::RecordFrameTime(float realSeconds, const std::string& label)
    {
        m_lastRealFrameMs = static_cast<double>(realSeconds) * 1000.0;
        if (!m_perf.enabled || !m_out)
        {
            return;
        }
        if (m_warmupLeft > 0)
        {
            --m_warmupLeft;
            return;
        }
        m_window.AddSample(m_lastRealFrameMs);
        if (m_window.Count() < m_perf.block)
        {
            return;
        }
        if (m_blockIndex == 0 && !m_perfContext.empty())
        {
            *m_out << "PERF context " << m_perfContext << std::endl;
        }
        *m_out << FormatPerfLine(m_blockIndex, m_window, label) << std::endl;
        if (m_csv)
        {
            *m_csv << m_blockIndex << ',' << m_window.Count() << ',' << m_window.Median() << ','
                   << m_window.Percentile(0.95) << ',' << m_window.Mean() << ',' << label << '\n';
        }
        ++m_blockIndex;
        m_window.Clear();
    }

    std::optional<int> GameDiagnostics::UpdateTestScript(float deltaSeconds, TestHooks& game)
    {
        if (!m_testRunner || m_reported)
        {
            return std::nullopt;
        }
        if (!m_testRunner->IsFinished())
        {
            m_testRunner->Update(deltaSeconds, game);
        }
        if (!m_testRunner->IsFinished())
        {
            return std::nullopt;
        }
        m_reported = true;
        if (m_testRunner->Passed())
        {
            std::cout << "[test] PASS\n";
            return 0;
        }
        std::cout << "[test] FAIL " << m_testRunner->GetFailure() << '\n';
        return 1;
    }
}
