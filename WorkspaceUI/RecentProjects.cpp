#include "RecentProjects.h"

#include <algorithm>
#include <fstream>

namespace AtomWorkspace
{
    void RecentProjects::Load(const std::string& file)
    {
        m_entries.clear();
        std::ifstream in(file);
        std::string line;
        while (std::getline(in, line) && m_entries.size() < MaxEntries)
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (!line.empty() && std::find(m_entries.begin(), m_entries.end(), line) == m_entries.end())
            {
                m_entries.push_back(line);
            }
        }
    }

    bool RecentProjects::Save(const std::string& file) const
    {
        std::ofstream out(file, std::ios::trunc);
        for (const std::string& entry : m_entries)
        {
            out << entry << '\n';
        }
        return static_cast<bool>(out);
    }

    void RecentProjects::Add(const std::string& descriptorPath)
    {
        if (descriptorPath.empty())
        {
            return;
        }
        Remove(descriptorPath);
        m_entries.insert(m_entries.begin(), descriptorPath);
        if (m_entries.size() > MaxEntries)
        {
            m_entries.resize(MaxEntries);
        }
    }

    void RecentProjects::Remove(const std::string& descriptorPath)
    {
        m_entries.erase(std::remove(m_entries.begin(), m_entries.end(), descriptorPath), m_entries.end());
    }
}
