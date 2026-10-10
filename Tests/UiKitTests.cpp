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

// M90: the title/pause menu and the settings screen, by their pure navigation.
#include "UI/MenuScreen.h"
#include "UI/SettingsScreen.h"

TEST_CASE("A menu moves with wrap-around and returns the item chosen")
{
    MenuScreen menu("Title", "", { "Explore", "Benchmark", "Settings", "Quit" });
    CHECK_FALSE(menu.Navigate(1, false).has_value());
    CHECK(menu.Selected() == 1);
    CHECK(menu.Navigate(-2, false) == std::nullopt);
    CHECK(menu.Selected() == 3); // wrapped from the top to the bottom
    CHECK(menu.Navigate(0, true) == 3);
}

TEST_CASE("The settings screen changes one value per step and says which")
{
    SettingsScreen screen;
    GameSettings settings;
    // Display: Enter toggles it.
    SettingsScreen::Result r = screen.Navigate(0, 0, true, false, settings);
    CHECK(settings.fullscreen);
    CHECK(r.changed == SettingRow::Display);
    // Quality: High, stepped right, wraps to Auto.
    r = screen.Navigate(1, 1, false, false, settings);
    CHECK(settings.quality == QualityMode::Auto);
    CHECK(r.changed == SettingRow::Quality);
    // Volume: tenths, clamped - at 100% another step changes nothing.
    screen.Navigate(1, 0, false, false, settings);
    screen.Navigate(0, 1, false, false, settings);
    CHECK(settings.volume == doctest::Approx(0.9f));
    screen.Navigate(0, 1, false, false, settings);
    r = screen.Navigate(0, 1, false, false, settings);
    CHECK(settings.volume == doctest::Approx(1.0f));
    CHECK_FALSE(r.changed.has_value());
    // GPU: an explicit choice clears a pending fallback.
    settings.pendingFallback = GpuPreference::LowPower;
    r = screen.Navigate(1, -1, false, false, settings);
    CHECK(settings.gpu == GpuPreference::HighPerformance);
    CHECK_FALSE(settings.pendingFallback.has_value());
    // Back, and Esc anywhere, leave.
    CHECK(screen.Navigate(1, 0, true, false, settings).back);
    CHECK(screen.Navigate(0, 0, false, true, settings).back);
}

TEST_CASE("A game's settings screen shows only the rows it has")
{
    SettingsScreen drift({ SettingRow::Display, SettingRow::Volume, SettingRow::Back });
    GameSettings settings;
    CHECK(drift.Rows().size() == 3);
    CHECK(SettingsScreen::Value(SettingRow::Volume, settings) == "80%");
    CHECK(SettingsScreen::Value(SettingRow::Display, settings) == "window");
}
