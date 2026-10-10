#pragma once

#include "Settings/GameSettings.h"

#include <string>
#include <string_view>

namespace AtomFramework
{
    // Where an app's settings live between runs (v0.0.14, M90; the demo's
    // since M61): settings.json under SDL's per-user pref path, one folder
    // per app (...\AtomEngine\<app>\). A run that must behave the same on
    // every machine - a scripted test, a benchmark, --no-settings - opens
    // it without persisting: defaults in, nothing written.
    class SettingsStore
    {
    public:
        static std::string PathFor(std::string_view app); // "" if there's no pref path

        // Loads (or, with `reset`, writes the defaults). A bad file gives the
        // defaults and Warning().
        void Open(std::string_view app, bool persist, bool reset);
        void Save() const; // only when persisting

        GameSettings& Settings() { return m_settings; }
        const GameSettings& Settings() const { return m_settings; }
        bool Persists() const { return m_persist; }
        const std::string& Path() const { return m_path; }
        const std::string& Warning() const { return m_warning; }

    private:
        GameSettings m_settings;
        std::string m_path;
        std::string m_warning;
        bool m_persist = false;
    };
}
