#pragma once

#include <map>
#include <string>
#include <string_view>
#include <unordered_set>

namespace AtomGame
{
    // Progress that outlives any single level: named story flags such as
    // "keeper_permission", and (M33) named counters such as "tokens" and
    // "balls". Owned by the game, not by a level, so it survives level
    // changes (M12).
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
        // For inspection (developer tools).
        const std::unordered_set<std::string>& GetFlags() const { return m_flags; }
        const std::map<std::string, int, std::less<>>& GetCounters() const { return m_counters; }

        // Counters (M33): missing ones read 0; they never go below 0.
        int GetCounter(std::string_view name) const
        {
            const auto found = m_counters.find(std::string(name));
            return found != m_counters.end() ? found->second : 0;
        }
        void SetCounter(std::string_view name, int value) { m_counters[std::string(name)] = value < 0 ? 0 : value; }
        void AddToCounter(std::string_view name, int amount) { SetCounter(name, GetCounter(name) + amount); }
        // Takes `amount` if there is that much; false (and nothing taken) if not.
        bool Spend(std::string_view name, int amount)
        {
            if (GetCounter(name) < amount)
            {
                return false;
            }
            AddToCounter(name, -amount);
            return true;
        }

    private:
        std::unordered_set<std::string> m_flags;
        std::map<std::string, int, std::less<>> m_counters;
    };
}
