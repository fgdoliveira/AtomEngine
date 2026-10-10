#include "UI/SettingsScreen.h"

#include "UI/UiKit.h"

#include "Audio/AudioSystem.h"
#include "Audio/SynthStream.h"
#include "Platform/Input.h"
#include "UI/Font.h"
#include "UI/UIRenderer.h"

#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cmath>

namespace AtomFramework
{
    SettingsScreen::SettingsScreen(std::vector<SettingRow> rows) : m_rows(std::move(rows))
    {
    }

    std::string SettingsScreen::Label(SettingRow row)
    {
        switch (row)
        {
        case SettingRow::Display: return "Display";
        case SettingRow::Quality: return "Quality";
        case SettingRow::Volume: return "Volume";
        case SettingRow::Gpu: return "Graphics card";
        case SettingRow::Back: return "Back";
        }
        return {};
    }

    std::string SettingsScreen::Value(SettingRow row, const GameSettings& s)
    {
        switch (row)
        {
        case SettingRow::Display: return s.fullscreen ? "fullscreen" : "window";
        case SettingRow::Quality: return std::string(ToString(s.quality));
        case SettingRow::Volume: return std::to_string(static_cast<int>(std::lround(s.volume * 100.0f))) + "%";
        case SettingRow::Gpu: return std::string(ToString(s.gpu));
        case SettingRow::Back: return {};
        }
        return {};
    }

    SettingsScreen::Result SettingsScreen::Navigate(int move, int step, bool choose, bool cancel, GameSettings& s)
    {
        Result result;
        if (m_rows.empty() || cancel)
        {
            result.back = true;
            return result;
        }
        const int count = static_cast<int>(m_rows.size());
        m_selected = ((m_selected + move) % count + count) % count;
        const SettingRow row = m_rows[m_selected];
        if (choose && step == 0)
        {
            if (row == SettingRow::Back)
            {
                result.back = true;
                return result;
            }
            step = 1; // Enter: the next value
        }
        if (step == 0)
        {
            return result;
        }
        switch (row)
        {
        case SettingRow::Display:
            s.fullscreen = !s.fullscreen;
            break;
        case SettingRow::Quality:
        {
            constexpr QualityMode modes[] = { QualityMode::Auto, QualityMode::Low, QualityMode::Balanced, QualityMode::High };
            const int index = static_cast<int>(std::find(std::begin(modes), std::end(modes), s.quality) - std::begin(modes));
            s.quality = modes[((index + step) % 4 + 4) % 4];
            break;
        }
        case SettingRow::Volume:
        {
            const float before = s.volume;
            s.volume = std::clamp(std::round((s.volume + 0.1f * static_cast<float>(step)) * 10.0f) / 10.0f, 0.0f, 1.0f);
            if (s.volume == before)
            {
                return result; // at an end: nothing changed
            }
            break;
        }
        case SettingRow::Gpu:
            s.gpu = s.gpu == GpuPreference::LowPower ? GpuPreference::HighPerformance : GpuPreference::LowPower;
            s.pendingFallback.reset(); // an explicit choice clears a fallback
            break;
        case SettingRow::Back:
            return result;
        }
        result.changed = row;
        return result;
    }

    SettingsScreen::Result SettingsScreen::Update(const Atom::Input& input, GameSettings& settings)
    {
        const auto pressed = [&](SDL_Scancode a, SDL_Scancode b) { return input.WasKeyPressed(a) || input.WasKeyPressed(b); };
        const int move = (pressed(SDL_SCANCODE_DOWN, SDL_SCANCODE_S) ? 1 : 0) - (pressed(SDL_SCANCODE_UP, SDL_SCANCODE_W) ? 1 : 0);
        const int step = (pressed(SDL_SCANCODE_RIGHT, SDL_SCANCODE_D) ? 1 : 0) - (pressed(SDL_SCANCODE_LEFT, SDL_SCANCODE_A) ? 1 : 0);
        const bool choose = pressed(SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE) || input.WasKeyPressed(SDL_SCANCODE_KP_ENTER);
        return Navigate(move, step, choose, input.WasKeyPressed(SDL_SCANCODE_ESCAPE), settings);
    }

    void SettingsScreen::Draw(UiKit& kit, const GameSettings& settings, GpuPreference running) const
    {
        const glm::vec2 screen = kit.Screen();
        const UiTheme& theme = kit.Theme();
        kit.Renderer().DrawRect({ 0.0f, 0.0f }, screen, { 0.0f, 0.0f, 0.0f, 0.6f });
        const glm::vec2 at{ screen.x * 0.12f, screen.y * 0.22f };
        kit.TitleText("Settings", at, theme.ink);
        const float lineHeight = kit.TitleFont().GetLineHeight();

        // Two columns: the setting, and its value between arrows.
        float labelWidth = 0.0f;
        for (const SettingRow row : m_rows)
        {
            labelWidth = std::max(labelWidth, kit.Renderer().MeasureText(kit.TitleFont(), Label(row)).x);
        }
        const float valueX = labelWidth + 4.0f * kit.Pad();
        const glm::vec2 origin = at + glm::vec2{ 0.0f, lineHeight * 1.6f };
        const float width = valueX + 260.0f * kit.Scale();
        // The menu's band down the left (MenuScreen), the rows on it.
        kit.Renderer().DrawRect({ 0.0f, 0.0f }, { at.x + width + 2.0f * kit.Margin(), screen.y }, theme.panel);
        kit.Panel(origin, { width, lineHeight * static_cast<float>(m_rows.size()) + 2.0f * kit.Pad() });
        for (std::size_t i = 0; i < m_rows.size(); ++i)
        {
            const SettingRow row = m_rows[i];
            const bool chosen = static_cast<int>(i) == m_selected;
            const glm::vec2 line = origin + glm::vec2{ 2.0f * kit.Pad(), kit.Pad() + lineHeight * static_cast<float>(i) };
            if (chosen)
            {
                kit.Renderer().DrawRect(line - glm::vec2{ kit.Pad(), 0.0f }, { 3.0f * kit.Scale(), lineHeight * 0.8f },
                                        theme.accent);
            }
            kit.TitleText(Label(row), line, chosen ? theme.accent : theme.dim);
            if (row != SettingRow::Back)
            {
                std::string value = Value(row, settings);
                value = chosen ? "<  " + value + "  >" : value;
                kit.TitleText(value, line + glm::vec2{ valueX, 0.0f }, chosen ? theme.ink : theme.dim);
            }
        }
        if (std::find(m_rows.begin(), m_rows.end(), SettingRow::Gpu) != m_rows.end() && settings.gpu != running)
        {
            kit.Text("The graphics card changes on the next launch.",
                     origin + glm::vec2{ 0.0f, lineHeight * static_cast<float>(m_rows.size()) + 3.0f * kit.Pad() },
                     theme.accent);
        }
        kit.HintBar("Up / Down choose    Left / Right change    Enter next value    Esc back");
    }

    void ApplyDisplayAndVolume(SDL_Window* window, Atom::AudioSystem* audio, Atom::SynthStream* synth,
                               const GameSettings& settings)
    {
        if (window)
        {
            const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
            if (fullscreen != settings.fullscreen)
            {
                SDL_SetWindowFullscreen(window, settings.fullscreen); // borderless, the desktop's mode
            }
        }
        if (audio)
        {
            audio->SetMasterGain(settings.volume);
        }
        if (synth)
        {
            // Relative to the default 80%, so a synth sounds as it always did
            // until the player turns it up or down.
            synth->SetGain(settings.volume / 0.8f);
        }
    }
}
