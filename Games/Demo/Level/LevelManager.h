#pragma once

#include "Level/Level.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Demo
{
    // Owns the current level and moves between levels behind a fade:
    //
    //   Idle --RequestChange--> FadingOut --(black)--> swap --> FadingIn --> Idle
    //
    // The swap happens while the screen is black, so the loading hitch is
    // hidden. The new level is loaded *before* the old one is released:
    // briefly more memory, but a failed load leaves you where you were.
    class LevelManager
    {
    public:
        explicit LevelManager(Level::Services services) : m_services(std::move(services)) {}

        // Loads a level right away (no fade). Returns false on failure.
        bool Load(const std::string& levelName, const std::string& spawnName);

        void RequestChange(const std::string& levelName, const std::string& spawnName);

        // Hot reload (M20): loads the current level's files again and swaps
        // it in right away, without a fade; the player stays where they are.
        // Returns an empty string on success; on failure the error, and the
        // old level stays.
        std::string Reload();

        // Every file the current level was built from (level file, markers,
        // models, collision, lightmap), for watching.
        std::vector<std::string> GetSourceFiles() const;
        void Update(float deltaSeconds);

        Level* GetLevel() { return m_level.get(); }
        const Level* GetLevel() const { return m_level.get(); }
        bool IsTransitioning() const { return m_phase != Phase::Idle; }
        float GetFade() const { return m_fade; }

        // Called just before the old level is destroyed, so systems can drop
        // anything that points into it.
        std::function<void(Level& outgoing)> onUnloading;
        // Called once the new level is current, with the chosen spawn.
        std::function<void(Level& incoming, const SpawnPoint& spawn)> onLoaded;
        // Called after a hot reload replaced the level in place.
        std::function<void(Level& incoming)> onReloaded;

        float fadeOutSeconds = 0.6f;
        float fadeInSeconds = 0.9f;

    private:
        enum class Phase
        {
            Idle,
            FadingOut,
            FadingIn,
        };

        Level::Services m_services;
        std::unique_ptr<Level> m_level;
        Phase m_phase = Phase::Idle;
        float m_fade = 0.0f;
        std::string m_pendingLevel;
        std::string m_pendingSpawn;
    };
}
