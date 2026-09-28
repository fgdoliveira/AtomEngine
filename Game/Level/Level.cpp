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

        level->m_scene = services.models.Get(services.renderer, assets + d.model);
        if (!level->m_scene || !level->m_collision.Load(assets + d.collision))
        {
            std::cerr << "Level '" << d.name << "': failed to load its scene or collision\n";
            return nullptr;
        }

        if (d.sky)
        {
            level->m_skyPanorama = services.renderer.LoadTexture(assets + d.sky->panorama);
            if (!level->m_skyPanorama)
            {
                std::cerr << "Level '" << d.name << "': missing sky " << d.sky->panorama << '\n';
                return nullptr;
            }
        }

        // Chunks (M22): their models through the shared cache, their
        // collision added to the level's.
        for (const ChunkData& data : d.chunks)
        {
            Chunk chunk;
            chunk.model = services.models.Get(services.renderer, assets + data.model);
            if (!chunk.model)
            {
                std::cerr << "Level '" << d.name << "': chunk '" << data.name << "' failed to load\n";
                return nullptr;
            }
            if (!data.collision.empty() && !level->m_collision.Append(assets + data.collision))
            {
                std::cerr << "Level '" << d.name << "': chunk '" << data.name << "' has no collision\n";
                return nullptr;
            }
            chunk.layer = static_cast<Atom::RenderLayer>(data.layer);
            chunk.castsShadow = data.castsShadow;
            for (std::size_t c = 0; c < d.cells.size(); ++c)
            {
                if (d.cells[c].name == data.cell)
                {
                    chunk.cell = static_cast<int>(c);
                }
            }
            level->m_chunks.push_back(std::move(chunk));
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
            if (data.animation)
            {
                const Atom::Model* model = entity.renderable ? entity.renderable->model : nullptr;
                const int clip = model ? model->FindClip(data.animation->clip) : -1;
                if (clip < 0)
                {
                    std::cerr << "Level '" << d.name << "': entity '" << data.name
                              << "' has no animation '" << data.animation->clip << "'\n";
                    return nullptr;
                }
                Animated animated;
                animated.clip = clip;
                animated.duration = model->GetClip(clip)->duration;
                animated.speed = data.animation->speed;
                animated.loop = data.animation->loop;
                animated.playing = data.animation->autoplay;
                animated.soundsPerLoop = data.animation->soundsPerLoop;
                animated.soundOffset = data.animation->soundOffset;
                if (!data.animation->sound.empty())
                {
                    animated.sound = services.sounds.GetSound(data.animation->sound);
                }
                entity.animated = animated;
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
        // Shared through the cache: loaded once for every user.
        auto& slot = m_models[relativePath];
        if (!slot)
        {
            slot = services.models.Get(services.renderer, services.assetRoot + "Assets/" + relativePath);
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

    void Level::SubmitChunk(Atom::Renderer& renderer, const Atom::Model& model,
                            Atom::RenderLayer layer, bool castsShadow) const
    {
        renderer.BeginChunk(Atom::ChunkInfo{ model.GetBoundsMin(), model.GetBoundsMax(), layer, castsShadow });
        model.Submit(renderer, glm::mat4{ 1.0f });
        renderer.EndChunk();
    }

    void Level::Submit(Atom::Renderer& renderer, const glm::vec3& viewer) const
    {
        SubmitChunk(renderer, *m_scene, Atom::RenderLayer::Near, true);

        // Near chunks of far-away cells are skipped altogether; the layout
        // (bends, alley mouths) hides them. Mid and far always draw.
        // Halos (M23): a flickering one stutters like a failing tube -
        // mostly on, with short irregular drops.
        if (!m_data.halos.empty())
        {
            std::vector<Atom::Particle> halos;
            halos.reserve(m_data.halos.size());
            for (std::size_t i = 0; i < m_data.halos.size(); ++i)
            {
                const HaloData& h = m_data.halos[i];
                float intensity = h.intensity;
                if (h.flicker > 0.0f)
                {
                    const float t = m_time * 7.0f + static_cast<float>(i) * 13.1f;
                    const float wobble = std::sin(t) * std::sin(t * 2.3f + 1.0f) * std::sin(t * 0.37f);
                    intensity *= wobble > 0.55f ? 1.0f - h.flicker : 1.0f;
                }
                Atom::Particle p{};
                p.position = h.position;
                p.size = h.size;
                p.color = glm::vec4{ h.color * intensity, 1.0f };
                p.atlasCell = 0.0f; // the soft round puff
                halos.push_back(p);
            }
            renderer.SubmitHalos(halos);
        }

        const std::vector<bool> visibleCells = VisibleCells(m_data.cells, viewer);
        for (const Chunk& chunk : m_chunks)
        {
            if (chunk.cell >= 0 && !visibleCells[chunk.cell])
            {
                continue;
            }
            SubmitChunk(renderer, *chunk.model, chunk.layer, chunk.castsShadow);
        }

        m_world.ForEach([&](EntityId, const Entity& entity) {
            if (!entity.renderable || !entity.renderable->model)
            {
                return;
            }
            glm::mat4 transform = glm::translate(glm::mat4{ 1.0f }, entity.position);
            transform = glm::rotate(transform, entity.renderable->yaw, glm::vec3{ 0.0f, 1.0f, 0.0f });
            if (entity.animated)
            {
                entity.renderable->model->Submit(
                    renderer, transform, entity.animated->clip, entity.animated->time);
            }
            else
            {
                entity.renderable->model->Submit(renderer, transform);
            }
        });
    }

    void Level::Update(float deltaSeconds)
    {
        m_time += deltaSeconds;
        m_world.ForEach([&](EntityId, Entity& entity) {
            if (!entity.animated || !entity.animated->playing)
            {
                return;
            }
            Animated& a = entity.animated.value();
            const float previous = a.time;
            a.time += deltaSeconds * a.speed;

            if (!a.loop)
            {
                if (a.time >= a.duration)
                {
                    a.time = a.duration;
                    a.playing = false;
                }
                return;
            }

            // Sounds on the beat: at every 1/soundsPerLoop of the loop.
            if (a.sound && a.soundsPerLoop > 0 && a.duration > 0.0f)
            {
                const float beat = a.duration / static_cast<float>(a.soundsPerLoop);
                if (std::floor(a.time / beat) != std::floor(previous / beat))
                {
                    Atom::PlayParams params{};
                    params.spatial = true;
                    params.position = entity.position + a.soundOffset;
                    params.gain = 0.5f;
                    params.minDistance = 3.0f;
                    params.maxDistance = 40.0f;
                    m_audio.Play(a.sound, params);
                }
            }
            if (a.duration > 0.0f && a.time >= a.duration)
            {
                a.time = std::fmod(a.time, a.duration);
            }
        });
    }

    bool Level::PlayAnimation(const std::string& name, const std::string& clipName)
    {
        bool found = false;
        m_world.ForEach([&](EntityId, Entity& entity) {
            if (found || entity.name != name || !entity.renderable || !entity.renderable->model)
            {
                return;
            }
            const Atom::Model& model = *entity.renderable->model;
            const int clip = model.FindClip(clipName);
            if (clip < 0)
            {
                return;
            }
            found = true;
            Animated& a = entity.animated ? *entity.animated : entity.animated.emplace();
            const bool finishedOneShot = a.clip == clip && !a.loop && !a.playing && a.time >= a.duration;
            if (finishedOneShot || (a.clip == clip && a.playing))
            {
                return; // already open, or already moving
            }
            a.clip = clip;
            a.duration = model.GetClip(clip)->duration;
            a.time = 0.0f;
            a.loop = false;
            a.playing = true;
        });
        return found;
    }

    std::vector<Atom::Material*> Level::FindSceneMaterials(std::string_view name)
    {
        std::vector<Atom::Material*> found;
        if (Atom::Material* material = m_scene ? m_scene->FindMaterial(name) : nullptr)
        {
            found.push_back(material);
        }
        for (Chunk& chunk : m_chunks)
        {
            if (Atom::Material* material = chunk.model->FindMaterial(name))
            {
                found.push_back(material);
            }
        }
        return found;
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
