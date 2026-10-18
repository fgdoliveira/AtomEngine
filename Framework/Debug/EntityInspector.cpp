#include "Debug/EntityInspector.h"

#include "World/GameWorld.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <type_traits>
#include <variant>

namespace AtomFramework
{
    namespace
    {
        std::string Vec(const glm::vec3& v)
        {
            char text[96];
            std::snprintf(text, sizeof(text), "%.2f %.2f %.2f", v.x, v.y, v.z);
            return text;
        }

        std::string Number(float value)
        {
            char text[32];
            std::snprintf(text, sizeof(text), "%.2f", value);
            return text;
        }

        // An action as one line: its type and what it names.
        std::string DescribeAction(const Action& action)
        {
            return std::visit([](const auto& a) -> std::string {
                using T = std::decay_t<decltype(a)>;
                if constexpr (std::is_same_v<T, ShowMessage>) return "message \"" + a.text + "\"";
                else if constexpr (std::is_same_v<T, SetFlag>) return "setFlag " + a.flag;
                else if constexpr (std::is_same_v<T, StartDialogue>) return "dialogue " + a.dialogueId;
                else if constexpr (std::is_same_v<T, ChangeLevel>)
                    return "changeLevel " + a.level + (a.spawn.empty() ? "" : " : " + a.spawn);
                else if constexpr (std::is_same_v<T, PlayAnimation>) return "playAnimation " + a.entity + " " + a.clip;
                else if constexpr (std::is_same_v<T, RunSequence>) return "sequence " + a.id;
                else if constexpr (std::is_same_v<T, PlayMachine>) return "playMachine " + a.machine;
                else if constexpr (std::is_same_v<T, AddCounter>) return "addCounter " + a.counter;
                else if constexpr (std::is_same_v<T, Exchange>) return "exchange " + a.counter + " -> " + a.flag;
                else if constexpr (std::is_same_v<T, SaveProgress>) return "save at " + a.place;
                else static_assert(sizeof(T) == 0, "describe the new action type here");
            }, action);
        }
    }

    void EntityInspector::Select(std::string_view level, std::string_view entity)
    {
        m_level = level;
        m_entity = entity;
    }

    void EntityInspector::Clear()
    {
        m_level.clear();
        m_entity.clear();
    }

    std::size_t EntityInspector::Resolve(std::string_view level, const LevelData& data)
    {
        if (m_entity.empty())
        {
            return npos;
        }
        if (level == m_level)
        {
            for (std::size_t i = 0; i < data.entities.size(); ++i)
            {
                if (data.entities[i].name == m_entity)
                {
                    return i;
                }
            }
        }
        Clear(); // another level, or the entity is gone after a reload
        return npos;
    }

    bool EntityInspector::Matches(std::string_view name, std::string_view filter)
    {
        if (filter.empty())
        {
            return true;
        }
        const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
        const auto found = std::search(name.begin(), name.end(), filter.begin(), filter.end(),
                                       [&](char a, char b) { return lower(a) == lower(b); });
        return found != name.end();
    }

    std::vector<std::string> EntityInspector::Describe(const LevelData& data, std::size_t index, const Entity* live)
    {
        std::vector<std::string> lines;
        if (index >= data.entities.size())
        {
            return lines;
        }
        const EntityData& e = data.entities[index];
        lines.push_back("json: /entities/" + std::to_string(index));
        lines.push_back("placed by: " + std::string(PlacementSourceName(e.placement)));
        lines.push_back("position: " + Vec(e.position) + "   yaw: " + Number(e.yawDegrees)
                        + (e.scale != 1.0f ? "   scale: " + Number(e.scale) : ""));
        if (!e.model.empty())
        {
            lines.push_back("model: " + e.model);
        }
        else
        {
            // Often an interaction point on something the level's own
            // model draws (the street's vending machines).
            lines.push_back(e.collider ? "model: none" : "model: none, collider: none - a point; what you see "
                                                         "there is part of the level's model");
        }
        if (e.collider)
        {
            lines.push_back("collider: centre " + Vec(e.collider->center) + ", half " + Vec(e.collider->halfExtents));
        }
        if (e.interactable)
        {
            std::string line = "interactable: \"" + e.interactable->prompt + "\" -> " + DescribeAction(e.interactable->action);
            if (!e.interactable->requiresFlag.empty())
            {
                line += "   needs " + e.interactable->requiresFlag;
            }
            lines.push_back(line);
            lines.push_back("  look at: " + Vec(e.position + e.interactable->focusOffset) + " (focus "
                            + Vec(e.interactable->focusOffset) + "), within " + Number(e.interactable->radius) + " m");
            if (e.interactable->lockedAction)
            {
                lines.push_back("  locked -> " + DescribeAction(*e.interactable->lockedAction));
            }
        }
        if (e.animation)
        {
            lines.push_back("animation: " + e.animation->clip + (e.animation->loop ? " (loop)" : "")
                            + (e.animation->autoplay ? "" : " (on demand)"));
        }
        if (e.animator)
        {
            lines.push_back("animator: " + std::to_string(e.animator->states.size()) + " states, starts in "
                            + e.animator->initial);
        }
        if (e.mover)
        {
            lines.push_back("mover: travels " + Vec(e.mover->travel) + " in " + Number(e.mover->travelSeconds)
                            + " s, waits " + Number(e.mover->waitSeconds) + " s");
        }
        if (e.style)
        {
            lines.push_back(std::string("style:") + (e.style->toon ? " toon" : "")
                            + (e.style->outline > 0.0f ? " outline " + Number(e.style->outline) : ""));
        }
        if (e.hidden || !e.goneWithFlag.empty())
        {
            lines.push_back(std::string("starts hidden: ") + (e.hidden ? "yes" : "no")
                            + (e.goneWithFlag.empty() ? "" : "   gone with flag " + e.goneWithFlag));
        }
        if (live)
        {
            std::string state = "now: at " + Vec(live->position);
            state += live->hidden ? ", hidden" : ", shown";
            if (live->animated)
            {
                state += live->animated->playing ? ", animating" : ", animation stopped";
            }
            lines.push_back(state);
        }
        else
        {
            lines.push_back("now: not in the world (gone)");
        }
        return lines;
    }

    std::vector<const LevelWarning*> EntityInspector::WarningsFor(const LevelData& data, std::size_t index)
    {
        std::vector<const LevelWarning*> found;
        const std::string prefix = "/entities/" + std::to_string(index);
        for (const LevelWarning& warning : data.warnings)
        {
            if (warning.path == prefix || warning.path.rfind(prefix + "/", 0) == 0)
            {
                found.push_back(&warning);
            }
        }
        return found;
    }
}
