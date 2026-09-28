#pragma once

#include "Interaction/Actions.h"
#include "World/GameWorld.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace AtomGame
{
    // Everything a level file (Assets/Levels/<name>.json) describes. Plain
    // data: parsing produces it, Level turns it into live objects.

    // Footstep surface names a level may use, in SoundSynth::Surface order.
    inline constexpr std::array<std::string_view, 5> FootstepSurfaces{
        "asphalt", "concrete", "dirt", "stone", "wood" };

    struct SpawnPoint
    {
        glm::vec3 position{ 0.0f };
        float yawDegrees = 0.0f; // 0 looks down -Z, 90 looks down +X
    };

    struct LevelLighting
    {
        glm::vec3 sunDirection{ 0.35f, 0.6f, -0.55f };
        glm::vec3 sunColor{ 0.45f };
        glm::vec3 skyColor{ 0.75f, 0.77f, 0.80f };
        glm::vec3 groundColor{ 0.20f, 0.19f, 0.17f };
        glm::vec3 fogColor{ 0.46f, 0.47f, 0.47f };
        bool shadows = true;
        float bakedLight = 1.0f; // weight of the vertex-colour bake (M15)
    };

    struct AudioBed
    {
        std::string sound;
        float gain = 1.0f;
    };

    struct AudioEmitter
    {
        std::string sound;
        std::string group; // lets gameplay address several emitters at once
        glm::vec3 position{ 0.0f };
        float gain = 1.0f;
        float minDistance = 1.0f;
        float maxDistance = 20.0f;
    };

    // Footstep surfaces by area on the ground plane; first match wins.
    struct SurfaceZone
    {
        glm::vec2 min{ 0.0f }; // x, z
        glm::vec2 max{ 0.0f };
        std::string surface;
    };

    struct ColliderBox
    {
        glm::vec3 center{ 0.0f }; // relative to the entity
        glm::vec3 halfExtents{ 0.5f };
    };

    struct EntityAnimation
    {
        std::string clip;
        bool loop = true;
        bool autoplay = true;
        float speed = 1.0f;
        std::string sound;        // optional, from the sound library
        int soundsPerLoop = 1;
        glm::vec3 soundOffset{ 0.0f, 1.0f, 0.0f };
    };

    struct EntityData
    {
        std::string name;
        glm::vec3 position{ 0.0f };
        float yawDegrees = 0.0f;
        std::string model; // optional, relative to Assets/
        std::optional<ColliderBox> collider;
        std::optional<Interactable> interactable;
        std::optional<EntityAnimation> animation; // needs a model with the clip
    };

    struct LevelUnease
    {
        bool figure = false;
        std::vector<glm::vec3> figureSpots;
        std::string flickerMaterial;
        std::vector<glm::vec3> flickerSites;
    };

    // Baked light texture for the scene model (M16), mapped by its second
    // UV set. Written by the asset build next to the model.
    struct LevelLightmap
    {
        std::string texture;    // relative to Assets/
        float intensity = 1.0f;
    };

    struct LevelData
    {
        std::string name;
        std::string model;     // relative to Assets/
        std::string collision; // relative to Assets/
        std::optional<LevelLightmap> lightmap;
        std::string defaultSpawn;
        std::unordered_map<std::string, SpawnPoint> spawns;
        LevelLighting lighting;
        std::vector<AudioBed> beds;
        std::vector<AudioEmitter> emitters;
        std::string defaultSurface = "dirt";
        std::vector<SurfaceZone> surfaces;
        bool outdoor = true; // distant cicada calls, sky
        bool leaves = true;
        bool fogBanks = true;
        LevelUnease unease;
        std::vector<EntityData> entities;

        const SpawnPoint* FindSpawn(std::string_view spawnName) const;
        std::string_view SurfaceAt(float x, float z) const;
    };

    struct LevelParseResult
    {
        std::optional<LevelData> level;
        std::string error;
    };

    LevelParseResult ParseLevel(std::string_view json);
    LevelParseResult LoadLevelFile(const std::string& path);
}
