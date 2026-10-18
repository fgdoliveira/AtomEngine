#include "Project/ProjectDiscovery.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace AtomProject
{
    namespace fs = std::filesystem;

    std::string DiscoveredProject::AssetsDir() const
    {
        return (fs::path(directory) / descriptor.assets).lexically_normal().string();
    }

    namespace
    {
        void Scan(const fs::path& folder, int depthLeft, Discovery& out)
        {
            std::error_code error;
            for (const auto& entry : fs::directory_iterator(folder, error))
            {
                const std::string name = entry.path().filename().string();
                if (name.starts_with(".")) // .<Name>.creating, .git...
                {
                    continue;
                }
                if (entry.is_regular_file(error) && name.ends_with(".atomproject.json"))
                {
                    const DescriptorLoad load = LoadDescriptor(entry.path().string());
                    if (load.descriptor)
                    {
                        out.projects.push_back({ *load.descriptor, entry.path().string(), folder.string() });
                    }
                    else
                    {
                        out.errors.push_back(load.error);
                    }
                }
                else if (entry.is_directory(error) && depthLeft > 1)
                {
                    Scan(entry.path(), depthLeft - 1, out);
                }
            }
        }
    }

    Discovery FindProjects(const std::string& appsDir, int maxDepth)
    {
        Discovery out;
        std::error_code error;
        if (fs::is_directory(appsDir, error))
        {
            std::error_code ignored;
            for (const auto& entry : fs::directory_iterator(appsDir, ignored))
            {
                if (entry.is_directory(ignored) && !entry.path().filename().string().starts_with("."))
                {
                    Scan(entry.path(), maxDepth, out);
                }
            }
        }
        std::sort(out.projects.begin(), out.projects.end(),
                  [](const DiscoveredProject& a, const DiscoveredProject& b) { return a.descriptor.name < b.descriptor.name; });
        return out;
    }
}
