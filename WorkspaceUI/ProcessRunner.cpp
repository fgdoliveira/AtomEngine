#include "ProcessRunner.h"

#include <SDL3/SDL.h>

namespace AtomWorkspace
{
    ProcessRunner::~ProcessRunner()
    {
        Stop();
        Release();
    }

    bool ProcessRunner::Start(std::vector<std::string> args, const std::string& workingDirectory)
    {
        if (IsRunning())
        {
            m_error = "Another operation is running.";
            return false;
        }
        Release();
        if (args.empty())
        {
            m_error = "Nothing to run.";
            return false;
        }
        std::vector<const char*> argv;
        for (const std::string& arg : args)
        {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        const SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
        if (!workingDirectory.empty())
        {
            SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, workingDirectory.c_str());
        }
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
        m_process = SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props);
        if (!m_process)
        {
            m_error = std::string("Couldn't start ") + args.front() + ": " + SDL_GetError();
            return false;
        }
        m_error.clear();
        m_stopped = false;
        m_finished = false;
        m_exitTaken = false;
        m_exitCode = 0;
        m_startTicks = SDL_GetTicks();
        m_endTicks = 0;
        m_reader = std::thread([this] { Read(); });
        return true;
    }

    void ProcessRunner::Read()
    {
        // Blocking reads on this thread, until the pipe closes (the process
        // ended, or was killed); then its exit code.
        SDL_IOStream* output = SDL_GetProcessOutput(m_process);
        char buffer[4096];
        while (output)
        {
            const std::size_t read = SDL_ReadIO(output, buffer, sizeof(buffer));
            if (read == 0)
            {
                if (SDL_GetIOStatus(output) == SDL_IO_STATUS_NOT_READY)
                {
                    SDL_Delay(5);
                    continue;
                }
                break; // end of the output, or an error reading it
            }
            m_partial.append(buffer, read);
            std::vector<std::string> complete;
            std::size_t start = 0;
            for (std::size_t end; (end = m_partial.find('\n', start)) != std::string::npos; start = end + 1)
            {
                std::string line = m_partial.substr(start, end - start);
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }
                complete.push_back(std::move(line));
            }
            m_partial.erase(0, start);
            if (!complete.empty())
            {
                const std::lock_guard lock(m_mutex);
                m_lines.insert(m_lines.end(), std::make_move_iterator(complete.begin()), std::make_move_iterator(complete.end()));
            }
        }
        int exitCode = -1;
        SDL_WaitProcess(m_process, true, &exitCode);
        {
            const std::lock_guard lock(m_mutex);
            if (!m_partial.empty())
            {
                m_lines.push_back(std::move(m_partial));
                m_partial.clear();
            }
        }
        m_exitCode = exitCode;
        m_finished = true;
    }

    void ProcessRunner::Stop()
    {
        if (IsRunning())
        {
            m_stopped = true;
            SDL_KillProcess(m_process, true);
        }
    }

    std::vector<std::string> ProcessRunner::TakeLines()
    {
        const std::lock_guard lock(m_mutex);
        std::vector<std::string> lines;
        lines.swap(m_lines);
        return lines;
    }

    std::optional<int> ProcessRunner::TakeExitCode()
    {
        if (!m_process || !m_finished || m_exitTaken)
        {
            return std::nullopt;
        }
        {
            const std::lock_guard lock(m_mutex);
            if (!m_lines.empty())
            {
                return std::nullopt; // the last lines first
            }
        }
        m_exitTaken = true;
        m_endTicks = SDL_GetTicks();
        const int code = m_exitCode;
        Release();
        return code;
    }

    double ProcessRunner::Seconds() const
    {
        if (m_startTicks == 0)
        {
            return 0.0;
        }
        const std::uint64_t end = m_endTicks ? m_endTicks : SDL_GetTicks();
        return static_cast<double>(end - m_startTicks) / 1000.0;
    }

    void ProcessRunner::Release()
    {
        if (m_reader.joinable())
        {
            m_reader.join();
        }
        if (m_process)
        {
            SDL_DestroyProcess(m_process);
            m_process = nullptr;
        }
    }
}
