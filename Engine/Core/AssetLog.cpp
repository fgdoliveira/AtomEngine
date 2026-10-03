#include "Core/AssetLog.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace Atom
{
    namespace AssetLog
    {
        void Opened(const std::string& path)
        {
            static const char* target = SDL_getenv("ATOM_ASSET_LOG");
            if (!target || !*target)
            {
                return;
            }
            // Once per path; forward slashes, so logs from any loader compare.
            static std::unordered_set<std::string> seen;
            const std::string normal = std::filesystem::path(path).lexically_normal().generic_string();
            if (seen.insert(normal).second)
            {
                std::ofstream(target, std::ios::app) << normal << '\n';
            }
        }
    }
}
