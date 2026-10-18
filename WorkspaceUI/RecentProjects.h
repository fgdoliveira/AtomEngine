#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace AtomWorkspace
{
    // The workspace's recent projects (v0.0.19): descriptor paths, newest
    // first, kept in the user's settings (one path per line) - never in
    // the repository, where a machine's paths don't belong.
    class RecentProjects
    {
    public:
        static constexpr std::size_t MaxEntries = 8;

        void Load(const std::string& file);
        bool Save(const std::string& file) const;

        // Moves `descriptorPath` to the front (adding it if new); the
        // oldest falls off past MaxEntries. Paths compare as written.
        void Add(const std::string& descriptorPath);
        void Remove(const std::string& descriptorPath);
        const std::vector<std::string>& Entries() const { return m_entries; }

    private:
        std::vector<std::string> m_entries;
    };
}
