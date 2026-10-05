#include "Platform/RunLog.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace AtomGame
{
    TeeBuffer::int_type TeeBuffer::overflow(int_type character)
    {
        if (traits_type::eq_int_type(character, traits_type::eof()))
        {
            return traits_type::not_eof(character);
        }
        const char c = traits_type::to_char_type(character);
        return xsputn(&c, 1) == 1 ? character : traits_type::eof();
    }

    std::streamsize TeeBuffer::xsputn(const char* text, std::streamsize count)
    {
        if (m_first)
        {
            m_first->sputn(text, count);
        }
        if (m_second)
        {
            m_second->sputn(text, count);
        }
        Track(text, count);
        return count;
    }

    int TeeBuffer::sync()
    {
        const int first = m_first ? m_first->pubsync() : 0;
        const int second = m_second ? m_second->pubsync() : 0;
        return first == 0 && second == 0 ? 0 : -1;
    }

    void TeeBuffer::Track(const char* text, std::streamsize count)
    {
        for (std::streamsize i = 0; i < count; ++i)
        {
            if (text[i] == '\n')
            {
                if (!m_line.empty())
                {
                    m_lastLine = m_line;
                }
                m_line.clear();
                // A line is finished: on disk now, so a crash keeps it.
                if (m_second)
                {
                    m_second->pubsync();
                }
            }
            else if (text[i] != '\r')
            {
                m_line += text[i];
            }
        }
    }

    RunLog::RunLog()
    {
#ifdef _WIN32
        // Output already goes somewhere (a pipe, a file): keep it. Else try
        // the console of the terminal that started us.
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        const bool redirected = output != nullptr && output != INVALID_HANDLE_VALUE;
        if (!redirected)
        {
            if (AttachConsole(ATTACH_PARENT_PROCESS))
            {
                FILE* stream = nullptr;
                freopen_s(&stream, "CONOUT$", "w", stdout);
                freopen_s(&stream, "CONOUT$", "w", stderr);
                std::cout.clear();
                std::cerr.clear();
            }
            else
            {
                m_withoutConsole = true;
            }
        }
#endif

        // Scripted runs (tests, benchmarks) keep their output on the console
        // only: the log file belongs to the player's last real session.
        if (!SDL_getenv("ATOM_TEST_SCRIPT"))
        {
            if (char* pref = SDL_GetPrefPath("AtomEngine", "AtomGame"))
            {
                const std::filesystem::path folder = std::filesystem::path(pref) / "logs";
                SDL_free(pref);
                std::error_code error;
                std::filesystem::create_directories(folder, error);
                const std::filesystem::path current = folder / "AtomGame.log";
                std::filesystem::rename(current, folder / "AtomGame.previous.log", error);
                m_file.open(current, std::ios::trunc);
                if (m_file)
                {
                    m_path = current.string();
                }
            }
        }

        std::streambuf* fileBuffer = m_file ? m_file.rdbuf() : nullptr;
        // Without a console the original buffers write nowhere useful.
        m_originalOut = std::cout.rdbuf();
        m_originalError = std::cerr.rdbuf();
        m_out = std::make_unique<TeeBuffer>(m_withoutConsole ? nullptr : m_originalOut, fileBuffer);
        m_error = std::make_unique<TeeBuffer>(m_withoutConsole ? nullptr : m_originalError, fileBuffer);
        std::cout.rdbuf(m_out.get());
        std::cerr.rdbuf(m_error.get());
    }

    RunLog::~RunLog()
    {
        std::cout.flush();
        std::cerr.flush();
        std::cout.rdbuf(m_originalOut);
        std::cerr.rdbuf(m_originalError);
    }

    std::string RunLog::GetLastError() const
    {
        return m_error ? m_error->LastLine() : std::string();
    }
}
