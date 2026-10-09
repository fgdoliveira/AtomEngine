// The player UI kit (v0.0.14, M89): the layout rules every app shares.
#include "UI/UiKit.h"

#include <doctest/doctest.h>

using namespace AtomFramework;

TEST_CASE("The UI scales with the window's height from 720 lines, within 0.75x-2x")
{
    CHECK(UiScale({ 1280.0f, 720.0f }) == doctest::Approx(1.0f));
    CHECK(UiScale({ 1920.0f, 1080.0f }) == doctest::Approx(1.5f));
    CHECK(UiScale({ 800.0f, 300.0f }) == doctest::Approx(0.75f)); // never smaller
    CHECK(UiScale({ 7680.0f, 4320.0f }) == doctest::Approx(2.0f)); // never larger
}

TEST_CASE("Columns start where the widest cell before them ends, plus the gap")
{
    const std::vector<float> offsets = ColumnOffsets({ 40.0f, 100.0f, 30.0f }, 12.0f);
    REQUIRE(offsets.size() == 3);
    CHECK(offsets[0] == doctest::Approx(0.0f));
    CHECK(offsets[1] == doctest::Approx(52.0f));
    CHECK(offsets[2] == doctest::Approx(164.0f));
    CHECK(ColumnOffsets({}, 12.0f).empty());
}

TEST_CASE("The theme's accent is the one colour for titles and what's on")
{
    const UiTheme& theme = DefaultUiTheme();
    CHECK(theme.accent.a == doctest::Approx(1.0f));
    CHECK(theme.panel.a < 1.0f); // panels let the world show through
    CHECK(theme.ink.r > theme.dim.r);
}
