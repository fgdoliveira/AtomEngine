#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace AtomFramework
{
    // Where an app's asset files are (v0.0.14). Paths in level and data
    // files are relative to an asset root: "Kit/wall.glb",
    // "Levels/street.json". Beside the executable there is one root, the
    // build's merged copy (<exe>/Assets/). Reading the source tree - hot
    // reload, the unit tests - there are several: the app's own Assets/ and
    // the shared Content/. The first root that has a file wins.
    class AssetRoots
    {
    public:
        AssetRoots() = default;
        explicit AssetRoots(std::vector<std::string> roots); // a separator is added where missing

        // The file or folder under the first root that has it. When none
        // does, the path under the first root, so a missing file is reported
        // where it was expected.
        std::string Resolve(std::string_view relative) const;

        const std::vector<std::string>& Roots() const { return m_roots; }

    private:
        std::vector<std::string> m_roots;
    };
}
