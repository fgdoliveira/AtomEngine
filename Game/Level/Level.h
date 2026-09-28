#pragma once

#include "Audio/AudioSystem.h"
#include "Level/LevelData.h"
#include "Physics/CollisionWorld.h"
#include "World/GameWorld.h"

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

        void Submit(Atom::Renderer& renderer) const;

        // Material of the level's scene model, for runtime effects.
        Atom::Material* FindSceneMaterial(std::string_view name);

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
        void AddCollider(const glm::vec3& position, float yawRadians, const ColliderBox& box);

        LevelData m_data;
        Atom::AudioSystem& m_audio;

        std::unique_ptr<Atom::Texture> m_lightmap; // declared first: outlives the scene
        std::unique_ptr<Atom::Model> m_scene;
        std::unordered_map<std::string, std::unique_ptr<Atom::Model>> m_models;
        Atom::CollisionWorld m_collision;
        GameWorld m_world; // after the models: entities die first
        std::vector<OwnedVoice> m_voices;
    };
}
