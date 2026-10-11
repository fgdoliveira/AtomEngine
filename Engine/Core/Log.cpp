#include "Core/Log.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <mutex>

namespace Atom::Log
{
    std::string_view Name(Level level)
    {
        switch (level)
        {
        case Level::Info: return "INFO ";
        case Level::Warn: return "WARN ";
        case Level::Error: return "ERROR";
        }
        return "?    ";
    }

    std::string Format(Level level, std::string_view subsystem, std::string_view message, std::int64_t millisecondsOfDay)
    {
        while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
        {
            message.remove_suffix(1); // the line adds its own
        }
        const std::int64_t ms = ((millisecondsOfDay % 86'400'000) + 86'400'000) % 86'400'000;
        char time[32];
        std::snprintf(time, sizeof(time), "[%02d:%02d:%02d.%03d]", static_cast<int>(ms / 3'600'000),
                      static_cast<int>(ms / 60'000 % 60), static_cast<int>(ms / 1000 % 60), static_cast<int>(ms % 1000));
        std::string line = time;
        line += " [";
        line += Name(level);
        line += "] [";
        line += subsystem;
        line += "] ";
        line += message;
        return line;
    }

    namespace
    {
        std::int64_t MillisecondsOfDay()
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
            std::tm local{};
#ifdef _WIN32
            localtime_s(&local, &seconds);
#else
            localtime_r(&seconds, &local);
#endif
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
            return ((local.tm_hour * 60 + local.tm_min) * 60 + local.tm_sec) * 1000LL + ms;
        }

        std::mutex& OutputMutex()
        {
            static std::mutex mutex;
            return mutex;
        }
    }

    void Write(Level level, std::string_view subsystem, std::string_view message)
    {
        const std::string line = Format(level, subsystem, message, MillisecondsOfDay());
        const std::lock_guard<std::mutex> lock(OutputMutex());
        std::ostream& out = level == Level::Info ? std::cout : std::cerr;
        out << line << '\n';
        if (level != Level::Info)
        {
            out.flush(); // an error must reach the file even if the next line is a crash
        }
    }

    Line::~Line()
    {
        Write(m_level, m_subsystem, m_text.str());
    }
}
