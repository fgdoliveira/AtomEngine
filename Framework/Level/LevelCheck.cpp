#include "Level/LevelCheck.h"

#include "Level/LevelData.h"

#include "Assets/Model.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <type_traits>
#include <variant>

namespace AtomFramework
{
    namespace
    {
        namespace fs = std::filesystem;

        // Beyond this a position is almost certainly a typo (metres).
        constexpr float FarAway = 10000.0f;

        bool Finite(const glm::vec3& v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }

        // Everything one level's check needs to say where a problem is.
        class Checker
        {
        public:
            Checker(const LevelCheckOptions& options, std::string levelName, std::string source,
                    std::vector<LevelDiagnostic>& out)
                : m_options(options), m_level(std::move(levelName)), m_source(std::move(source)), m_out(out)
            {
            }

            void Report(LevelDiagnostic::Severity severity, std::string entity, std::string property,
                        std::string problem, std::string fix = {}, std::string source = {})
            {
                m_out.push_back({ severity, m_level, source.empty() ? m_source : std::move(source), std::move(entity),
                                  std::move(property), std::move(problem), std::move(fix) });
            }

            void Error(std::string entity, std::string property, std::string problem, std::string fix = {})
            {
                Report(LevelDiagnostic::Severity::Error, std::move(entity), std::move(property), std::move(problem),
                       std::move(fix));
            }

            // A file the level names must be under the app's roots. Found
            // only in another app's Assets/, it is a cross-app dependency
            // the app's own build would never have (v0.0.16, M106).
            void RequireFile(const std::string& relative, const std::string& entity, const std::string& property,
                             const char* what)
            {
                if (relative.empty() || fs::exists(m_options.assets.Resolve(relative)))
                {
                    return;
                }
                for (const OtherAppAssets& other : m_options.otherApps)
                {
                    if (fs::exists(fs::path(other.folder) / relative))
                    {
                        Error(entity, property, std::string(what) + " \"" + relative + "\" is only in another app's assets ("
                              + other.app + ")",
                              "an app reads only its own Assets/ and the shared Content/: move the file to Content/ or copy it into this app");
                        return;
                    }
                }
                Error(entity, property, std::string(what) + " \"" + relative + "\" was not found",
                      "check the path (relative to Assets/), or build it: Tools/Blender/export.ps1");
            }

            // The clips a model holds, loaded once per model.
            const std::set<std::string>* Clips(const std::string& model)
            {
                if (!m_options.checkClips || model.empty())
                {
                    return nullptr;
                }
                const std::string path = m_options.assets.Resolve(model);
                if (!fs::exists(path))
                {
                    return nullptr; // reported by RequireFile
                }
                auto [found, inserted] = m_clips.try_emplace(model);
                if (inserted)
                {
                    for (const Atom::AnimationClip& clip : Atom::LoadModelAnimations(path))
                    {
                        found->second.insert(clip.name);
                    }
                }
                return &found->second;
            }

            void RequireClip(const std::string& model, const std::string& clip, const std::string& entity,
                             const std::string& property)
            {
                if (clip.empty())
                {
                    return;
                }
                const std::set<std::string>* clips = Clips(model);
                if (clips && !clips->contains(clip))
                {
                    std::string known;
                    for (const std::string& name : *clips)
                    {
                        known += (known.empty() ? "" : ", ") + name;
                    }
                    Error(entity, property, "clip \"" + clip + "\" is not in " + model,
                          known.empty() ? "the model has no animation clips" : "its clips: " + known);
                }
            }

        private:
            const LevelCheckOptions& m_options;
            std::string m_level;
            std::string m_source;
            std::vector<LevelDiagnostic>& m_out;
            std::map<std::string, std::set<std::string>> m_clips;
        };

        // "street.json:/entities/3/model: message" -> source, property, problem.
        void SplitParseError(const std::string& error, const std::string& levelFile, std::string& source,
                             std::string& property, std::string& problem)
        {
            problem = error;
            const std::size_t colon = error.find(':');
            const std::string file = colon == std::string::npos ? std::string() : error.substr(0, colon);
            if (file.ends_with(".json"))
            {
                source = "Levels/" + file;
                problem = error.substr(colon + 1);
            }
            else
            {
                source = "Levels/" + levelFile;
            }
            if (problem.rfind('/', 0) == 0)
            {
                const std::size_t end = problem.find(": ");
                if (end != std::string::npos)
                {
                    property = problem.substr(0, end);
                    problem = problem.substr(end + 2);
                }
            }
        }

        // Which entity or spawn a JSON Pointer is about.
        std::string Subject(const LevelData& level, const std::string& pointer)
        {
            unsigned index = 0;
            if (std::sscanf(pointer.c_str(), "/entities/%u", &index) == 1 && index < level.entities.size())
            {
                return level.entities[index].name;
            }
            if (pointer.rfind("/spawns/", 0) == 0)
            {
                const std::string rest = pointer.substr(8);
                return "spawn " + rest.substr(0, rest.find('/'));
            }
            return {};
        }

