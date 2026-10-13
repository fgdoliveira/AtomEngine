#pragma once

#include "Level/LevelData.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace AtomFramework
{
    struct Entity;

    // The F10 entity report's selection and what it shows (v0.0.17, M114).
    // Read-only: it looks at the loaded level's data and live entities and
    // changes nothing.
    //
    // The selection is two names - the level and the entity - never a
    // pointer or an EntityId: it is resolved against whatever is loaded,
    // every time it is asked for. A reload of the same level finds the
    // entity again (or drops it if it is gone); another level clears it.
    // So nothing the inspector holds can outlive what it points at.
    class EntityInspector
    {
    public:
        void Select(std::string_view level, std::string_view entity);
        void Clear();
        bool HasSelection() const { return !m_entity.empty(); }
        const std::string& SelectedEntity() const { return m_entity; }

        // The selected entity's index in `data.entities`, if the level
        // named `level` (the one loaded now) has it; otherwise the
        // selection is cleared and npos returned.
        std::size_t Resolve(std::string_view level, const LevelData& data);

        // The filter box: a case-insensitive substring of the name.
        static bool Matches(std::string_view name, std::string_view filter);

        // What the report shows for entity `index`: "label: value" lines -
        // where its position came from, transform, model, collider,
        // interaction, animation, movement - and the live state when the
        // entity exists in the world.
        static std::vector<std::string> Describe(const LevelData& data, std::size_t index, const Entity* live);

        // The level's warnings about entity `index` (M111): its JSON Pointer
        // is /entities/<index> or below.
        static std::vector<const LevelWarning*> WarningsFor(const LevelData& data, std::size_t index);

        static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    private:
        std::string m_level;
        std::string m_entity;
    };
}
