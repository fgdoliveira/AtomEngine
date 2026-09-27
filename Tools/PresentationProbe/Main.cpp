#include "Probe.h"

#include <chrono>
#include <iostream>
#include <sstream>

namespace PresentationProbe
{
    Log::Log(const std::string& path)
        : m_file(path, std::ios::app)
    {
    }

    void Log::Write(const std::string& message)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        std::ostringstream entry;
        entry << "[steady_ms=" << milliseconds << "] " << message;
        std::cout << entry.str() << '\n';
        if (m_file)
        {
            m_file << entry.str() << '\n';
            m_file.flush();
        }
    }
}

namespace
{
    void PrintUsage()
    {
        std::cout
            << "Usage: PresentationProbe --api sdl|d3d12 "
               "[--new-window-after-failure] [--prefer-low-power] "
               "[--adapter-index n] [--position x y] "
               "[--frames count] [--log path]\n";
    }
}

int main(int argc, char** argv)
{
    PresentationProbe::Options options;
    std::string api;

    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--api" && index + 1 < argc)
        {
            api = argv[++index];
        }
        else if (argument == "--new-window-after-failure")
        {
            options.tryNewWindowAfterFailure = true;
        }
        else if (argument == "--prefer-low-power")
        {
            options.preferLowPower = true;
        }
        else if (argument == "--adapter-index" && index + 1 < argc)
        {
            try
            {
                options.adapterIndex = static_cast<std::uint32_t>(
                    std::stoul(argv[++index])
                );
            }
            catch (...)
            {
                std::cerr << "Invalid adapter index.\n";
                return 2;
            }
        }
        else if (argument == "--position" && index + 2 < argc)
        {
            try
            {
                options.initialX = std::stoi(argv[++index]);
                options.initialY = std::stoi(argv[++index]);
            }
            catch (...)
            {
                std::cerr << "Invalid window position.\n";
                return 2;
            }
        }
        else if (argument == "--frames" && index + 1 < argc)
        {
            try
            {
                options.maxFrames = std::stoull(argv[++index]);
            }
            catch (...)
            {
                std::cerr << "Invalid frame count.\n";
                return 2;
            }
        }
        else if (argument == "--log" && index + 1 < argc)
        {
            options.logPath = argv[++index];
        }
        else if (argument == "--help" || argument == "-h")
        {
            PrintUsage();
            return 0;
        }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << argument << '\n';
            PrintUsage();
            return 2;
        }
    }

    if (api == "sdl")
    {
        return PresentationProbe::RunSdl(options);
    }
    if (api == "d3d12")
    {
        return PresentationProbe::RunD3D12(options);
    }

    PrintUsage();
    return 2;
}