        // The level a changeLevel names, parsed once; nullptr if it doesn't load.
        class Destinations
        {
        public:
            explicit Destinations(const AssetRoots& assets) : m_assets(assets) {}

            const LevelData* Find(const std::string& name, bool& exists)
            {
                const std::string path = m_assets.Resolve("Levels/" + name + ".json");
                exists = fs::exists(path);
                if (!exists)
                {
                    return nullptr;
                }
                auto [found, inserted] = m_levels.try_emplace(name);
                if (inserted)
                {
                    found->second = LoadLevelFile(path).level;
                }
                return found->second ? &*found->second : nullptr;
            }

        private:
            const AssetRoots& m_assets;
            std::map<std::string, std::optional<LevelData>> m_levels;
        };

        void CheckAction(const Action& action, const LevelData& level, const std::string& entity,
                         const std::string& property, Checker& check, Destinations& destinations,
                         const std::map<std::string, const EntityData*>& byName)
        {
            if (const auto* change = std::get_if<ChangeLevel>(&action))
            {
                bool exists = false;
                const LevelData* target = destinations.Find(change->level, exists);
                if (!exists)
                {
                    check.Error(entity, property, "changeLevel goes to \"" + change->level + "\", which has no Levels/"
                                + change->level + ".json in this app");
                }
                else if (target && !change->spawn.empty() && !target->FindSpawn(change->spawn))
                {
                    check.Error(entity, property, "changeLevel lands on spawn \"" + change->spawn + "\", which "
                                + change->level + " doesn't have");
                }
            }
            else if (const auto* play = std::get_if<PlayAnimation>(&action))
            {
                const auto found = byName.find(play->entity);
                if (found == byName.end())
                {
                    check.Error(entity, property, "playAnimation names entity \"" + play->entity + "\", which this level doesn't have");
                }
                else
                {
                    check.RequireClip(found->second->model, play->clip, entity, property);
                }
            }
            else if (const auto* machine = std::get_if<PlayMachine>(&action))
            {
                check.RequireFile(machine->machine, entity, property, "machine");
            }
            (void)level;
        }

        void CheckLevel(const LevelData& level, Checker& check, const LevelCheckOptions& options)
        {
            check.RequireFile(level.model, {}, "/model", "model");
            check.RequireFile(level.collision, {}, "/collision", "collision");
            if (level.lightmap)
            {
                check.RequireFile(level.lightmap->texture, {}, "/lightmap/texture", "lightmap");
            }
            if (level.sky)
            {
                check.RequireFile(level.sky->panorama, {}, "/sky/panorama", "sky panorama");
            }
            for (std::size_t i = 0; i < level.chunks.size(); ++i)
            {
                const ChunkData& chunk = level.chunks[i];
                const std::string at = "/chunks/" + std::to_string(i);
                check.RequireFile(chunk.model, chunk.name, at + "/model", "chunk model");
                check.RequireFile(chunk.collision, chunk.name, at + "/collision", "chunk collision");
                check.RequireFile(chunk.lightmap, chunk.name, at + "/lightmap", "chunk lightmap");
            }
            for (std::size_t i = 0; i < level.impostors.size(); ++i)
            {
                check.RequireFile(level.impostors[i].set, {}, "/impostors/" + std::to_string(i) + "/set", "impostor descriptor");
            }

            // Names: markers, interactions, sequences and scenarios all find
            // an entity by its name, so two of one name is always a mistake.
            std::map<std::string, const EntityData*> byName;
            for (std::size_t i = 0; i < level.entities.size(); ++i)
            {
                const EntityData& e = level.entities[i];
                const std::string at = "/entities/" + std::to_string(i);
                if (!byName.emplace(e.name, &e).second)
                {
                    check.Error(e.name, at + "/name", "another entity is already named \"" + e.name + "\"",
                                "names are how markers, interactions, sequences and scenarios find an entity: make it unique");
                }
                if (!Finite(e.position) || std::abs(e.position.x) > FarAway || std::abs(e.position.y) > FarAway
                    || std::abs(e.position.z) > FarAway)
                {
                    check.Report(LevelDiagnostic::Severity::Warning, e.name, at + "/position",
                                 "position is more than 10 km from the origin", "a typo in the coordinates?");
                }
                if (e.collider)
                {
                    const glm::vec3 h = e.collider->halfExtents;
                    if (!Finite(h) || h.x <= 0.0f || h.y <= 0.0f || h.z <= 0.0f)
                    {
                        check.Error(e.name, at + "/collider/halfExtents", "a collider's half extents must all be positive");
                    }
                }
                check.RequireFile(e.model, e.name, at + "/model", "model");
                if (e.animation)
                {
                    check.RequireClip(e.model, e.animation->clip, e.name, at + "/animation/clip");
                }
                if (e.animator)
                {
                    for (const auto& [stateName, state] : e.animator->states)
                    {
                        const std::string stateAt = at + "/animator/states/" + stateName;
                        check.RequireClip(e.model, state.clip, e.name, stateAt);
                        check.RequireClip(e.model, state.blendFrom, e.name, stateAt + "/blend");
                        check.RequireClip(e.model, state.blendTo, e.name, stateAt + "/blend");
                    }
                }
            }

            Destinations destinations(options.assets);
            for (std::size_t i = 0; i < level.entities.size(); ++i)
            {
                const EntityData& e = level.entities[i];
                if (!e.interactable)
                {
                    continue;
                }
                const std::string at = "/entities/" + std::to_string(i) + "/interactable";
                CheckAction(e.interactable->action, level, e.name, at + "/action", check, destinations, byName);
                if (e.interactable->lockedAction)
                {
                    CheckAction(*e.interactable->lockedAction, level, e.name, at + "/locked", check, destinations, byName);
                }
            }
            for (const auto& [id, sequence] : level.sequences)
            {
                for (std::size_t i = 0; i < sequence.size(); ++i)
                {
                    const SequenceStep& step = sequence[i];
                    const std::string at = "/sequences/" + id + "/" + std::to_string(i);
                    // The parser already refuses a step naming no entity;
                    // whether the clip is in the model needs the model.
                    if (step.type == SequenceStep::Type::PlayAnimation)
                    {
                        if (const auto found = byName.find(step.entity); found != byName.end())
                        {
                            check.RequireClip(found->second->model, step.clip, step.entity, at);
                        }
                    }
                    if (step.type == SequenceStep::Type::ChangeLevel)
                    {
                        CheckAction(ChangeLevel{ step.text, step.clip }, level, {}, at, check, destinations, byName);
                    }
                }
            }
        }
    }

