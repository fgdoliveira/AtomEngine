#pragma once

#include "Level/Environment.h"

#include <map>
#include <string>
#include <vector>

namespace AtomFramework
{
    class Level;

    // The environment presets an app offers (M49; shared since v0.0.14):
    // every Environments/*.json, checked as it loads. A preset is laid over
    // a level's own light, so the same "rain" works in any level.
    class EnvironmentPresets
    {
    public:
        // Loads every .json in `folder`; a broken one is left out and its
        // problem returned ("<file>: <problem>").
        std::vector<std::string> Load(const std::string& folder);

        bool Has(const std::string& name) const { return m_presets.count(name) != 0; }

        // The level's own light (null level: defaults) with the named
        // preset on top; "" or an unknown name gives the level's own.
        EnvironmentState Resolve(const Level* level, const std::string& name) const;

        // The level's list of presets; a level without one may try them all.
        std::vector<std::string> Offered(const Level* level) const;

    private:
        std::map<std::string, std::string> m_presets; // name -> JSON text
    };
}
