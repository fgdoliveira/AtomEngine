#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace Atom::Log
{
    // The engine's log lines (v0.0.15, M100): a time, a level and the part
    // of the engine that speaks, then the message -
    //   [14:03:07.412] [ERROR] [Renderer] Failed to begin the scene pass: ...
    // INFO goes to std::cout, WARN and ERROR to std::cerr; RunLog (the
    // framework) copies both to the per-user log file, so a player's log
    // reads the same as a developer's console. One line at a time, whole,
    // from any thread.
    //
    //   ATOM_LOG_ERROR("Renderer") << "Failed to acquire: " << SDL_GetError();
    //
    // A trailing newline in the message is dropped (the line adds its own).
    enum class Level
    {
        Info,
        Warn,
        Error,
    };

    std::string_view Name(Level level); // "INFO ", "WARN ", "ERROR" (one width)

    // Pure (unit-tested): one line, without its newline. `millisecondsOfDay`
    // is the local time since midnight.
    std::string Format(Level level, std::string_view subsystem, std::string_view message,
                       std::int64_t millisecondsOfDay);

    // Writes one line now, stamped with the local time.
    void Write(Level level, std::string_view subsystem, std::string_view message);

    // A line under construction: written when the statement ends.
    class Line
    {
    public:
        Line(Level level, const char* subsystem) : m_level(level), m_subsystem(subsystem) {}
        ~Line();
        Line(const Line&) = delete;
        Line& operator=(const Line&) = delete;

        template <typename T>
        Line& operator<<(const T& value)
        {
            m_text << value;
            return *this;
        }

    private:
        Level m_level;
        const char* m_subsystem;
        std::ostringstream m_text;
    };
}

#define ATOM_LOG_INFO(subsystem) ::Atom::Log::Line(::Atom::Log::Level::Info, subsystem)
#define ATOM_LOG_WARN(subsystem) ::Atom::Log::Line(::Atom::Log::Level::Warn, subsystem)
#define ATOM_LOG_ERROR(subsystem) ::Atom::Log::Line(::Atom::Log::Level::Error, subsystem)
