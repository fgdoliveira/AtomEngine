#include "Level/Level.h"

#include "Assets/Model.h"
#include "AudioScape.h"
#include "PlayerController.h"
#include "Renderer/Renderer.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glm/geometric.hpp>

#include <cmath>
#include <iostream>
#include <optional>
#include <string>

namespace AtomGame
{
    namespace
    {
        // A spawn must put the player on a floor (within a step) and clear
        // of walls; otherwise they arrive falling or get shoved on frame one.
        std::string CheckSpawn(const Atom::CollisionWorld& collision, const SpawnPoint& spawn)
        {
            const PlayerController body{}; // default body dimensions
            const glm::vec3 feet = spawn.position;

            const std::optional<float> floor = collision.FindFloor(
                feet + glm::vec3{ 0.0f, body.stepHeight, 0.0f }, 2.0f * body.stepHeight);
            if (!floor)
            {
                return "no floor under it";
            }

            // Same sphere stack the controller slides along walls.
            for (float height = body.radius + body.stepHeight; height <= body.bodyHeight - body.radius; height += body.radius)
            {
                glm::vec3 center = glm::vec3{ feet.x, *floor + height, feet.z };
                const glm::vec3 before = center;
                collision.ResolveSphereHorizontal(center, body.radius);
                if (glm::length(center - before) > 0.01f)
                {
                    return "inside or against a wall at " + std::to_string(height).substr(0, 4) + " m";
                }
            }
            return {};
        }
    }

    Level::Level(LevelData data, Atom::AudioSystem& audio)
        : m_data(std::move(data))
        , m_audio(audio)
    {
    }

    Level::~Level()
    {
        // Voices live in the (persistent) audio system; stop the ones this
        // level started so nothing keeps humming after we leave.
        for (const OwnedVoice& voice : m_voices)
        {
            m_audio.Stop(voice.id);
        }
        std::cout << "Level '" << m_data.name << "' unloaded (" << m_voices.size() << " voices stopped)\n";
    }

    std::unique_ptr<Level> Level::Create(LevelData data, Services& services)
    {
        std::unique_ptr<Level> level(new Level(std::move(data), services.audio));
        const LevelData& d = level->m_data;
        const std::string assets = services.assetRoot + "Assets/";

        level->m_scene = Atom::Model::Load(services.renderer, assets + d.model);
        if (!level->m_scene || !level->m_collision.Load(assets + d.collision))
        {
            std::cerr << "Level '" << d.name << "': failed to load its scene or collision\n";
            return nullptr;
        }
        if (d.lightmap)
        {
            // A level that names a lightmap is lit by it; without it the
            // room would silently fall back to flat light, so it's an error.
            level->m_lightmap = services.renderer.LoadTexture(assets + d.lightmap->texture);
            if (!level->m_lightmap)
            {
                std::cerr << "Level '" << d.name << "': missing lightmap " << d.lightmap->texture << '\n';
                return nullptr;
            }
            level->m_scene->SetLightmap(level->m_lightmap.get(), d.lightmap->intensity);
        }

        for (const EntityData& data : d.entities)
        {
            Entity entity;
            entity.name = data.name;
            entity.position = data.position;
            entity.interactable = data.interactable;

            const float yaw = glm::radians(data.yawDegrees);
            if (!data.model.empty())
            {
                const Atom::Model* model = level->LoadModel(data.model, services);
                if (!model)
                {
                    return nullptr;
                }
                entity.renderable = Renderable{ model, yaw };
            }
            if (data.collider)
            {
                level->AddCollider(data.position, yaw, *data.collider);
            }
            level->m_world.Spawn(std::move(entity));
        }

        for (const auto& [name, spawn] : d.spawns)
        {
            if (const std::string problem = CheckSpawn(level->m_collision, spawn); !problem.empty())
            {
                std::cerr << "Level '" << d.name << "': spawn '" << name << "' is invalid: " << problem << '\n';
                return nullptr;
            }
        }

        // Ambience that belongs to this place.
        for (const AudioBed& bed : d.beds)
        {
            Atom::PlayParams params{};
            params.loop = true;
            params.gain = bed.gain;
            const Atom::VoiceId id = services.audio.Play(services.sounds.GetSound(bed.sound), params);
            level->m_voices.push_back({ id, {}, bed.gain });
        }
        for (const AudioEmitter& emitter : d.emitters)
        {
            Atom::PlayParams params{};
            params.loop = true;
            params.gain = emitter.gain;
            params.spatial = true;
            params.position = emitter.position;
            params.minDistance = emitter.minDistance;
            params.maxDistance = emitter.maxDistance;
            const Atom::VoiceId id = services.audio.Play(services.sounds.GetSound(emitter.sound), params);
            level->m_voices.push_back({ id, emitter.group, emitter.gain });
        }

        std::cout
            << "Level '" << d.name << "' loaded: " << level->m_world.Count() << " entities, "
            << level->m_models.size() << " extra models, " << level->m_voices.size() << " voices\n";
        return level;
    }

