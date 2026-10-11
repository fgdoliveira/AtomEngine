#include "Core/Log.h"

#include <algorithm>
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
        case Level::Game: return "GAME ";
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

    std::string Relative(std::string_view message, const std::vector<std::string>& roots)
    {
        // Windows paths: '\' and '/' alike, letters in either case.
        const auto same = [](char a, char b) {
            const auto fold = [](char c) {
                return c == '\\' ? '/' : (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
            };
            return fold(a) == fold(b);
        };
        std::string out;
        out.reserve(message.size());
        std::size_t i = 0;
        while (i < message.size())
        {
            bool matched = false;
            for (const std::string& root : roots) // longest first (AddRoot keeps them so)
            {
                if (!root.empty() && message.size() - i >= root.size()
                    && std::equal(root.begin(), root.end(), message.begin() + static_cast<std::ptrdiff_t>(i), same))
                {
                    i += root.size();
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                out += message[i++];
            }
        }
        return out;
    }

    namespace
    {
        std::vector<std::string>& Roots()
        {
            static std::vector<std::string> roots;
            return roots;
        }

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

    void AddRoot(std::string root)
    {
        if (root.empty())
        {
            return;
        }
        if (root.back() != '/' && root.back() != '\\')
        {
            root += '/'; // a folder: "C:/game" mustn't match "C:/gameplay"
        }
        const std::lock_guard<std::mutex> lock(OutputMutex());
        std::vector<std::string>& roots = Roots();
        roots.push_back(std::move(root));
        // Longest first, so a root inside another (the build inside the repository) wins.
        std::sort(roots.begin(), roots.end(), [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
    }

    void Write(Level level, std::string_view subsystem, std::string_view message)
    {
        const std::lock_guard<std::mutex> lock(OutputMutex());
        const std::string line = Format(level, subsystem, Relative(message, Roots()), MillisecondsOfDay());
        std::ostream& out = (level == Level::Info || level == Level::Game) ? std::cout : std::cerr;
        out << line << '\n';
        if (level == Level::Warn || level == Level::Error)
        {
            out.flush(); // an error must reach the file even if the next line is a crash
        }
    }

    Line::~Line()
    {
        Write(m_level, m_subsystem, m_text.str());
    }
}
