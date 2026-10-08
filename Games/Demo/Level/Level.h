#pragma once

#include "Audio/AudioSystem.h"
#include "Level/LevelData.h"
#include "Level/ModelCache.h"
#include "Renderer/Renderer.h"
#include "Physics/CollisionWorld.h"
#include "Platform/AssetRoots.h"
#include "World/GameWorld.h"
#include "World/FixedStep.h"
#include "World/Impostors.h"
#include "Pachinko/PachinkoGame.h"
#include "World/PachinkoAttract.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Atom
{
    class Model;
    class Renderer;
    class Texture;
    struct Material;
}

namespace AtomGame
{
    class AudioScape;

    // A loaded level: owns everything that exists only while you're in it.
    //
    //   owned here (dies on unload)      owned by the game (survives)
    //   ---------------------------      ----------------------------
    //   scene + entity models            player, camera, GameState
    //   collision world                  renderer, audio system
    //   entities (GameWorld)             sound library, fonts, dialogue
    //   ambience voices it started       settings
    //
    // Destruction is RAII: ~Level stops its voices, then members die in
    // reverse declaration order - entities (which point at models) go
    // before the models themselves.
    // Where an entity's model is drawn: its position, yaw and scale.
    glm::mat4 EntityModelTransform(const Entity& entity);

    class Level
    {
    public:
        struct Services
        {
            Atom::Renderer& renderer;
            Atom::AudioSystem& audio;
            const AudioScape& sounds;
            AtomFramework::AssetRoots assets; // v0.0.14: where "Kit/wall.glb" is found
            ModelCache& models;
        };

        static std::unique_ptr<Level> Create(LevelData data, Services& services);
        ~Level();

        Level(const Level&) = delete;
        Level& operator=(const Level&) = delete;

        const LevelData& GetData() const { return m_data; }
        // Developer tools (M41): tune the light live; a hot reload of the
        // level file puts back what the file says.
        LevelLighting& EditLighting() { return m_data.lighting; }
        const std::string& GetName() const { return m_data.name; }
        GameWorld& GetWorld() { return m_world; }
        const GameWorld& GetWorld() const { return m_world; }
        const Atom::CollisionWorld& GetCollision() const { return m_collision; }

        // Draws the level as seen from `viewer` (the player's position
        // decides which cells are drawn).
        void Submit(Atom::Renderer& renderer, const glm::vec3& viewer) const;

        // Advances animations (and their sounds), movers, flickering
        // materials, and the cell ambience crossfade around `listener`.
        void Update(float deltaSeconds, const glm::vec3& listener);

        // The level's night sky panorama, if it has one.
        const Atom::Texture* GetSkyPanorama() const { return m_skyPanorama.get(); }

        // Starts `clip` on the named entity's model. A finished one-shot
        // clip stays finished (a door opened once stays open).
        bool PlayAnimation(const std::string& entity, const std::string& clip);

        // Material of the level's scene model, for runtime effects.
        // Every material of that name in the scene and its chunks (each
        // model file has its own copy), for runtime effects on all of them.
        std::vector<Atom::Material*> FindSceneMaterials(std::string_view name);

        // For sequences (M26): show or hide an entity (and what rides on
        // it), move it, play a sound that follows it until the level unloads
        // or the sound ends.
        bool SetEntityVisible(const std::string& entity, bool visible);
        std::optional<glm::vec3> GetEntityPosition(const std::string& entity) const;
        bool SetEntityPosition(const std::string& entity, const glm::vec3& position);
        void PlaySound(const Atom::SoundHandle& sound, const std::string& entity, float gain, bool loop);

        // Scales the gain of every emitter in a group (e.g. "vending").
        void SetGroupGain(std::string_view group, float scale);
        std::size_t GetVoiceCount() const { return m_voices.size(); }

        // A live screen the game takes over (M29): its attract loop pauses and
        // the caller draws into the returned target until it hands it back.
        Atom::RenderTexture* TakeOverScreen(std::string_view material);
        void ReleaseScreen(std::string_view material);

        // Cell ambience (M25): how far each zone bed has faded in (0..1).
        float GetZoneLevel(std::string_view cell) const;

    private:
        struct OwnedVoice
        {
            Atom::VoiceId id = 0;
            std::string group;
            float baseGain = 1.0f;
        };

        explicit Level(LevelData data, Atom::AudioSystem& audio);
        const Atom::Model* LoadModel(const std::string& relativePath, Services& services);
        void SubmitChunk(Atom::Renderer& renderer, const Atom::Model& model,
                         Atom::RenderLayer layer, bool castsShadow) const;
        void AddCollider(const glm::vec3& position, float yawRadians, const ColliderBox& box);

        LevelData m_data;
        Atom::AudioSystem& m_audio;

        std::unique_ptr<Atom::Texture> m_lightmap; // declared first: outlives the scene
        std::unique_ptr<Atom::Texture> m_skyPanorama;
        std::vector<std::unique_ptr<Atom::Texture>> m_chunkLightmaps; // M25, also before the models
        // Live screens (M27): declared before the models whose materials
        // sample them, so they outlive those materials.
        struct Screen
        {
            std::unique_ptr<Atom::RenderTexture> target;
            PachinkoAttract attract;
            FixedStep clock;
            std::string material;
            bool takenOver = false;
            std::optional<PachinkoGame> demo; // M32: the real game, playing itself
        };
        std::vector<Screen> m_screens;
        float m_time = 0.0f; // for halo flicker
        std::shared_ptr<Atom::Model> m_scene;
        struct Chunk
        {
            std::shared_ptr<Atom::Model> model;
            Atom::RenderLayer layer = Atom::RenderLayer::Near;
            bool castsShadow = true;
            int cell = -1; // index into m_data.cells
        };
        std::vector<Chunk> m_chunks;
        std::unordered_map<std::string, std::unique_ptr<ImpostorSet>> m_impostorSets;
        struct Impostor
        {
            const ImpostorSet* set = nullptr;
            glm::vec3 position{ 0.0f };
            float yaw = 0.0f; // radians
            Atom::RenderLayer layer = Atom::RenderLayer::Far;
            mutable int view = -1; // shown last frame (hysteresis)
        };
        std::vector<Impostor> m_impostors;
        std::unordered_map<std::string, std::shared_ptr<Atom::Model>> m_models;
        Atom::CollisionWorld m_collision;
        GameWorld m_world; // after the models: entities die first
        std::vector<OwnedVoice> m_voices;

        // Live effects (M25).
        std::optional<EntityId> FindEntity(const std::string& name) const;
        glm::vec3 Anchor(const std::optional<EntityId>& entity, const glm::vec3& offset) const;
        bool IsHidden(const std::optional<EntityId>& entity) const;
        struct Mover
        {
            EntityId entity;
            glm::vec3 start{ 0.0f };
            EntityMover data;
            Atom::VoiceId voice = 0;
        };
        std::vector<Mover> m_movers;
        std::vector<std::optional<EntityId>> m_haloAnchors;  // per m_data.halos
        struct LiveLight
        {
            std::optional<EntityId> anchor;
            std::vector<std::pair<Atom::Material*, glm::vec3>> materials; // base emission
        };
        std::vector<LiveLight> m_lights;                      // per m_data.lights
        struct ZoneVoice
        {
            Atom::VoiceId id = 0;
            int cell = -1;
            float gain = 1.0f;
            float level = 0.0f;
        };
        std::vector<ZoneVoice> m_zoneVoices;
        struct FollowingVoice
        {
            Atom::VoiceId id = 0;
            EntityId entity;
        };
        std::vector<FollowingVoice> m_followingVoices; // M26
    };
}
