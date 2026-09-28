#pragma once

#include "Audio/AudioSystem.h"
#include "Level/LevelData.h"
#include "Level/ModelCache.h"
#include "Renderer/Renderer.h"
#include "Physics/CollisionWorld.h"
#include "World/GameWorld.h"
#include "World/Impostors.h"

#include <memory>
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
    class Level
    {
    public:
        struct Services
        {
            Atom::Renderer& renderer;
            Atom::AudioSystem& audio;
            const AudioScape& sounds;
            std::string assetRoot; // folder containing Assets/
            ModelCache& models;
        };

        static std::unique_ptr<Level> Create(LevelData data, Services& services);
        ~Level();

        Level(const Level&) = delete;
        Level& operator=(const Level&) = delete;

        const LevelData& GetData() const { return m_data; }
        const std::string& GetName() const { return m_data.name; }
        GameWorld& GetWorld() { return m_world; }
        const GameWorld& GetWorld() const { return m_world; }
        const Atom::CollisionWorld& GetCollision() const { return m_collision; }

        // Draws the level as seen from `viewer` (the player's position
        // decides which cells are drawn).
        void Submit(Atom::Renderer& renderer, const glm::vec3& viewer) const;

        // Advances animations and fires their sounds.
        void Update(float deltaSeconds);

        // The level's night sky panorama, if it has one.
        const Atom::Texture* GetSkyPanorama() const { return m_skyPanorama.get(); }

        // Starts `clip` on the named entity's model. A finished one-shot
        // clip stays finished (a door opened once stays open).
        bool PlayAnimation(const std::string& entity, const std::string& clip);

        // Material of the level's scene model, for runtime effects.
        // Every material of that name in the scene and its chunks (each
        // model file has its own copy), for runtime effects on all of them.
        std::vector<Atom::Material*> FindSceneMaterials(std::string_view name);

        // Scales the gain of every emitter in a group (e.g. "vending").
        void SetGroupGain(std::string_view group, float scale);
        std::size_t GetVoiceCount() const { return m_voices.size(); }

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
    };
}
