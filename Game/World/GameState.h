#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

namespace AtomGame
{
    // Progress that outlives any single level: named story flags such as
    // "keeper_permission". Owned by the game, not by a level, so it survives
    // level changes (M12).
    class GameState
    {
    public:
        void SetFlag(std::string_view name, bool value = true)
        {
            if (value)
            {
                m_flags.emplace(name);
            }
            else
            {
                m_flags.erase(std::string(name));
            }
        }

        bool HasFlag(std::string_view name) const
        {
            return m_flags.contains(std::string(name));
        }

        std::size_t FlagCount() const { return m_flags.size(); }

    private:
        std::unordered_set<std::string> m_flags;
    };
}
