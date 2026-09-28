#include "Level/LevelManager.h"

#include <algorithm>
#include <iostream>

namespace AtomGame
{
    bool LevelManager::Load(const std::string& levelName, const std::string& spawnName)
    {
        const std::string path = m_services.assetRoot + "Assets/Levels/" + levelName + ".json";
        LevelParseResult parsed = LoadLevelFile(path);
        if (!parsed.level)
        {
            std::cerr << "Level '" << levelName << "' is invalid: " << parsed.error << '\n';
            return false;
        }

        const std::string spawn = spawnName.empty() ? parsed.level->defaultSpawn : spawnName;
        if (!parsed.level->FindSpawn(spawn))
        {
            std::cerr << "Level '" << levelName << "' has no spawn '" << spawn << "'\n";
            return false;
        }

        std::unique_ptr<Level> incoming = Level::Create(std::move(*parsed.level), m_services);
        if (!incoming)
        {
            return false;
        }

        if (m_level)
        {
            if (onUnloading)
            {
                onUnloading(*m_level);
            }
            m_level.reset(); // RAII: the old level cleans up after itself
        }
        m_level = std::move(incoming);

        if (onLoaded)
        {
            onLoaded(*m_level, *m_level->GetData().FindSpawn(spawn));
        }
        return true;
    }

    void LevelManager::RequestChange(const std::string& levelName, const std::string& spawnName)
    {
        if (m_phase != Phase::Idle)
        {
            return;
        }
        m_pendingLevel = levelName;
        m_pendingSpawn = spawnName;
        m_phase = Phase::FadingOut;
        std::cout << "Level change requested: " << levelName << " at '" << spawnName << "'\n";
    }

    void LevelManager::Update(float deltaSeconds)
    {
        switch (m_phase)
        {
        case Phase::Idle:
            break;

        case Phase::FadingOut:
            m_fade = std::min(1.0f, m_fade + deltaSeconds / fadeOutSeconds);
            if (m_fade >= 1.0f)
            {
                if (!Load(m_pendingLevel, m_pendingSpawn))
                {
                    std::cerr << "Staying in the current level\n";
                }
                m_phase = Phase::FadingIn;
            }
            break;

        case Phase::FadingIn:
            m_fade = std::max(0.0f, m_fade - deltaSeconds / fadeInSeconds);
            if (m_fade <= 0.0f)
            {
                m_phase = Phase::Idle;
            }
            break;
        }
    }
}
