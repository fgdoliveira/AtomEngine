#pragma once

#include "Settings/GameSettings.h"

#include <optional>
#include <string>
#include <vector>

struct SDL_Window;

namespace Atom
{
    class AudioSystem;
    class Input;
    class SynthStream;
}

namespace AtomFramework
{
    class UiKit;

    // The player's settings (v0.0.14, M90): the same screen in every app,
    // with the rows that app has. Until M82 these lived in the demo's F10
    // tools, which a packaged game's player never sees.
    enum class SettingRow { Display, Quality, Volume, Gpu, Back };

    class SettingsScreen
    {
    public:
        explicit SettingsScreen(std::vector<SettingRow> rows = { SettingRow::Display, SettingRow::Quality,
                                                                 SettingRow::Volume, SettingRow::Gpu, SettingRow::Back });

        struct Result
        {
            bool back = false;                  // leave the screen
            std::optional<SettingRow> changed;  // a value changed: apply it and save
        };
        // `move` -1/+1 between rows, `step` -1/+1 the value, `choose`
        // (Enter: the next value, or Back), `cancel` (Esc). Pure: tested
        // without a window.
        Result Navigate(int move, int step, bool choose, bool cancel, GameSettings& settings);
        Result Update(const Atom::Input& input, GameSettings& settings);

        // `running`: the GPU this run uses - a different saved choice says
        // "restart to apply".
        void Draw(UiKit& kit, const GameSettings& settings, GpuPreference running) const;

        static std::string Label(SettingRow row);
        static std::string Value(SettingRow row, const GameSettings& settings);
        int Selected() const { return m_selected; }
        const std::vector<SettingRow>& Rows() const { return m_rows; }

    private:
        std::vector<SettingRow> m_rows;
        int m_selected = 0;
    };

    // What applies at once, wherever the settings change: the window's
    // display mode and the volume of the mixer and of a synth stream.
    void ApplyDisplayAndVolume(SDL_Window* window, Atom::AudioSystem* audio, Atom::SynthStream* synth,
                               const GameSettings& settings);
}
