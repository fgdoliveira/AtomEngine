#include "Settings/SettingsStore.h"
#include "Core/Log.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <fstream>
#include <iostream>
#include <iterator>

namespace AtomFramework
{
    std::string SettingsStore::PathFor(std::string_view app)
    {
        std::string path;
        if (char* pref = SDL_GetPrefPath("AtomEngine", std::string(app).c_str()))
        {
            path = std::string(pref) + "settings.json";
            SDL_free(pref);
        }
        return path;
    }

    void SettingsStore::Open(std::string_view app, bool persist, bool reset)
    {
        m_settings = GameSettings{};
        m_warning.clear();
        m_path = PathFor(app);
        m_persist = persist && !m_path.empty();
        if (!m_persist)
        {
            return; // defaults and explicit flags only
        }
        if (reset)
        {
            Save();
            ATOM_LOG_INFO("Settings") << "Settings reset: " << m_path << '\n';
            return;
        }
        std::ifstream file(m_path, std::ios::binary);
        const std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        SettingsLoad load = ParseSettings(text);
        m_warning = std::move(load.warning);
        if (!m_warning.empty())
        {
            ATOM_LOG_ERROR("Settings") << m_path << ": " << m_warning << '\n';
        }
        m_settings = std::move(load.settings);
    }

    void SettingsStore::Save() const
    {
        if (!m_persist)
        {
            return;
        }
        std::ofstream file(m_path, std::ios::binary | std::ios::trunc);
        file << WriteSettings(m_settings);
        if (!file)
        {
            ATOM_LOG_ERROR("Settings") << "Could not save settings to " << m_path << '\n';
        }
    }
}
