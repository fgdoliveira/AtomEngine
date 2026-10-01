#include "Level/LevelData.h"

#include "Level/JsonText.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace AtomGame
{
    namespace
    {
        using Json = nlohmann::json;

        // Parse errors are reported as exceptions internally and turned into
        // a LevelParseResult at the boundary, so the helpers stay short.
        // Every message starts with the JSON Pointer of the offending value
        // ("/entities/3/interactable/action/type: ...").
        struct LevelError : std::runtime_error
        {
            LevelError(const std::string& path, const std::string& message)
                : std::runtime_error((path.empty() ? std::string("/") : path) + ": " + message) {}
        };

        // Typed getters: a missing key gives the fallback, a key of the wrong
        // type is an error (it used to fall back silently).
        std::string String(const Json& object, const char* key, const std::string& path, std::string fallback = {})
        {
            const auto found = object.find(key);
            if (found == object.end())
            {
                return fallback;
            }
            if (!found->is_string())
            {
                throw LevelError(JsonPath(path, key), "must be a string");
            }
            return found->get<std::string>();
        }

        float Number(const Json& object, const char* key, const std::string& path, float fallback)
        {
            const auto found = object.find(key);
            if (found == object.end())
            {
                return fallback;
            }
            if (!found->is_number())
            {
                throw LevelError(JsonPath(path, key), "must be a number");
            }
            return found->get<float>();
        }

        bool Bool(const Json& object, const char* key, const std::string& path, bool fallback)
        {
            const auto found = object.find(key);
            if (found == object.end())
            {
                return fallback;
            }
            if (!found->is_boolean())
            {
                throw LevelError(JsonPath(path, key), "must be true or false");
            }
            return found->get<bool>();
        }

        glm::vec3 Vec3(const Json& value, const std::string& path)
        {
            if (!value.is_array() || value.size() != 3
                || !value[0].is_number() || !value[1].is_number() || !value[2].is_number())
            {
                throw LevelError(path, "must be [x, y, z]");
            }
            return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
        }

        glm::vec3 Vec3(const Json& object, const char* key, glm::vec3 fallback, const std::string& path)
        {
            const auto found = object.find(key);
            return found != object.end() ? Vec3(*found, JsonPath(path, key)) : fallback;
        }

        const Json& Array(const Json& object, const char* key, const std::string& path)
        {
            static const Json empty = Json::array();
            const auto found = object.find(key);
            if (found == object.end())
            {
                return empty;
            }
            if (!found->is_array())
            {
                throw LevelError(JsonPath(path, key), "must be an array");
            }
            return *found;
        }

        Action ParseAction(const Json& json, const std::string& path)
        {
            const std::string type = String(json, "type", path);
            if (type == "message")
            {
                return ShowMessage{ String(json, "text", path) };
            }
            if (type == "setFlag")
            {
                const std::string flag = String(json, "flag", path);
                if (flag.empty())
                {
                    throw LevelError(path, "setFlag needs \"flag\"");
                }
                return SetFlag{ flag, String(json, "message", path) };
            }
            if (type == "dialogue")
            {
                return StartDialogue{ String(json, "id", path) };
            }
            if (type == "changeLevel")
            {
                const std::string level = String(json, "level", path);
                if (level.empty())
                {
                    throw LevelError(path, "changeLevel needs \"level\"");
                }
                return ChangeLevel{ level, String(json, "spawn", path) };
            }
            if (type == "playAnimation")
            {
                PlayAnimation play{ String(json, "entity", path), String(json, "clip", path), String(json, "message", path) };
                if (play.entity.empty() || play.clip.empty())
                {
                    throw LevelError(path, "playAnimation needs \"entity\" and \"clip\"");
                }
                return play;
            }
            if (type == "addCounter")
            {
                AddCounter add{ String(json, "counter", path), static_cast<int>(Number(json, "amount", path, 0.0f)),
                                String(json, "message", path), String(json, "once", path), String(json, "again", path) };
                if (add.counter.empty())
                {
                    throw LevelError(path, "addCounter needs \"counter\"");
                }
                return add;
            }
            if (type == "exchange")
            {
                Exchange exchange{ String(json, "counter", path), static_cast<int>(Number(json, "cost", path, 0.0f)),
                                   String(json, "flag", path), String(json, "message", path), String(json, "short", path) };
                if (exchange.counter.empty() || exchange.flag.empty() || exchange.cost <= 0)
                {
                    throw LevelError(path, "exchange needs \"counter\", a positive \"cost\" and \"flag\"");
                }
                return exchange;
            }
            if (type == "playMachine")
            {
                PlayMachine play;
                play.screen = String(json, "screen", path);
                play.machine = String(json, "machine", path);
                const auto view = json.find("view");
                if (play.screen.empty() || play.machine.empty() || view == json.end())
                {
                    throw LevelError(path, "playMachine needs \"screen\", \"machine\" and \"view\"");
                }
                const std::string at = JsonPath(path, "view");
                if (!view->contains("position"))
                {
                    throw LevelError(at, "view needs a \"position\"");
                }
                play.viewPosition = Vec3(*view, "position", play.viewPosition, at);
                play.viewYawDegrees = Number(*view, "yaw", at, 0.0f);
                play.viewPitchDegrees = Number(*view, "pitch", at, 0.0f);
                return play;
            }
            if (type == "sequence")
            {
                const std::string id = String(json, "id", path);
                if (id.empty())
                {
                    throw LevelError(path, "sequence needs \"id\"");
                }
                return RunSequence{ id };
            }
            throw LevelError(JsonPath(path, "type"), "unknown action type \"" + type + "\"");
        }

        EntityData ParseEntity(const Json& json, const std::string& path)
        {
            EntityData entity;
            entity.name = String(json, "name", path);
            if (entity.name.empty())
            {
                throw LevelError(path, "entity has no \"name\"");
            }
            entity.hasPosition = json.contains("position");
            entity.hasYaw = json.contains("yaw");
            entity.position = Vec3(json, "position", glm::vec3{ 0.0f }, path);
            entity.yawDegrees = Number(json, "yaw", path, 0.0f);
            entity.scale = Number(json, "scale", path, 1.0f);
            if (entity.scale <= 0.0f)
            {
                throw LevelError(JsonPath(path, "scale"), "must be positive");
            }
            entity.model = String(json, "model", path);
            entity.hidden = Bool(json, "hidden", path, false);

            if (const auto collider = json.find("collider"); collider != json.end())
            {
                const std::string at = JsonPath(path, "collider");
                entity.collider = ColliderBox{
                    Vec3(*collider, "center", glm::vec3{ 0.0f }, at),
                    Vec3(*collider, "halfExtents", glm::vec3{ 0.5f }, at),
                };
            }

            if (const auto use = json.find("interactable"); use != json.end())
            {
                const std::string at = JsonPath(path, "interactable");
                const auto action = use->find("action");
                if (action == use->end())
                {
                    throw LevelError(at, "interactable needs an \"action\"");
                }
                Interactable interactable{
                    String(*use, "prompt", at, "Use"),
                    ParseAction(*action, JsonPath(at, "action")),
                };
                interactable.focusOffset = Vec3(*use, "focus", glm::vec3{ 0.0f, 1.2f, 0.0f }, at);
                interactable.radius = Number(*use, "radius", at, 2.2f);
                interactable.requiresFlag = String(*use, "requires", at);
                if (const auto locked = use->find("locked"); locked != use->end())
                {
                    interactable.lockedAction = ParseAction(*locked, JsonPath(at, "locked"));
                }
                entity.interactable = std::move(interactable);
            }

            if (const auto animation = json.find("animation"); animation != json.end())
            {
                const std::string at = JsonPath(path, "animation");
                EntityAnimation a;
                a.clip = String(*animation, "clip", at);
                a.loop = Bool(*animation, "loop", at, true);
                a.autoplay = Bool(*animation, "autoplay", at, true);
                a.speed = Number(*animation, "speed", at, 1.0f);
                a.sound = String(*animation, "sound", at);
                a.soundsPerLoop = static_cast<int>(Number(*animation, "soundsPerLoop", at, 1.0f));
                a.soundOffset = Vec3(*animation, "soundOffset", a.soundOffset, at);
                if (a.clip.empty() || entity.model.empty())
                {
                    throw LevelError(at, "animation needs a \"clip\" and the entity a \"model\"");
                }
                entity.animation = a;
            }

            if (const auto mover = json.find("mover"); mover != json.end())
            {
                const std::string at = JsonPath(path, "mover");
                EntityMover m;
                m.travel = Vec3(*mover, "travel", m.travel, at);
                m.travelSeconds = Number(*mover, "travelSeconds", at, m.travelSeconds);
                m.waitSeconds = Number(*mover, "waitSeconds", at, m.waitSeconds);
                m.startSeconds = Number(*mover, "startSeconds", at, m.startSeconds);
                m.sound = String(*mover, "sound", at);
                m.gain = Number(*mover, "gain", at, m.gain);
                m.minDistance = Number(*mover, "minDistance", at, m.minDistance);
                m.maxDistance = Number(*mover, "maxDistance", at, m.maxDistance);
                if (!mover->contains("travel") || m.travelSeconds <= 0.0f || m.waitSeconds < 0.0f)
                {
                    throw LevelError(at, "a mover needs \"travel\", \"travelSeconds\" > 0 and \"waitSeconds\" >= 0");
                }
                entity.mover = m;
            }
            return entity;
        }

        SequenceStep ParseStep(const Json& json, const std::string& path)
        {
            using Type = SequenceStep::Type;
            static const std::map<std::string, Type> types{
                { "wait", Type::Wait }, { "message", Type::Message }, { "setFlag", Type::SetFlag },
                { "show", Type::Show }, { "hide", Type::Hide }, { "playSound", Type::PlaySound },
                { "playAnimation", Type::PlayAnimation }, { "moveEntity", Type::MoveEntity },
                { "changeLevel", Type::ChangeLevel },
            };
            const std::string name = String(json, "type", path);
            const auto found = types.find(name);
            if (found == types.end())
            {
                throw LevelError(JsonPath(path, "type"), "unknown step type \"" + name + "\"");
            }
            SequenceStep step;
            step.type = found->second;
            step.seconds = Number(json, "seconds", path, 0.0f);
            step.entity = String(json, "entity", path);
            step.gain = Number(json, "gain", path, 1.0f);
            step.loop = Bool(json, "loop", path, false);
            step.to = Vec3(json, "to", step.to, path);
            const auto require = [&](bool ok, const char* what) {
                if (!ok)
                {
                    throw LevelError(path, name + " needs " + what);
                }
            };
            switch (step.type)
            {
            case Type::Wait:
                require(step.seconds > 0.0f, "\"seconds\" > 0");
                break;
            case Type::Message:
                step.text = String(json, "text", path);
                require(!step.text.empty(), "\"text\"");
                break;
            case Type::SetFlag:
                step.text = String(json, "flag", path);
                require(!step.text.empty(), "\"flag\"");
                break;
            case Type::Show:
            case Type::Hide:
                require(!step.entity.empty(), "\"entity\"");
                break;
            case Type::PlaySound:
                step.text = String(json, "sound", path);
                require(!step.text.empty(), "\"sound\"");
                break;
            case Type::PlayAnimation:
                step.clip = String(json, "clip", path);
                require(!step.entity.empty() && !step.clip.empty(), "\"entity\" and \"clip\"");
                break;
            case Type::MoveEntity:
                require(!step.entity.empty() && json.contains("to") && step.seconds > 0.0f,
                        "\"entity\", \"to\" and \"seconds\" > 0");
                break;
            case Type::ChangeLevel:
                step.text = String(json, "level", path);
                step.clip = String(json, "spawn", path);
                require(!step.text.empty(), "\"level\"");
                break;
            }
            return step;
        }

        // Placement from Blender markers (<level>.markers.json, written by
        // the asset build): positions and yaw of spawns and entities.
        struct Placement
        {
            glm::vec3 position{ 0.0f };
            float yawDegrees = 0.0f;
        };

        std::map<std::string, Placement> ParsePlacements(const Json& markers, const char* key)
        {
            std::map<std::string, Placement> placements;
            const std::string path = JsonPath("", key);
            const auto found = markers.find(key);
            if (found == markers.end())
            {
                return placements;
            }
            if (!found->is_object())
            {
                throw LevelError("(markers)" + path, "must be an object");
            }
            for (const auto& [name, value] : found->items())
            {
                const std::string at = "(markers)" + JsonPath(path, name);
                placements[name] = Placement{ Vec3(value, "position", glm::vec3{ 0.0f }, at),
                                              Number(value, "yaw", at, 0.0f) };
            }
            return placements;
        }

        LevelData Parse(const Json& root, const Json* markers)
        {
            if (!root.is_object())
            {
                throw LevelError("", "the file must hold a JSON object");
            }

            LevelData level;
            level.name = String(root, "name", "");
            level.model = String(root, "model", "");
            level.collision = String(root, "collision", "");
            if (const auto lightmap = root.find("lightmap"); lightmap != root.end())
            {
                LevelLightmap map{ String(*lightmap, "texture", "/lightmap"), Number(*lightmap, "intensity", "/lightmap", 1.0f) };
                if (map.texture.empty() || map.intensity < 0.0f)
                {
                    throw LevelError("/lightmap", "lightmap needs \"texture\" and a non-negative \"intensity\"");
                }
                level.lightmap = map;
            }
            if (level.name.empty() || level.model.empty() || level.collision.empty())
            {
                throw LevelError("", "needs \"name\", \"model\" and \"collision\"");
            }

            // Spawns; a spawn may leave its position to a marker.
            std::set<std::string> unplacedSpawns;
            std::set<std::string> spawnsWithYaw;
            const auto spawns = root.find("spawns");
            if (spawns != root.end() && !spawns->is_object())
            {
                throw LevelError("/spawns", "must be an object");
            }
            if (spawns != root.end())
            {
                for (const auto& [name, spawn] : spawns->items())
                {
                    const std::string at = JsonPath("/spawns", name);
                    if (!spawn.contains("position"))
                    {
                        unplacedSpawns.insert(name);
                    }
                    if (spawn.contains("yaw"))
                    {
                        spawnsWithYaw.insert(name);
                    }
                    level.spawns[name] = SpawnPoint{
                        Vec3(spawn, "position", glm::vec3{ 0.0f }, at),
                        Number(spawn, "yaw", at, 0.0f),
                    };
                }
            }

            if (const auto light = root.find("lighting"); light != root.end())
            {
                const std::string at = "/lighting";
                LevelLighting& l = level.lighting;
                l.sunDirection = Vec3(*light, "sunDirection", l.sunDirection, at);
                l.sunColor = Vec3(*light, "sunColor", l.sunColor, at);
                l.skyColor = Vec3(*light, "skyColor", l.skyColor, at);
                l.groundColor = Vec3(*light, "groundColor", l.groundColor, at);
                l.fogColor = Vec3(*light, "fogColor", l.fogColor, at);
                l.shadows = Bool(*light, "shadows", at, l.shadows);
                l.bakedLight = Number(*light, "bakedLight", at, l.bakedLight);
                if (const auto glow = light->find("glow"); glow != light->end())
                {
                    l.glowStrength = Number(*glow, "strength", "/lighting/glow", l.glowStrength);
                    l.glowThreshold = Number(*glow, "threshold", "/lighting/glow", l.glowThreshold);
                    if (l.glowStrength < 0.0f || l.glowThreshold <= 0.0f)
                    {
                        throw LevelError("/lighting/glow", "strength must be >= 0 and threshold > 0");
                    }
                }
                if (l.bakedLight < 0.0f || l.bakedLight > 1.0f)
                {
                    throw LevelError("/lighting/bakedLight", "must be between 0 and 1");
                }
            }

            if (const auto audio = root.find("audio"); audio != root.end())
            {
                std::size_t index = 0;
                for (const Json& bed : Array(*audio, "beds", "/audio"))
                {
                    const std::string at = JsonPath("/audio/beds", index++);
                    level.beds.push_back(AudioBed{ String(bed, "sound", at), Number(bed, "gain", at, 1.0f) });
                }
                index = 0;
                for (const Json& zone : Array(*audio, "zones", "/audio"))
                {
                    const std::string at = JsonPath("/audio/zones", index++);
                    AudioZone z{ String(zone, "cell", at), {} };
                    std::size_t bedIndex = 0;
                    for (const Json& bed : Array(zone, "beds", at))
                    {
                        const std::string bedAt = JsonPath(JsonPath(at, "beds"), bedIndex++);
                        z.beds.push_back(AudioBed{ String(bed, "sound", bedAt), Number(bed, "gain", bedAt, 1.0f) });
                    }
                    if (z.cell.empty())
                    {
                        throw LevelError(at, "a zone needs a \"cell\"");
                    }
                    level.audioZones.push_back(std::move(z));
                }
                level.zoneFadeSeconds = Number(*audio, "zoneFadeSeconds", "/audio", level.zoneFadeSeconds);
                if (const auto reverb = audio->find("reverb"); reverb != audio->end())
                {
                    const std::string at = "/audio/reverb";
                    LevelReverb& r = level.reverb;
                    r.mix = Number(*reverb, "mix", at, r.mix);
                    r.size = Number(*reverb, "size", at, r.size);
                    r.feedback = Number(*reverb, "feedback", at, r.feedback);
                    if (r.mix < 0.0f || r.mix > 1.0f || r.size <= 0.0f || r.feedback < 0.0f || r.feedback >= 1.0f)
                    {
                        throw LevelError(at, "mix 0..1, size > 0, feedback 0..<1");
                    }
                }
                index = 0;
                for (const Json& emitter : Array(*audio, "emitters", "/audio"))
                {
                    const std::string at = JsonPath("/audio/emitters", index++);
                    level.emitters.push_back(AudioEmitter{
                        String(emitter, "sound", at),
                        String(emitter, "group", at),
                        Vec3(emitter, "position", glm::vec3{ 0.0f }, at),
                        Number(emitter, "gain", at, 1.0f),
                        Number(emitter, "minDistance", at, 1.0f),
                        Number(emitter, "maxDistance", at, 20.0f),
                    });
                }
            }

            if (const auto surfaces = root.find("surfaces"); surfaces != root.end())
            {
                // A misspelt surface would silently sound like the default.
                const auto known = [](const std::string& surface, const std::string& at) {
                    if (std::find(FootstepSurfaces.begin(), FootstepSurfaces.end(), surface) == FootstepSurfaces.end())
                    {
                        throw LevelError(at, "unknown footstep surface \"" + surface + "\"");
                    }
                    return surface;
                };
                level.defaultSurface = known(String(*surfaces, "default", "/surfaces", level.defaultSurface),
                                             "/surfaces/default");
                std::size_t index = 0;
                for (const Json& zone : Array(*surfaces, "zones", "/surfaces"))
                {
                    const std::string at = JsonPath("/surfaces/zones", index++);
                    const glm::vec3 min = Vec3(zone, "min", glm::vec3{ 0.0f }, at);
                    const glm::vec3 max = Vec3(zone, "max", glm::vec3{ 0.0f }, at);
                    level.surfaces.push_back(SurfaceZone{
                        { min.x, min.z }, { max.x, max.z },
                        known(String(zone, "surface", at, level.defaultSurface), JsonPath(at, "surface")) });
                }
            }

            level.outdoor = Bool(root, "outdoor", "", true);

            if (const auto particles = root.find("particles"); particles != root.end())
            {
                level.leaves = Bool(*particles, "leaves", "/particles", true);
                level.fogBanks = Bool(*particles, "fogBanks", "/particles", true);
            }

            if (const auto unease = root.find("unease"); unease != root.end())
            {
                level.unease.figure = Bool(*unease, "figure", "/unease", false);
                std::size_t index = 0;
                for (const Json& spot : Array(*unease, "figureSpots", "/unease"))
                {
                    level.unease.figureSpots.push_back(Vec3(spot, JsonPath("/unease/figureSpots", index++)));
                }
                level.unease.flickerMaterial = String(*unease, "flickerMaterial", "/unease");
                index = 0;
                for (const Json& site : Array(*unease, "flickerSites", "/unease"))
                {
                    level.unease.flickerSites.push_back(Vec3(site, JsonPath("/unease/flickerSites", index++)));
                }
            }

            if (const auto lab = root.find("lab"); lab != root.end())
            {
                LevelLab l;
                l.subject = String(*lab, "subject", "/lab");
                if (l.subject.empty())
                {
                    throw LevelError("/lab", "lab needs a \"subject\" entity");
                }
                l.target = Vec3(*lab, "target", l.target, "/lab");
                l.distance = Number(*lab, "distance", "/lab", l.distance);
                l.yawDegrees = Number(*lab, "yaw", "/lab", l.yawDegrees);
                l.pitchDegrees = Number(*lab, "pitch", "/lab", l.pitchDegrees);
                level.lab = l;
            }

            if (const auto sky = root.find("sky"); sky != root.end())
            {
                LevelSky s{ String(*sky, "panorama", "/sky"), Number(*sky, "intensity", "/sky", 1.0f) };
                if (s.panorama.empty())
                {
                    throw LevelError("/sky", "sky needs a \"panorama\"");
                }
                level.sky = s;
            }
            std::size_t haloIndex = 0;
            for (const Json& halo : Array(root, "halos", ""))
            {
                const std::string at = JsonPath("/halos", haloIndex++);
                HaloData h;
                h.position = Vec3(halo, "position", h.position, at);
                h.size = Number(halo, "size", at, h.size);
                h.color = Vec3(halo, "color", h.color, at);
                h.intensity = Number(halo, "intensity", at, h.intensity);
                h.flicker = Number(halo, "flicker", at, h.flicker);
                h.entity = String(halo, "entity", at);
                if (!halo.contains("position"))
                {
                    throw LevelError(at, "a halo needs a \"position\"");
                }
                level.halos.push_back(h);
            }

            std::size_t screenIndex = 0;
            for (const Json& screen : Array(root, "screens", ""))
            {
                const std::string at = JsonPath("/screens", screenIndex++);
                ScreenData s{ String(screen, "material", at),
                              static_cast<std::uint32_t>(Number(screen, "seed", at, 1.0f)),
                              String(screen, "machine", at) };
                if (s.material.empty())
                {
                    throw LevelError(at, "a screen needs a \"material\"");
                }
                level.screens.push_back(s);
            }

            std::size_t lightIndex = 0;
            for (const Json& light : Array(root, "lights", ""))
            {
                const std::string at = JsonPath("/lights", lightIndex++);
                LiveLightData l;
                l.position = Vec3(light, "position", l.position, at);
                l.color = Vec3(light, "color", l.color, at);
                l.intensity = Number(light, "intensity", at, l.intensity);
                l.radius = Number(light, "radius", at, l.radius);
                l.flicker = Number(light, "flicker", at, l.flicker);
                l.entity = String(light, "entity", at);
                l.material = String(light, "material", at);
                if (!light.contains("position") || l.radius <= 0.0f)
                {
                    throw LevelError(at, "a light needs a \"position\" and a positive \"radius\"");
                }
                level.lights.push_back(l);
            }

            std::size_t impostorIndex = 0;
            for (const Json& impostor : Array(root, "impostors", ""))
            {
                const std::string at = JsonPath("/impostors", impostorIndex++);
                ImpostorData d;
                d.set = String(impostor, "set", at);
                d.position = Vec3(impostor, "position", d.position, at);
                d.yawDegrees = Number(impostor, "yaw", at, 0.0f);
                const std::string layer = String(impostor, "layer", at, "far");
                if (layer == "mid") d.layer = ChunkLayer::Mid;
                else if (layer == "far") d.layer = ChunkLayer::Far;
                else throw LevelError(JsonPath(at, "layer"), "must be \"mid\" or \"far\"");
                if (d.set.empty() || !impostor.contains("position"))
                {
                    throw LevelError(at, "an impostor needs \"set\" and \"position\"");
                }
                level.impostors.push_back(d);
            }

            std::size_t chunkIndex = 0;
            for (const Json& chunk : Array(root, "chunks", ""))
            {
                const std::string at = JsonPath("/chunks", chunkIndex++);
                ChunkData c;
                c.name = String(chunk, "name", at);
                c.model = String(chunk, "model", at);
                c.collision = String(chunk, "collision", at);
                const std::string layer = String(chunk, "layer", at, "near");
                if (layer == "near") c.layer = ChunkLayer::Near;
                else if (layer == "mid") c.layer = ChunkLayer::Mid;
                else if (layer == "far") c.layer = ChunkLayer::Far;
                else throw LevelError(JsonPath(at, "layer"), "must be \"near\", \"mid\" or \"far\"");
                // Only near chunks cast shadows unless told otherwise:
                // distant shells in the shadow pass cost and show nothing.
                c.castsShadow = Bool(chunk, "castsShadow", at, c.layer == ChunkLayer::Near);
                c.cell = String(chunk, "cell", at);
                c.lightmap = String(chunk, "lightmap", at);
                if (c.name.empty() || c.model.empty())
                {
                    throw LevelError(at, "a chunk needs \"name\" and \"model\"");
                }
                if (!c.cell.empty() && c.layer != ChunkLayer::Near)
                {
                    throw LevelError(JsonPath(at, "cell"), "only near chunks belong to cells");
                }
                level.chunks.push_back(std::move(c));
            }

            std::size_t cellIndex = 0;
            for (const Json& cell : Array(root, "cells", ""))
            {
                const std::string at = JsonPath("/cells", cellIndex++);
                CellData c;
                c.name = String(cell, "name", at);
                const glm::vec3 min = Vec3(cell, "min", glm::vec3{ 0.0f }, at);
                const glm::vec3 max = Vec3(cell, "max", glm::vec3{ 0.0f }, at);
                c.min = { min.x, min.z };
                c.max = { max.x, max.z };
                std::size_t n = 0;
                for (const Json& neighbour : Array(cell, "neighbours", at))
                {
                    if (!neighbour.is_string())
                    {
                        throw LevelError(JsonPath(JsonPath(at, "neighbours"), n), "must be a cell name");
                    }
                    c.neighbours.push_back(neighbour.get<std::string>());
                    ++n;
                }
                if (c.name.empty())
                {
                    throw LevelError(at, "a cell needs a \"name\"");
                }
                level.cells.push_back(std::move(c));
            }
            const auto cellExists = [&](const std::string& name) {
                return std::any_of(level.cells.begin(), level.cells.end(),
                    [&](const CellData& c) { return c.name == name; });
            };
            for (std::size_t i = 0; i < level.cells.size(); ++i)
            {
                for (std::size_t n = 0; n < level.cells[i].neighbours.size(); ++n)
                {
                    if (!cellExists(level.cells[i].neighbours[n]))
                    {
                        throw LevelError(JsonPath(JsonPath(JsonPath("/cells", i), "neighbours"), n),
                            "no cell named \"" + level.cells[i].neighbours[n] + "\"");
                    }
                }
            }
            for (std::size_t i = 0; i < level.chunks.size(); ++i)
            {
                if (!level.chunks[i].cell.empty() && !cellExists(level.chunks[i].cell))
                {
                    throw LevelError(JsonPath(JsonPath("/chunks", i), "cell"),
                        "no cell named \"" + level.chunks[i].cell + "\"");
                }
            }
            for (std::size_t i = 0; i < level.audioZones.size(); ++i)
            {
                if (!cellExists(level.audioZones[i].cell))
                {
                    throw LevelError(JsonPath(JsonPath("/audio/zones", i), "cell"),
                        "no cell named \"" + level.audioZones[i].cell + "\"");
                }
            }

            if (const auto sequences = root.find("sequences"); sequences != root.end())
            {
                if (!sequences->is_object())
                {
                    throw LevelError("/sequences", "must be an object of named step lists");
                }
                for (const auto& [id, steps] : sequences->items())
                {
                    const std::string at = JsonPath("/sequences", id);
                    if (!steps.is_array() || steps.empty())
                    {
                        throw LevelError(at, "must be a non-empty array of steps");
                    }
                    Sequence sequence;
                    for (std::size_t i = 0; i < steps.size(); ++i)
                    {
                        sequence.push_back(ParseStep(steps[i], JsonPath(at, i)));
                        if (sequence.back().type == SequenceStep::Type::ChangeLevel && i + 1 != steps.size())
                        {
                            throw LevelError(JsonPath(at, i), "changeLevel must be the last step");
                        }
                    }
                    level.sequences[id] = std::move(sequence);
                }
            }

            std::vector<std::string> entityPaths;
            std::size_t index = 0;
            for (const Json& entity : Array(root, "entities", ""))
            {
                entityPaths.push_back(JsonPath("/entities", index++));
                level.entities.push_back(ParseEntity(entity, entityPaths.back()));
            }

            // Markers (M20): Blender places spawns and entities; the level
            // file says what they do. A value written in the level file wins.
            if (markers)
            {
                for (const auto& [name, placement] : ParsePlacements(*markers, "spawns"))
                {
                    auto found = level.spawns.find(name);
                    if (found == level.spawns.end())
                    {
                        level.spawns[name] = SpawnPoint{ placement.position, placement.yawDegrees };
                        continue;
                    }
                    if (unplacedSpawns.erase(name) > 0)
                    {
                        found->second.position = placement.position;
                    }
                    if (!spawnsWithYaw.contains(name))
                    {
                        found->second.yawDegrees = placement.yawDegrees;
                    }
                }
                for (const auto& [name, placement] : ParsePlacements(*markers, "entities"))
                {
                    const auto entity = std::find_if(level.entities.begin(), level.entities.end(),
                        [&](const EntityData& e) { return e.name == name; });
                    if (entity == level.entities.end())
                    {
                        throw LevelError("(markers)" + JsonPath("/entities", name),
                                         "marker entity:" + name + " has no entity of that name in the level");
                    }
                    if (!entity->hasPosition)
                    {
                        entity->position = placement.position;
                        entity->hasPosition = true;
                    }
                    if (!entity->hasYaw)
                    {
                        entity->yawDegrees = placement.yawDegrees;
                    }
                }
            }
            if (!unplacedSpawns.empty())
            {
                throw LevelError(JsonPath("/spawns", *unplacedSpawns.begin()),
                                 "no \"position\" and no spawn:" + *unplacedSpawns.begin() + " marker");
            }
            for (std::size_t i = 0; i < level.entities.size(); ++i)
            {
                // Entities without a position used to sit at the origin; now
                // only when that's written down or a marker says so.
                if (!level.entities[i].hasPosition && markers)
                {
                    throw LevelError(entityPaths[i], "no \"position\" and no entity:"
                                     + level.entities[i].name + " marker");
                }
            }

            // Halos and lights riding on an entity need it to exist.
            const auto entityExists = [&](const std::string& name) {
                return std::any_of(level.entities.begin(), level.entities.end(),
                    [&](const EntityData& e) { return e.name == name; });
            };
            for (std::size_t i = 0; i < level.halos.size(); ++i)
            {
                if (!level.halos[i].entity.empty() && !entityExists(level.halos[i].entity))
                {
                    throw LevelError(JsonPath(JsonPath("/halos", i), "entity"),
                        "no entity named \"" + level.halos[i].entity + "\"");
                }
            }
            for (std::size_t i = 0; i < level.lights.size(); ++i)
            {
                if (!level.lights[i].entity.empty() && !entityExists(level.lights[i].entity))
                {
                    throw LevelError(JsonPath(JsonPath("/lights", i), "entity"),
                        "no entity named \"" + level.lights[i].entity + "\"");
                }
            }

            if (level.lab && !entityExists(level.lab->subject))
            {
                throw LevelError("/lab/subject", "no entity named \"" + level.lab->subject + "\"");
            }

            // Sequences name entities and are named by actions (M26).
            for (const auto& [id, sequence] : level.sequences)
            {
                for (std::size_t i = 0; i < sequence.size(); ++i)
                {
                    if (!sequence[i].entity.empty() && !entityExists(sequence[i].entity))
                    {
                        throw LevelError(JsonPath(JsonPath(JsonPath("/sequences", id), i), "entity"),
                            "no entity named \"" + sequence[i].entity + "\"");
                    }
                }
            }
            for (std::size_t i = 0; i < level.entities.size(); ++i)
            {
                const auto& use = level.entities[i].interactable;
                if (!use)
                {
                    continue;
                }
                for (const Action* action : { &use->action, use->lockedAction ? &*use->lockedAction : nullptr })
                {
                    const auto* run = action ? std::get_if<RunSequence>(action) : nullptr;
                    if (run && !level.sequences.contains(run->id))
                    {
                        throw LevelError(JsonPath(entityPaths[i], "interactable"),
                            "no sequence named \"" + run->id + "\"");
                    }
                }
            }

            if (level.spawns.empty())
            {
                throw LevelError("/spawns", "needs at least one spawn in \"spawns\"");
            }
            // Without one, the first spawn by name (JSON objects are ordered by key).
            level.defaultSpawn = String(root, "defaultSpawn", "", level.spawns.begin()->first);
            if (!level.FindSpawn(level.defaultSpawn))
            {
                throw LevelError("/defaultSpawn", "defaultSpawn \"" + level.defaultSpawn + "\" does not exist");
            }
            return level;
        }
    }

    std::vector<bool> VisibleCells(const std::vector<CellData>& cells, const glm::vec3& position)
    {
        std::vector<bool> visible(cells.size(), false);
        const auto inside = [&](const CellData& c) {
            return position.x >= c.min.x && position.x <= c.max.x
                && position.z >= c.min.y && position.z <= c.max.y;
        };
        const auto current = std::find_if(cells.begin(), cells.end(), inside);
        if (current == cells.end())
        {
            // Nowhere known (a gap between cells, a teleport): draw it all
            // rather than risk a hole.
            visible.assign(cells.size(), true);
            return visible;
        }
        for (std::size_t i = 0; i < cells.size(); ++i)
        {
            const bool neighbour = std::find(current->neighbours.begin(), current->neighbours.end(),
                                             cells[i].name) != current->neighbours.end();
            visible[i] = &cells[i] == &*current || neighbour;
        }
        return visible;
    }

    int CellAt(const std::vector<CellData>& cells, const glm::vec3& position)
    {
        for (std::size_t i = 0; i < cells.size(); ++i)
        {
            const CellData& c = cells[i];
            if (position.x >= c.min.x && position.x <= c.max.x
                && position.z >= c.min.y && position.z <= c.max.y)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
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

    LevelParseResult ParseLevel(std::string_view text, std::string_view markersText)
    {
        Json root;
        if (std::string error = ParseJsonText(text, root); !error.empty())
        {
            return { std::nullopt, error };
        }
        Json markers;
        if (!markersText.empty())
        {
            if (std::string error = ParseJsonText(markersText, markers); !error.empty())
            {
                return { std::nullopt, "(markers) " + error };
            }
        }
        try
        {
            return { Parse(root, markersText.empty() ? nullptr : &markers), {} };
        }
        catch (const LevelError& error)
        {
            return { std::nullopt, error.what() };
        }
        catch (const Json::exception& error)
        {
            return { std::nullopt, error.what() };
        }
    }

    namespace
    {
        std::optional<std::string> ReadFile(const std::string& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return std::nullopt;
            }
            std::stringstream contents;
            contents << file.rdbuf();
            return contents.str();
        }
    }

    std::string MarkersPathFor(const std::string& levelPath)
    {
        std::filesystem::path path(levelPath);
        return (path.parent_path() / (path.stem().string() + ".markers.json")).string();
    }

    LevelParseResult LoadLevelFile(const std::string& path)
    {
        const std::optional<std::string> text = ReadFile(path);
        if (!text)
        {
            return { std::nullopt, "cannot open " + path };
        }
        const std::optional<std::string> markers = ReadFile(MarkersPathFor(path));
        LevelParseResult result = ParseLevel(*text, markers ? std::string_view(*markers) : std::string_view{});
        if (!result.error.empty())
        {
            // "machiya_interior.json:/entities/3/...: ..." - file, then place.
            const std::string file = std::filesystem::path(path).filename().string();
            const bool fromMarkers = result.error.rfind("(markers)", 0) == 0;
            result.error = (fromMarkers
                ? std::filesystem::path(MarkersPathFor(path)).filename().string() + ":" + result.error.substr(9)
                : file + ":" + result.error);
        }
        return result;
    }
}