    const Atom::Model* Level::LoadModel(const std::string& relativePath, Services& services)
    {
        // Several entities may share a model; load it once per level.
        auto& slot = m_models[relativePath];
        if (!slot)
        {
            slot = Atom::Model::Load(services.renderer, services.assetRoot + "Assets/" + relativePath);
        }
        return slot.get();
    }

    void Level::AddCollider(const glm::vec3& position, float yawRadians, const ColliderBox& box)
    {
        // The box turns with the entity; collision stores the axis-aligned
        // box around it (exact for the 90-degree steps levels use).
        const float c = std::abs(std::cos(yawRadians));
        const float s = std::abs(std::sin(yawRadians));
        const glm::vec3 half{
            box.halfExtents.x * c + box.halfExtents.z * s,
            box.halfExtents.y,
            box.halfExtents.x * s + box.halfExtents.z * c,
        };
        const glm::vec3 offset{
            box.center.x * std::cos(yawRadians) + box.center.z * std::sin(yawRadians),
            box.center.y,
            -box.center.x * std::sin(yawRadians) + box.center.z * std::cos(yawRadians),
        };
        const glm::vec3 center = position + offset;

        const auto corner = [&](int x, int y, int z) {
            return center + glm::vec3{ x ? half.x : -half.x, y ? half.y : -half.y, z ? half.z : -half.z };
        };
        const int faces[6][4][3] = {
            { { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 1, 0, 1 } },
            { { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 0 }, { 0, 0, 0 } },
            { { 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 } },
            { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } },
            { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } },
            { { 1, 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 } },
        };
        for (const auto& face : faces)
        {
            const glm::vec3 a = corner(face[0][0], face[0][1], face[0][2]);
            const glm::vec3 b = corner(face[1][0], face[1][1], face[1][2]);
            const glm::vec3 c2 = corner(face[2][0], face[2][1], face[2][2]);
            const glm::vec3 d = corner(face[3][0], face[3][1], face[3][2]);
            m_collision.AddTriangle(a, b, c2);
            m_collision.AddTriangle(a, c2, d);
        }
    }

    void Level::Submit(Atom::Renderer& renderer) const
    {
        m_scene->Submit(renderer, glm::mat4{ 1.0f });
        m_world.ForEach([&](EntityId, const Entity& entity) {
            if (!entity.renderable || !entity.renderable->model)
            {
                return;
            }
            glm::mat4 transform = glm::translate(glm::mat4{ 1.0f }, entity.position);
            transform = glm::rotate(transform, entity.renderable->yaw, glm::vec3{ 0.0f, 1.0f, 0.0f });
            entity.renderable->model->Submit(renderer, transform);
        });
    }

    Atom::Material* Level::FindSceneMaterial(std::string_view name)
    {
        return m_scene ? m_scene->FindMaterial(name) : nullptr;
    }

    void Level::SetGroupGain(std::string_view group, float scale)
    {
        for (const OwnedVoice& voice : m_voices)
        {
            if (voice.group == group)
            {
                m_audio.SetVoiceGain(voice.id, voice.baseGain * scale);
            }
        }
    }
}
