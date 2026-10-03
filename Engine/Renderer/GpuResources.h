#pragma once

#include <cstddef>
#include <string>

struct SDL_GPUDevice;

namespace Atom
{
    // Live GPU wrappers per device (M53, audit CPP-001). Mesh and Texture
    // free their SDL resources through a raw device pointer, so they must
    // die before the device does - a rule that used to live only in
    // comments. Each wrapper counts itself in and out here, and
    // Renderer::Shutdown checks nothing is left before destroying the
    // device: a wrapper kept too long becomes an immediate, named report
    // instead of a silent use-after-free during teardown.
    //
    // Bookkeeping only, main thread only (like the rest of the renderer);
    // ownership doesn't change.
    enum class GpuResourceKind
    {
        Mesh,
        Texture,
        Count
    };

    namespace GpuResources
    {
        void Added(SDL_GPUDevice* device, GpuResourceKind kind);
        void Removed(SDL_GPUDevice* device, GpuResourceKind kind);
        std::size_t Live(SDL_GPUDevice* device, GpuResourceKind kind);
        // "" when nothing is alive on the device, else e.g. "2 textures, 1 mesh".
        std::string Report(SDL_GPUDevice* device);
        // Forget a device's counts (after its report, as it's destroyed).
        void Forget(SDL_GPUDevice* device);
    }
}
