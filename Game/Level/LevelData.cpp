#include "Level/LevelData.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace AtomGame
{
    namespace
    {
        using Json = nlohmann::json;

        // Parse errors are reported as exceptions internally and turned into
        // a LevelParseResult at the boundary, so the helpers stay short.
        struct LevelError : std::runtime_error
        {
            using std::runtime_error::runtime_error;
        };

        std::string String(const Json& object, const char* key, std::string fallback = {})
        {
            const auto found = object.find(key);
            return found != object.end() && found->is_string() ? found->get<std::string>() : fallback;
        }

        float Number(const Json& object, const char* key, float fallback)
        {
            const auto found = object.find(key);
            return found != object.end() && found->is_number() ? found->get<float>() : fallback;
        }

        bool Bool(const Json& object, const char* key, bool fallback)
        {
            const auto found = object.find(key);
            return found != object.end() && found->is_boolean() ? found->get<bool>() : fallback;
        }

        glm::vec3 Vec3(const Json& value, const std::string& where)
        {
            if (!value.is_array() || value.size() != 3)
            {
                throw LevelError(where + " must be [x, y, z]");
            }
            return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
        }

        glm::vec3 Vec3(const Json& object, const char* key, glm::vec3 fallback, const std::string& where)
        {
            const auto found = object.find(key);
            return found != object.end() ? Vec3(*found, where + "." + key) : fallback;
        }

        Action ParseAction(const Json& json, const std::string& where)
        {
            const std::string type = String(json, "type");
            if (type == "message")
            {
                return ShowMessage{ String(json, "text") };
            }
            if (type == "setFlag")
            {
                const std::string flag = String(json, "flag");
                if (flag.empty())
                {
                    throw LevelError(where + ": setFlag needs \"flag\"");
                }
                return SetFlag{ flag, String(json, "message") };
            }
            if (type == "dialogue")
            {
                return StartDialogue{ String(json, "id") };
            }
            if (type == "changeLevel")
            {
                const std::string level = String(json, "level");
                if (level.empty())
                {
                    throw LevelError(where + ": changeLevel needs \"level\"");
                }
                return ChangeLevel{ level, String(json, "spawn") };
            }
            throw LevelError(where + ": unknown action type \"" + type + "\"");
        }

        EntityData ParseEntity(const Json& json, std::size_t index)
        {
            EntityData entity;
            entity.name = String(json, "name");
            const std::string where = entity.name.empty()
                ? "entity #" + std::to_string(index)
                : "entity \"" + entity.name + "\"";
            if (entity.name.empty())
            {
                throw LevelError(where + " has no name");
            }
            entity.position = Vec3(json, "position", glm::vec3{ 0.0f }, where);
            entity.yawDegrees = Number(json, "yaw", 0.0f);
            entity.model = String(json, "model");

            if (const auto collider = json.find("collider"); collider != json.end())
            {
                entity.collider = ColliderBox{
                    Vec3(*collider, "center", glm::vec3{ 0.0f }, where + ".collider"),
                    Vec3(*collider, "halfExtents", glm::vec3{ 0.5f }, where + ".collider"),
                };
            }

            if (const auto use = json.find("interactable"); use != json.end())
            {
                const auto action = use->find("action");
                if (action == use->end())
                {
                    throw LevelError(where + ": interactable needs an \"action\"");
                }
                Interactable interactable{
                    String(*use, "prompt", "Use"),
                    ParseAction(*action, where + ".action"),
                };
                interactable.focusOffset = Vec3(*use, "focus", glm::vec3{ 0.0f, 1.2f, 0.0f }, where);
                interactable.radius = Number(*use, "radius", 2.2f);
                interactable.requiresFlag = String(*use, "requires");
                if (const auto locked = use->find("locked"); locked != use->end())
                {
                    interactable.lockedAction = ParseAction(*locked, where + ".locked");
                }
                entity.interactable = std::move(interactable);
            }
            return entity;
        }

        LevelData Parse(const Json& root)
        {
            if (!root.is_object())
            {
                throw LevelError("not valid JSON");
            }

            LevelData level;
            level.name = String(root, "name");
            level.model = String(root, "model");
            level.collision = String(root, "collision");
            if (const auto lightmap = root.find("lightmap"); lightmap != root.end())
            {
                LevelLightmap map{ String(*lightmap, "texture"), Number(*lightmap, "intensity", 1.0f) };
                if (map.texture.empty() || map.intensity < 0.0f)
                {
                    throw LevelError("lightmap needs \"texture\" and a non-negative \"intensity\"");
                }
                level.lightmap = map;
            }
            if (level.name.empty() || level.model.empty() || level.collision.empty())
            {
                throw LevelError("needs \"name\", \"model\" and \"collision\"");
            }

            const auto spawns = root.find("spawns");
            if (spawns == root.end() || !spawns->is_object() || spawns->empty())
            {
                throw LevelError("needs at least one spawn in \"spawns\"");
            }
            for (const auto& [name, spawn] : spawns->items())
            {
                level.spawns[name] = SpawnPoint{
                    Vec3(spawn, "position", glm::vec3{ 0.0f }, "spawn \"" + name + "\""),
                    Number(spawn, "yaw", 0.0f),
                };
            }
            level.defaultSpawn = String(root, "defaultSpawn", spawns->begin().key());
            if (!level.FindSpawn(level.defaultSpawn))
            {
                throw LevelError("defaultSpawn \"" + level.defaultSpawn + "\" does not exist");
            }

            if (const auto light = root.find("lighting"); light != root.end())
            {
                LevelLighting& l = level.lighting;
                l.sunDirection = Vec3(*light, "sunDirection", l.sunDirection, "lighting");
                l.sunColor = Vec3(*light, "sunColor", l.sunColor, "lighting");
                l.skyColor = Vec3(*light, "skyColor", l.skyColor, "lighting");
                l.groundColor = Vec3(*light, "groundColor", l.groundColor, "lighting");
                l.fogColor = Vec3(*light, "fogColor", l.fogColor, "lighting");
                l.shadows = Bool(*light, "shadows", l.shadows);
                l.bakedLight = Number(*light, "bakedLight", l.bakedLight);
                if (l.bakedLight < 0.0f || l.bakedLight > 1.0f)
                {
                    throw LevelError("lighting.bakedLight must be between 0 and 1");
                }
            }

            if (const auto audio = root.find("audio"); audio != root.end())
            {
                for (const Json& bed : audio->value("beds", Json::array()))
                {
                    level.beds.push_back(AudioBed{ String(bed, "sound"), Number(bed, "gain", 1.0f) });
                }
                for (const Json& emitter : audio->value("emitters", Json::array()))
                {
                    level.emitters.push_back(AudioEmitter{
                        String(emitter, "sound"),
                        String(emitter, "group"),
                        Vec3(emitter, "position", glm::vec3{ 0.0f }, "audio emitter"),
                        Number(emitter, "gain", 1.0f),
                        Number(emitter, "minDistance", 1.0f),
                        Number(emitter, "maxDistance", 20.0f),
                    });
                }
            }

            if (const auto surfaces = root.find("surfaces"); surfaces != root.end())
            {
                // A misspelt surface would silently sound like the default.
                const auto known = [](const std::string& surface) {
                    if (std::find(FootstepSurfaces.begin(), FootstepSurfaces.end(), surface) == FootstepSurfaces.end())
                    {
                        throw LevelError("unknown footstep surface \"" + surface + "\"");
                    }
                    return surface;
                };
                level.defaultSurface = known(String(*surfaces, "default", level.defaultSurface));
                for (const Json& zone : surfaces->value("zones", Json::array()))
                {
                    const glm::vec3 min = Vec3(zone, "min", glm::vec3{ 0.0f }, "surface zone");
                    const glm::vec3 max = Vec3(zone, "max", glm::vec3{ 0.0f }, "surface zone");
                    level.surfaces.push_back(SurfaceZone{
                        { min.x, min.z }, { max.x, max.z }, known(String(zone, "surface", level.defaultSurface)) });
                }
            }

            level.outdoor = Bool(root, "outdoor", true);

            if (const auto particles = root.find("particles"); particles != root.end())
            {
                level.leaves = Bool(*particles, "leaves", true);
                level.fogBanks = Bool(*particles, "fogBanks", true);
            }

            if (const auto unease = root.find("unease"); unease != root.end())
            {
                level.unease.figure = Bool(*unease, "figure", false);
                for (const Json& spot : unease->value("figureSpots", Json::array()))
                {
                    level.unease.figureSpots.push_back(Vec3(spot, "unease.figureSpots"));
                }
                level.unease.flickerMaterial = String(*unease, "flickerMaterial");
                for (const Json& site : unease->value("flickerSites", Json::array()))
                {
                    level.unease.flickerSites.push_back(Vec3(site, "unease.flickerSites"));
                }
            }

            std::size_t index = 0;
            for (const Json& entity : root.value("entities", Json::array()))
            {
                level.entities.push_back(ParseEntity(entity, index++));
            }

            return level;
        }
    }

    const SpawnPoint* LevelData::FindSpawn(std::string_view spawnName) const
    {
        const auto found = spawns.find(std::string(spawnName));
        return found != spawns.end() ? &found->second : nullptr;
    }

    std::string_view LevelData::SurfaceAt(float x, float z) const
    {
        for (const SurfaceZone& zone : surfaces)
        {
            if (x >= zone.min.x && x <= zone.max.x && z >= zone.min.y && z <= zone.max.y)
            {
                return zone.surface;
            }
        }
        return defaultSurface;
    }

    LevelParseResult ParseLevel(std::string_view text)
    {
        const Json root = Json::parse(text, nullptr, false);
        if (root.is_discarded())
        {
            return { std::nullopt, "not valid JSON" };
        }
        try
        {
            return { Parse(root), {} };
        }
        catch (const LevelError& error)
        {
            return { std::nullopt, error.what() };
        }
        catch (const Json::exception& error)
        {
            // e.g. a string where a number was expected.
            return { std::nullopt, error.what() };
        }
    }

    LevelParseResult LoadLevelFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return { std::nullopt, "cannot open " + path };
        }
        std::stringstream contents;
        contents << file.rdbuf();
        return ParseLevel(contents.str());
    }
}
