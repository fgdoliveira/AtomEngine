#include "Core/FrameStatsWindow.h"

#include <doctest/doctest.h>

using Atom::FrameStatsWindow;

TEST_CASE("Median and percentiles of known frame times")
{
    FrameStatsWindow window;
    for (int ms = 1; ms <= 9; ++ms)
    {
        window.AddSample(static_cast<double>(ms));
    }
    CHECK(window.Count() == 9);
    CHECK(window.Median() == doctest::Approx(5.0));
    CHECK(window.Mean() == doctest::Approx(5.0));
    CHECK(window.Percentile(0.0) == doctest::Approx(1.0));
    CHECK(window.Percentile(1.0) == doctest::Approx(9.0));
    // Between samples it interpolates: 95 % of the way from 1 to 9.
    CHECK(window.Percentile(0.95) == doctest::Approx(8.6));

    // The order samples arrive in doesn't matter.
    FrameStatsWindow shuffled;
    for (const double ms : { 7.0, 2.0, 9.0, 4.0, 1.0, 8.0, 3.0, 6.0, 5.0 })
    {
        shuffled.AddSample(ms);
    }
    CHECK(shuffled.Median() == doctest::Approx(5.0));
}

TEST_CASE("One spike moves the mean, not the median")
{
    FrameStatsWindow window;
    for (int i = 0; i < 99; ++i)
    {
        window.AddSample(5.0);
    }
    window.AddSample(105.0); // a hitch: a shader compiling, a window moved
    CHECK(window.Median() == doctest::Approx(5.0));
    CHECK(window.Mean() == doctest::Approx(6.0));
    CHECK(window.Percentile(0.95) == doctest::Approx(5.0));
}

TEST_CASE("Empty, single and cleared windows")
{
    FrameStatsWindow window;
    CHECK(window.Count() == 0);
    CHECK(window.Median() == 0.0);
    CHECK(window.Mean() == 0.0);

    window.AddSample(4.2);
    CHECK(window.Median() == doctest::Approx(4.2));
    CHECK(window.Percentile(0.95) == doctest::Approx(4.2));

    window.Clear();
    CHECK(window.Count() == 0);
    CHECK(window.Median() == 0.0);
}
