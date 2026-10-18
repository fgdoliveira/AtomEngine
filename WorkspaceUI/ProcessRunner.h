#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct SDL_Process;

namespace AtomWorkspace
{
    // One child process at a time (v0.0.19, M125): Validate, Build and Run
    // are separate programs (AtomLevelCheck, cmake, the game), so a crash
    // or a hang in one never takes the workspace with it. Its output (stdout
    // and stderr, merged) is read on a thread of its own and handed over in
    // lines; the UI polls each frame and never waits.
    class ProcessRunner
    {
    public:
        ProcessRunner() = default;
        ~ProcessRunner(); // stops a running process and waits for its reader

        ProcessRunner(const ProcessRunner&) = delete;
        ProcessRunner& operator=(const ProcessRunner&) = delete;

        // False (with Error()) when it couldn't start or one is running.
        bool Start(std::vector<std::string> args, const std::string& workingDirectory = {});
        bool IsRunning() const { return m_process && !m_finished; }
        // Asks it to end, forcefully: what it started itself (MSBuild's
        // workers under cmake) may outlive it.
        void Stop();

        // The complete lines since the last call (a trailing partial line
        // waits for its end, or for the process to finish).
        std::vector<std::string> TakeLines();
        // Once, when it has finished and every line was taken: its exit code.
        std::optional<int> TakeExitCode();

        bool WasStopped() const { return m_stopped; }
        double Seconds() const; // running time so far, or in all once finished
        const std::string& Error() const { return m_error; }

    private:
        void Read();
        void Release();

        SDL_Process* m_process = nullptr;
        std::thread m_reader;
        std::mutex m_mutex;
        std::vector<std::string> m_lines;  // guarded by m_mutex
        std::string m_partial;             // the reader's only
        std::atomic<bool> m_finished{ false };
        std::atomic<int> m_exitCode{ 0 };
        bool m_stopped = false;
        bool m_exitTaken = true;
        std::uint64_t m_startTicks = 0;
        std::uint64_t m_endTicks = 0;
        std::string m_error;
    };
}
