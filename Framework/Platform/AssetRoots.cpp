#include "Platform/AssetRoots.h"

#include <filesystem>
#include <system_error>

namespace AtomFramework
{
    AssetRoots::AssetRoots(std::vector<std::string> roots) : m_roots(std::move(roots))
    {
        for (std::string& root : m_roots)
        {
            if (!root.empty() && root.back() != '/' && root.back() != '\\')
            {
                root += '/';
            }
        }
    }

    std::string AssetRoots::Resolve(std::string_view relative) const
    {
        if (m_roots.empty())
        {
            return std::string(relative);
        }
        if (m_roots.size() > 1)
        {
            std::error_code error;
            for (const std::string& root : m_roots)
            {
                std::string path = root;
                path += relative;
                if (std::filesystem::exists(path, error))
                {
                    return path;
                }
            }
        }
        // One root (the usual case, beside the executable): no file-system
        // query per lookup.
        std::string path = m_roots.front();
        path += relative;
        return path;
    }
}
