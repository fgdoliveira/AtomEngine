#pragma once

#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <string>

namespace PresentationProbe
{
    struct Options
    {
        bool tryNewWindowAfterFailure = false;
        bool preferLowPower = false;
        std::uint64_t maxFrames = 0;
        std::uint32_t adapterIndex = std::numeric_limits<std::uint32_t>::max();
        std::optional<int> initialX;
        std::optional<int> initialY;
        std::string logPath = "presentation-probe.log";
    };

    class Log
    {
    public:
        explicit Log(const std::string& path);
        void Write(const std::string& message);

    private:
        std::ofstream m_file;
    };

    int RunSdl(const Options& options);
    int RunD3D12(const Options& options);
}