    std::vector<LevelDiagnostic> CheckLevelFile(const std::string& levelPath, const LevelCheckOptions& options)
    {
        std::vector<LevelDiagnostic> out;
        fs::path path(levelPath);
        if (!path.has_parent_path() || !fs::exists(path))
        {
            const std::string name = path.extension() == ".json" ? path.string() : levelPath + ".json";
            path = fs::path(options.assets.Resolve("Levels/" + fs::path(name).filename().string()));
        }
        const std::string file = path.filename().string();
        const std::string levelName = path.stem().string();
        Checker check(options, levelName, "Levels/" + file, out);

        const LevelParseResult result = LoadLevelFile(path.string());
        if (!result.level)
        {
            std::string source, property, problem;
            SplitParseError(result.error, file, source, property, problem);
            check.Report(LevelDiagnostic::Severity::Error, {}, property, problem, {}, source);
            return out;
        }
        for (const LevelWarning& warning : result.level->warnings)
        {
            check.Report(LevelDiagnostic::Severity::Warning, Subject(*result.level, warning.path), warning.path,
                         warning.message);
        }
        CheckLevel(*result.level, check, options);
        return out;
    }

    std::vector<LevelDiagnostic> CheckLevels(const LevelCheckOptions& options)
    {
        std::vector<LevelDiagnostic> out;
        if (options.assets.Roots().empty())
        {
            return out;
        }
        const fs::path folder = fs::path(options.assets.Roots().front()) / "Levels";
        std::vector<fs::path> levels;
        if (fs::is_directory(folder))
        {
            for (const auto& entry : fs::directory_iterator(folder))
            {
                const std::string name = entry.path().filename().string();
                if (entry.path().extension() == ".json" && !name.ends_with(".markers.json"))
                {
                    levels.push_back(entry.path());
                }
            }
        }
        std::sort(levels.begin(), levels.end());
        for (const fs::path& level : levels)
        {
            std::vector<LevelDiagnostic> found = CheckLevelFile(level.string(), options);
            out.insert(out.end(), found.begin(), found.end());
        }
        return out;
    }

    std::string FormatDiagnostic(const LevelDiagnostic& d)
    {
        std::ostringstream text;
        text << (d.severity == LevelDiagnostic::Severity::Error ? "ERROR" : "WARNING") << " [" << d.level << "]\n";
        if (!d.entity.empty())
        {
            text << "  Entity: " << d.entity << '\n';
        }
        text << "  Source: " << d.source << '\n';
        if (!d.property.empty())
        {
            text << "  Property: " << d.property << '\n';
        }
        text << "  Problem: " << d.problem << '\n';
        if (!d.fix.empty())
        {
            text << "  Fix: " << d.fix << '\n';
        }
        return text.str();
    }

    std::size_t CountErrors(const std::vector<LevelDiagnostic>& diagnostics)
    {
        return static_cast<std::size_t>(std::count_if(diagnostics.begin(), diagnostics.end(),
            [](const LevelDiagnostic& d) { return d.severity == LevelDiagnostic::Severity::Error; }));
    }
}
