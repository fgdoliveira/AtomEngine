#include "Level/Level.h"

#include "Assets/Model.h"
#include "AudioScape.h"
#include "PlayerController.h"
#include "Renderer/Renderer.h"
#include "World/LiveEffects.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glm/geometric.hpp>

#include <algorithm>
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
        if (m_data.reverb.mix > 0.0f)
        {
            m_audio.SetReverb(0.0f, 1.0f, 0.0f); // the room stays behind
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

        // Impostors (M24): each set loaded once, instances share it.
        for (const ImpostorData& data : d.impostors)
        {
            auto& set = level->m_impostorSets[data.set];
            if (!set)
            {
                set = ImpostorSet::Load(services.renderer, assets + data.set);
                if (!set)
                {
                    std::cerr << "Level '" << d.name << "': impostor " << data.set << " failed to load\n";
                    return nullptr;
                }
            }
            level->m_impostors.push_back(Impostor{
                set.get(), data.position, glm::radians(data.yawDegrees),
                static_cast<Atom::RenderLayer>(data.layer) });
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
            if (!data.lightmap.empty())
            {
                // Per-chunk lightmaps (M25): a whole street at a useful
                // density doesn't fit one texture.
                auto lightmap = services.renderer.LoadTexture(assets + data.lightmap);
                if (!lightmap)
                {
                    std::cerr << "Level '" << d.name << "': chunk '" << data.name << "' is missing its lightmap "
                              << data.lightmap << '\n';
                    return nullptr;
                }
                chunk.model->SetLightmap(lightmap.get(), 1.0f);
                level->m_chunkLightmaps.push_back(std::move(lightmap));
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
            entity.hidden = data.hidden;

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

        // Live screens (M27): a render texture per screen material, running
        // its own attract loop; the material shows it as colour and glow.
        for (const ScreenData& data : d.screens)
        {
            Screen screen{ services.renderer.CreateRenderTexture(PachinkoAttract::Width, PachinkoAttract::Height),
                           PachinkoAttract(data.seed), FixedStep{}, data.material };
            const std::vector<Atom::Material*> materials = level->FindSceneMaterials(data.material);
            if (!screen.target || materials.empty())
            {
                std::cerr << "Level '" << d.name << "': no screen material '" << data.material << "'\n";
                return nullptr;
            }
            for (Atom::Material* material : materials)
            {
                material->baseColorTexture = &screen.target->GetTexture();
                material->emissiveTexture = &screen.target->GetTexture();
            }
            level->m_screens.push_back(std::move(screen));
        }
        if (d.reverb.mix > 0.0f)
        {
            services.audio.SetReverb(d.reverb.mix, d.reverb.size, d.reverb.feedback);
        }

        // Cell ambience (M25): every zone's beds play from the start, silent
        // until the player walks into the zone's cell.
        for (const AudioZone& zone : d.audioZones)
        {
            const int cell = static_cast<int>(std::find_if(d.cells.begin(), d.cells.end(),
                [&](const CellData& c) { return c.name == zone.cell; }) - d.cells.begin());
            for (const AudioBed& bed : zone.beds)
            {
                Atom::PlayParams params{};
                params.loop = true;
                params.gain = 0.0f;
                const Atom::VoiceId id = services.audio.Play(services.sounds.GetSound(bed.sound), params);
                level->m_voices.push_back({ id, {}, 0.0f });
                level->m_zoneVoices.push_back({ id, cell, bed.gain, 0.0f });
            }
        }

        // Movers, and what rides on entities (M25).
        for (const EntityData& data : d.entities)
        {
            if (!data.mover)
            {
                continue;
            }
            Mover mover{ *level->FindEntity(data.name), data.position, *data.mover };
            if (!data.mover->sound.empty())
            {
                Atom::PlayParams params{};
                params.loop = true;
                params.gain = 0.0f;
                params.spatial = true;
                params.position = data.position;
                params.minDistance = data.mover->minDistance;
                params.maxDistance = data.mover->maxDistance;
                mover.voice = services.audio.Play(services.sounds.GetSound(data.mover->sound), params);
                level->m_voices.push_back({ mover.voice, {}, 0.0f });
            }
            level->m_movers.push_back(std::move(mover));
        }
        for (const HaloData& halo : d.halos)
        {
            level->m_haloAnchors.push_back(halo.entity.empty() ? std::nullopt : level->FindEntity(halo.entity));
        }
        for (const LiveLightData& data : d.lights)
        {
            LiveLight light{ data.entity.empty() ? std::nullopt : level->FindEntity(data.entity), {} };
            if (!data.material.empty())
            {
                for (Atom::Material* material : level->FindSceneMaterials(data.material))
                {
                    light.materials.emplace_back(material, material->emissiveFactor);
                }
                if (light.materials.empty())
                {
                    std::cerr << "Level '" << d.name << "': no material '" << data.material << "' to flicker\n";
                    return nullptr;
                }
            }
            level->m_lights.push_back(std::move(light));
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
                if (IsHidden(m_haloAnchors[i]))
                {
                    continue;
                }
                const float intensity = h.intensity * FlickerFactor(m_time, h.position, h.flicker);
                Atom::Particle p{};
                p.position = Anchor(m_haloAnchors[i], h.position);
                p.size = h.size;
                p.color = glm::vec4{ h.color * intensity, 1.0f };
                p.atlasCell = 0.0f; // the soft round puff
                halos.push_back(p);
            }
            renderer.SubmitHalos(halos);
        }

        // Live lights (M25): the few lights computed per pixel.
        for (std::size_t i = 0; i < m_data.lights.size(); ++i)
        {
            const LiveLightData& l = m_data.lights[i];
            if (IsHidden(m_lights[i].anchor))
            {
                continue;
            }
            renderer.SubmitLiveLight(Atom::LiveLight{
                Anchor(m_lights[i].anchor, l.position), l.radius,
                l.color * l.intensity * FlickerFactor(m_time, l.position, l.flicker) });
        }

        // Impostors (M24): the card turns to face the viewer; the view shown
        // is the one rendered from closest to that direction.
        constexpr float Hysteresis = 0.13f; // radians, ~7.5 degrees
        for (const Impostor& impostor : m_impostors)
        {
            const ImpostorDescriptor& d = impostor.set->GetDescriptor();
            const glm::vec3 toViewer = viewer - impostor.position;
            const float facing = std::atan2(toViewer.x, toViewer.z); // card normal +Z toward the viewer
            impostor.view = SelectImpostorView(impostor.view, facing - impostor.yaw, d.views, Hysteresis);

            const glm::vec3 half{ d.width * 0.5f, 0.0f, d.width * 0.5f };
            renderer.BeginChunk(Atom::ChunkInfo{
                impostor.position - half,
                impostor.position + half + glm::vec3{ 0.0f, d.height, 0.0f },
                impostor.layer, false });
            const glm::mat4 transform = glm::rotate(
                glm::translate(glm::mat4{ 1.0f }, impostor.position), facing, glm::vec3{ 0.0f, 1.0f, 0.0f });
            renderer.Submit(impostor.set->GetView(impostor.view), impostor.set->GetMaterial(), transform);
            renderer.EndChunk();
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
            if (!entity.renderable || !entity.renderable->model || entity.hidden)
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

    void Level::Update(float deltaSeconds, const glm::vec3& listener)
    {
        m_time += deltaSeconds;

        // Movers (M25): the entity shuttles along its line; its sound
        // follows it and is heard only while it moves.
        for (const Mover& mover : m_movers)
        {
            const MoverState state = EvaluateMover(
                mover.data.travelSeconds, mover.data.waitSeconds, m_time + mover.data.startSeconds);
            Entity* entity = m_world.Find(mover.entity);
            if (!entity)
            {
                continue;
            }
            entity->position = mover.start + mover.data.travel * state.progress;
            if (mover.voice)
            {
                m_audio.SetVoicePosition(mover.voice, entity->position);
                m_audio.SetVoiceGain(mover.voice, state.moving ? mover.data.gain : 0.0f);
            }
        }

        // Screens (M27): the attract loops tick at a fixed step and draw
        // this frame's picture into their render texture.
        for (Screen& screen : m_screens)
        {
            if (screen.takenOver)
            {
                continue; // the game draws it
            }
            for (int steps = screen.clock.Advance(deltaSeconds); steps > 0; --steps)
            {
                screen.attract.Step();
            }
            screen.attract.Draw(screen.target->GetCanvas());
        }

        // Sequence sounds follow their entity.
        for (const FollowingVoice& voice : m_followingVoices)
        {
            if (const Entity* entity = m_world.Find(voice.entity))
            {
                m_audio.SetVoicePosition(voice.id, entity->position);
            }
        }

        // A flickering light's material stutters with it.
        for (std::size_t i = 0; i < m_lights.size(); ++i)
        {
            const LiveLightData& l = m_data.lights[i];
            const float factor = FlickerFactor(m_time, l.position, l.flicker);
            for (auto& [material, base] : m_lights[i].materials)
            {
                material->emissiveFactor = base * factor;
            }
        }

        // Cell ambience (M25): the zone of the listener's cell fades in,
        // the others out.
        const int cell = CellAt(m_data.cells, listener);
        for (ZoneVoice& voice : m_zoneVoices)
        {
            const float target = voice.cell == cell ? 1.0f : 0.0f;
            voice.level = StepTowards(voice.level, target, deltaSeconds, m_data.zoneFadeSeconds);
            m_audio.SetVoiceGain(voice.id, voice.gain * voice.level);
        }

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

    std::optional<EntityId> Level::FindEntity(const std::string& name) const
    {
        std::optional<EntityId> found;
        m_world.ForEach([&](EntityId id, const Entity& entity) {
            if (!found && entity.name == name)
            {
                found = id;
            }
        });
        return found;
    }

    bool Level::IsHidden(const std::optional<EntityId>& entity) const
    {
        const Entity* anchor = entity ? m_world.Find(*entity) : nullptr;
        return anchor && anchor->hidden;
    }

    bool Level::SetEntityVisible(const std::string& name, bool visible)
    {
        const auto id = FindEntity(name);
        Entity* entity = id ? m_world.Find(*id) : nullptr;
        if (entity)
        {
            entity->hidden = !visible;
        }
        return entity != nullptr;
    }

    std::optional<glm::vec3> Level::GetEntityPosition(const std::string& name) const
    {
        const auto id = FindEntity(name);
        const Entity* entity = id ? m_world.Find(*id) : nullptr;
        return entity ? std::optional<glm::vec3>(entity->position) : std::nullopt;
    }

    bool Level::SetEntityPosition(const std::string& name, const glm::vec3& position)
    {
        const auto id = FindEntity(name);
        Entity* entity = id ? m_world.Find(*id) : nullptr;
        if (entity)
        {
            entity->position = position;
        }
        return entity != nullptr;
    }

    void Level::PlaySound(const Atom::SoundHandle& sound, const std::string& entityName, float gain, bool loop)
    {
        Atom::PlayParams params{};
        params.gain = gain;
        params.loop = loop;
        const auto id = entityName.empty() ? std::nullopt : FindEntity(entityName);
        if (id)
        {
            params.spatial = true;
            params.position = m_world.Find(*id)->position;
            params.minDistance = 4.0f;
            params.maxDistance = 90.0f;
        }
        const Atom::VoiceId voice = m_audio.Play(sound, params);
        if (loop)
        {
            m_voices.push_back({ voice, {}, gain }); // stopped with the level
        }
        if (id)
        {
            m_followingVoices.push_back({ voice, *id });
        }
    }

    glm::vec3 Level::Anchor(const std::optional<EntityId>& entity, const glm::vec3& offset) const
    {
        if (!entity)
        {
            return offset;
        }
        const Entity* anchor = m_world.Find(*entity);
        return anchor ? anchor->position + offset : offset;
    }

    Atom::RenderTexture* Level::TakeOverScreen(std::string_view material)
    {
        for (Screen& screen : m_screens)
        {
            if (screen.material == material)
            {
                screen.takenOver = true;
                return screen.target.get();
            }
        }
        return nullptr;
    }

    void Level::ReleaseScreen(std::string_view material)
    {
        for (Screen& screen : m_screens)
        {
            if (screen.material == material)
            {
                screen.takenOver = false;
            }
        }
    }

    float Level::GetZoneLevel(std::string_view cellName) const
    {
        float level = 0.0f;
        for (const ZoneVoice& voice : m_zoneVoices)
        {
            if (voice.cell >= 0 && m_data.cells[voice.cell].name == cellName)
            {
                level = std::max(level, voice.level);
            }
        }
        return level;
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
