#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace Atom::Memory
{
    // How much memory the process uses (v0.0.15, M101), from the operating
    // system: what's in RAM now and at most so far (the working set), and
    // what it has committed for itself (private bytes).
    struct ProcessMemory
    {
        std::uint64_t workingSetBytes = 0;
        std::uint64_t peakWorkingSetBytes = 0;
        std::uint64_t privateBytes = 0;
    };
    std::optional<ProcessMemory> QueryProcess();

    // GPU memory this process uses on the adapter called `adapterName`, and
    // the budget Windows gives it there (DXGI). On an integrated GPU this is
    // shared system memory. Empty if the adapter isn't found by name.
    struct GpuMemory
    {
        std::uint64_t usageBytes = 0;
        std::uint64_t budgetBytes = 0;
        bool shared = false; // from system memory (integrated GPU), not dedicated
    };
    std::optional<GpuMemory> QueryGpu(const std::string& adapterName);

    // "123.4 MB" (pure).
    std::string Megabytes(std::uint64_t bytes);

    // Pure: one line for F1 and the reports, e.g.
    //   "RAM 312.4 MB (peak 340.1 MB)   GPU 210.0 MB of 3800.0 MB (shared)"
    // with "unavailable" for what couldn't be measured.
    std::string Describe(const std::optional<ProcessMemory>& process, const std::optional<GpuMemory>& gpu);
}
