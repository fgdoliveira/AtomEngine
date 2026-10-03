#include "Renderer/GpuResources.h"

#include <array>
#include <unordered_map>

namespace Atom
{
    namespace
    {
        using Counts = std::array<std::size_t, static_cast<std::size_t>(GpuResourceKind::Count)>;

        // Keyed by device: the tests use fake devices, and a device's counts
        // are forgotten when it is destroyed.
        std::unordered_map<SDL_GPUDevice*, Counts>& Table()
        {
            static std::unordered_map<SDL_GPUDevice*, Counts> table;
            return table;
        }

        std::size_t Index(GpuResourceKind kind)
        {
            return static_cast<std::size_t>(kind);
        }
    }

    namespace GpuResources
    {
        void Added(SDL_GPUDevice* device, GpuResourceKind kind)
        {
            if (device)
            {
                ++Table()[device][Index(kind)];
            }
        }

        void Removed(SDL_GPUDevice* device, GpuResourceKind kind)
        {
            const auto found = Table().find(device);
            if (found != Table().end() && found->second[Index(kind)] > 0)
            {
                --found->second[Index(kind)];
            }
        }

        std::size_t Live(SDL_GPUDevice* device, GpuResourceKind kind)
        {
            const auto found = Table().find(device);
            return found != Table().end() ? found->second[Index(kind)] : 0;
        }

        std::string Report(SDL_GPUDevice* device)
        {
            std::string report;
            const auto add = [&](std::size_t count, const char* one, const char* many) {
                if (count > 0)
                {
                    report += (report.empty() ? "" : ", ") + std::to_string(count) + ' ' + (count == 1 ? one : many);
                }
            };
            add(Live(device, GpuResourceKind::Texture), "texture", "textures");
            add(Live(device, GpuResourceKind::Mesh), "mesh", "meshes");
            return report;
        }

        void Forget(SDL_GPUDevice* device)
        {
            Table().erase(device);
        }
    }
}
