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

    std::string LevelManager::Reload()
    {
        if (!m_level || m_phase != Phase::Idle)
        {
            return "no level to reload";
        }
        const std::string path = m_services.assetRoot + "Assets/Levels/" + m_level->GetName() + ".json";
        LevelParseResult parsed = LoadLevelFile(path);
        if (!parsed.level)
        {
            return parsed.error;
        }
        // Built before the old one goes: a broken model leaves you where you were.
        std::unique_ptr<Level> incoming = Level::Create(std::move(*parsed.level), m_services);
        if (!incoming)
        {
            return "level '" + m_level->GetName() + "' failed to build (see the log)";
        }
        if (onUnloading)
        {
            onUnloading(*m_level);
        }
        m_level = std::move(incoming);
        if (onReloaded)
        {
            onReloaded(*m_level);
        }
        std::cout << "Level '" << m_level->GetName() << "' reloaded\n";
        return {};
    }

    std::vector<std::string> LevelManager::GetSourceFiles() const
    {
        std::vector<std::string> files;
        if (!m_level)
        {
            return files;
        }
        const LevelData& data = m_level->GetData();
        const std::string assets = m_services.assetRoot + "Assets/";
        const std::string levelFile = assets + "Levels/" + data.name + ".json";
        files.push_back(levelFile);
        files.push_back(MarkersPathFor(levelFile));
        files.push_back(assets + data.model);
        files.push_back(assets + data.collision);
        if (data.lightmap)
        {
            files.push_back(assets + data.lightmap->texture);
        }
        for (const ChunkData& chunk : data.chunks)
        {
            files.push_back(assets + chunk.model);
            if (!chunk.collision.empty())
            {
                files.push_back(assets + chunk.collision);
            }
        }
        for (const ImpostorData& impostor : data.impostors)
        {
            files.push_back(assets + impostor.set);
        }
        for (const EntityData& entity : data.entities)
        {
            if (!entity.model.empty())
            {
                files.push_back(assets + entity.model);
            }
        }
        return files;
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
