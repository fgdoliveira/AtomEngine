#include "Renderer/GpuResources.h"

#include <doctest/doctest.h>

using namespace Atom;

namespace
{
    // The bookkeeping never touches the device: any distinct address will do.
    SDL_GPUDevice* FakeDevice(int n)
    {
        static char devices[4];
        return reinterpret_cast<SDL_GPUDevice*>(&devices[n]);
    }
}

TEST_CASE("GPU wrappers are counted per device and kind, and named when left alive")
{
    SDL_GPUDevice* device = FakeDevice(0);
    SDL_GPUDevice* other = FakeDevice(1);

    CHECK(GpuResources::Report(device).empty());
    GpuResources::Added(device, GpuResourceKind::Texture);
    GpuResources::Added(device, GpuResourceKind::Texture);
    GpuResources::Added(device, GpuResourceKind::Mesh);
    GpuResources::Added(other, GpuResourceKind::Mesh);

    CHECK(GpuResources::Live(device, GpuResourceKind::Texture) == 2);
    CHECK(GpuResources::Live(device, GpuResourceKind::Mesh) == 1);
    CHECK(GpuResources::Report(device) == "2 textures, 1 mesh");
    CHECK(GpuResources::Report(other) == "1 mesh"); // devices don't mix

    GpuResources::Removed(device, GpuResourceKind::Texture);
    GpuResources::Removed(device, GpuResourceKind::Texture);
    GpuResources::Removed(device, GpuResourceKind::Mesh);
    CHECK(GpuResources::Report(device).empty()); // all gone: a clean shutdown

    // A removal past zero (or for an unknown device) stays at zero.
    GpuResources::Removed(device, GpuResourceKind::Mesh);
    GpuResources::Removed(FakeDevice(2), GpuResourceKind::Texture);
    CHECK(GpuResources::Live(device, GpuResourceKind::Mesh) == 0);

    // A null device (a wrapper that failed before it had one) isn't counted.
    GpuResources::Added(nullptr, GpuResourceKind::Texture);
    CHECK(GpuResources::Live(nullptr, GpuResourceKind::Texture) == 0);

    GpuResources::Forget(other);
    CHECK(GpuResources::Report(other).empty());
    GpuResources::Forget(device);
}
