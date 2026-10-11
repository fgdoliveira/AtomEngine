#include "Platform/Memory.h"

#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <dxgi1_4.h>
#endif

namespace Atom::Memory
{
    std::string Megabytes(std::uint64_t bytes)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        return text;
    }

    std::string Describe(const std::optional<ProcessMemory>& process, const std::optional<GpuMemory>& gpu)
    {
        std::string line = "RAM ";
        line += process ? Megabytes(process->workingSetBytes) + " (peak " + Megabytes(process->peakWorkingSetBytes) + ")"
                        : "unavailable";
        line += "   GPU ";
        line += gpu ? Megabytes(gpu->usageBytes) + " of " + Megabytes(gpu->budgetBytes) + (gpu->shared ? " (shared)" : "")
                    : "unavailable";
        return line;
    }

#ifdef _WIN32
    std::optional<ProcessMemory> QueryProcess()
    {
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        if (!K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                                     sizeof(counters)))
        {
            return std::nullopt;
        }
        return ProcessMemory{ counters.WorkingSetSize, counters.PeakWorkingSetSize, counters.PrivateUsage };
    }

    std::optional<GpuMemory> QueryGpu(const std::string& adapterName)
    {
        // SDL_GPU doesn't expose its adapter, so it's found by name among
        // DXGI's (the name SDL reports is DXGI's description).
        IDXGIFactory1* factory = nullptr;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))))
        {
            return std::nullopt;
        }
        std::optional<GpuMemory> result;
        IDXGIAdapter1* adapter = nullptr;
        for (UINT i = 0; !result && factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            char name[256]{};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name) - 1, nullptr, nullptr);
            IDXGIAdapter3* adapter3 = nullptr;
            if (adapterName == name
                && SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&adapter3))))
            {
                // An integrated GPU has (almost) no dedicated memory: its
                // textures and buffers live in the shared, "non-local" segment.
                const bool shared = desc.DedicatedVideoMemory < 512ull * 1024 * 1024;
                DXGI_QUERY_VIDEO_MEMORY_INFO info{};
                if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(
                        0, shared ? DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL : DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
                {
                    result = GpuMemory{ info.CurrentUsage, info.Budget, shared };
                }
                adapter3->Release();
            }
            adapter->Release();
        }
        factory->Release();
        return result;
    }
#else
    std::optional<ProcessMemory> QueryProcess() { return std::nullopt; }
    std::optional<GpuMemory> QueryGpu(const std::string&) { return std::nullopt; }
#endif
}
