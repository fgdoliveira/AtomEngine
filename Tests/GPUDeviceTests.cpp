#include "Renderer/GPUDevice.h"

#include <doctest/doctest.h>

using namespace Atom;

TEST_CASE("Present mode: vsync whatever the window supports")
{
    const PresentationConfig vsync{ true, true, 3 };
    CHECK(ChoosePresentMode(vsync, true, true) == PresentMode::Vsync);
    CHECK(ChoosePresentMode(vsync, false, false) == PresentMode::Vsync);
}

TEST_CASE("Present mode: uncapped prefers tear-free mailbox, else immediate")
{
    const PresentationConfig uncapped{ false, false, 3 };
    CHECK(ChoosePresentMode(uncapped, true, true) == PresentMode::Mailbox);
    CHECK(ChoosePresentMode(uncapped, false, true) == PresentMode::Immediate);
    CHECK(ChoosePresentMode(uncapped, false, false) == PresentMode::Vsync);
}

TEST_CASE("Present mode: ATOM_PRESENT=immediate puts immediate first (M46)")
{
    const PresentationConfig immediate{ false, true, 3 };
    CHECK(ChoosePresentMode(immediate, true, true) == PresentMode::Immediate);
    CHECK(ChoosePresentMode(immediate, true, false) == PresentMode::Mailbox);
    CHECK(ChoosePresentMode(immediate, false, false) == PresentMode::Vsync);
    CHECK(std::string(GetPresentModeName(PresentMode::Immediate)) == "immediate");
}

TEST_CASE("Fallback: presentation and device failures read differently")
{
    const StartupFallback presentation{ "NVIDIA GeForce RTX 4060 Laptop GPU", StartupFallback::Stage::Presentation,
                                        "Could not create swapchain" };
    CHECK(DescribeFallback(presentation) == "\"NVIDIA GeForce RTX 4060 Laptop GPU\" could not present to this window");

    const StartupFallback device{ "", StartupFallback::Stage::Device, "no adapter" };
    CHECK(DescribeFallback(device) == "the device could not be created");
}

TEST_CASE("Preference: the request, and why it wasn't honoured")
{
    CHECK(DescribePreference(GPUPreference::LowPower, std::nullopt) == "low_power");
    const StartupFallback presentation{ "RTX", StartupFallback::Stage::Presentation, "" };
    CHECK(DescribePreference(GPUPreference::HighPerformance, presentation)
          == "high_performance (fell back: \"RTX\" could not present to this window)");
}
