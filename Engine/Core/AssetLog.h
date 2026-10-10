#pragma once

#include <string>

namespace Atom
{
    // Which asset files the game actually opens (M56, audit ASSET-001).
    // With ATOM_ASSET_LOG=<file> set, every loader reports the path it
    // opens and each new one is appended to that file; unset, it costs a
    // check. Run the scenarios with it and the union of the logs is the
    // evidence for the runtime asset payload (Games/Demo/CMakeLists.txt).
    namespace AssetLog
    {
        void Opened(const std::string& path);
    }
}
