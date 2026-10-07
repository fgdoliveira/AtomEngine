#pragma once

#include <fstream>
#include <memory>
#include <streambuf>
#include <string>

namespace AtomFramework
{
    // Writes everything to two buffers - the console's and the log file's -
    // and remembers the last complete line (the reason a start failed).
    class TeeBuffer : public std::streambuf
    {
    public:
        TeeBuffer(std::streambuf* first, std::streambuf* second) : m_first(first), m_second(second) {}
        const std::string& LastLine() const { return m_lastLine; }

    protected:
        int_type overflow(int_type character) override;
        std::streamsize xsputn(const char* text, std::streamsize count) override;
        int sync() override;

    private:
        void Track(const char* text, std::streamsize count);

        std::streambuf* m_first;  // may be null (no console)
        std::streambuf* m_second; // may be null (no log file)
        std::string m_line;
        std::string m_lastLine;
    };

    // The run's output for a distributed, windowed game (M67). A
    // Windows-subsystem program has no console: started from a terminal it
    // re-attaches to it; with redirected output (ctest, ab.ps1) it keeps the
    // inherited handles. Either way std::cout and std::cerr are copied to
    // <prefpath>/logs/<app>.log (the previous run kept as
    // <app>.previous.log), the pref path being SDL's for ("AtomEngine",
    // app) - except in scripted runs, which must not overwrite the player's
    // log. M76: shared by every game; `app` is the game's name.
    class RunLog
    {
    public:
        explicit RunLog(const std::string& app);
        ~RunLog();

        RunLog(const RunLog&) = delete;
        RunLog& operator=(const RunLog&) = delete;

        // Double-clicked: no console and no redirected output, so a failure
        // can only be shown in a message box.
        bool IsWithoutConsole() const { return m_withoutConsole; }
        const std::string& GetPath() const { return m_path; } // empty: no file
        // The last line written to std::cerr, for a start-failure message.
        std::string GetLastError() const;

    private:
        bool m_withoutConsole = false;
        std::string m_path;
        std::ofstream m_file;
        std::unique_ptr<TeeBuffer> m_out;
        std::unique_ptr<TeeBuffer> m_error;
        std::streambuf* m_originalOut = nullptr;
        std::streambuf* m_originalError = nullptr;
    };
}
