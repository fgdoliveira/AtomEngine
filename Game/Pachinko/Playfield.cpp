#include "Pachinko/Playfield.h"

#include "Level/JsonText.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace AtomGame
{
    namespace
    {
        using Json = nlohmann::json;

        struct FieldError : std::runtime_error
        {
            FieldError(const std::string& path, const std::string& message)
                : std::runtime_error((path.empty() ? std::string("/") : path) + ": " + message) {}
        };

        float Number(const Json& object, const char* key, const std::string& path, float fallback)
        {
            const auto found = object.find(key);
            if (found == object.end())
            {
                return fallback;
            }
            if (!found->is_number())
            {
                throw FieldError(JsonPath(path, key), "must be a number");
            }
            return found->get<float>();
        }

        glm::vec2 Vec2(const Json& value, const std::string& path)
        {
            if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number())
            {
                throw FieldError(path, "must be [x, y]");
            }
            return { value[0].get<float>(), value[1].get<float>() };
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
                throw FieldError(JsonPath(path, key), "must be an array");
            }
            return *found;
        }

        Segment ParseWall(const Json& value, const std::string& path)
        {
            if (!value.is_array() || value.size() != 4)
            {
                throw FieldError(path, "must be [x0, y0, x1, y1]");
            }
            for (const Json& n : value)
            {
                if (!n.is_number())
                {
                    throw FieldError(path, "must be [x0, y0, x1, y1]");
                }
            }
            return { { value[0].get<float>(), value[1].get<float>() }, { value[2].get<float>(), value[3].get<float>() } };
        }

        Pocket::Kind ParseKind(const std::string& kind, const std::string& path)
        {
            if (kind == "start") return Pocket::Kind::Start;
            if (kind == "attacker") return Pocket::Kind::Attacker;
            if (kind == "side") return Pocket::Kind::Side;
            if (kind == "out") return Pocket::Kind::Out;
            if (kind == "foul") return Pocket::Kind::Foul;
            throw FieldError(path, "must be start, attacker, side, out or foul");
        }

        Playfield Parse(const Json& root)
        {
            if (!root.is_object())
            {
                throw FieldError("", "the file must hold a JSON object");
            }
            Playfield field;
            field.name = root.value("name", "");
            const auto bounds = root.find("field");
            if (bounds == root.end() || !bounds->contains("min") || !bounds->contains("max"))
            {
                throw FieldError("/field", "needs \"min\" and \"max\"");
            }
            field.fieldMin = Vec2((*bounds)["min"], "/field/min");
            field.fieldMax = Vec2((*bounds)["max"], "/field/max");

            std::size_t index = 0;
            for (const Json& wall : Array(root, "walls", ""))
            {
                field.walls.push_back(ParseWall(wall, JsonPath("/walls", index++)));
            }
            index = 0;
            for (const Json& arc : Array(root, "arcs", ""))
            {
                // Angles in degrees, 0 = right, 90 = up (y is down on screen).
                const std::string at = JsonPath("/arcs", index++);
                const glm::vec2 center = Vec2(arc.value("center", Json()), JsonPath(at, "center"));
                const float radius = Number(arc, "radius", at, 0.0f);
                const float from = glm::radians(Number(arc, "from", at, 0.0f));
                const float to = glm::radians(Number(arc, "to", at, 0.0f));
                const int pieces = static_cast<int>(Number(arc, "segments", at, 24.0f));
                if (radius <= 0.0f || pieces < 1)
                {
                    throw FieldError(at, "an arc needs a positive \"radius\" and \"segments\"");
                }
                const auto point = [&](float angle) {
                    return center + radius * glm::vec2{ std::cos(angle), -std::sin(angle) };
                };
                for (int i = 0; i < pieces; ++i)
                {
                    const float a = from + (to - from) * (static_cast<float>(i) / pieces);
                    const float b = from + (to - from) * (static_cast<float>(i + 1) / pieces);
                    field.walls.push_back({ point(a), point(b) });
                }
            }

            if (const auto nails = root.find("nails"); nails != root.end())
            {
                const float radius = Number(*nails, "radius", "/nails", 1.0f);
                index = 0;
                for (const Json& p : Array(*nails, "points", "/nails"))
                {
                    field.nails.push_back({ Vec2(p, JsonPath("/nails/points", index++)), radius });
                }
                index = 0;
                for (const Json& row : Array(*nails, "rows", "/nails"))
                {
                    const std::string at = JsonPath("/nails/rows", index++);
                    const float y = Number(row, "y", at, 0.0f);
                    const float x0 = Number(row, "x0", at, 0.0f);
                    const float x1 = Number(row, "x1", at, 0.0f);
                    const float spacing = Number(row, "spacing", at, 0.0f);
                    if (spacing <= 0.0f || x1 < x0)
                    {
                        throw FieldError(at, "a row needs x0 <= x1 and a positive \"spacing\"");
                    }
                    std::vector<std::pair<float, float>> skips;
                    std::size_t skipIndex = 0;
                    for (const Json& skip : Array(row, "skip", at))
                    {
                        const glm::vec2 range = Vec2(skip, JsonPath(JsonPath(at, "skip"), skipIndex++));
                        skips.emplace_back(range.x, range.y);
                    }
                    for (float x = x0; x <= x1 + 1e-3f; x += spacing)
                    {
                        bool skipped = false;
                        for (const auto& [a, b] : skips)
                        {
                            skipped = skipped || (x >= a && x <= b);
                        }
                        if (!skipped)
                        {
                            field.nails.push_back({ { x, y }, radius });
                        }
                    }
                }
            }

            const auto launch = root.find("launch");
            if (launch == root.end())
            {
                throw FieldError("/launch", "the machine needs a launcher");
            }
            field.launch.position = Vec2(launch->value("position", Json()), "/launch/position");
            field.launch.direction = Vec2(launch->value("direction", Json()), "/launch/direction");
            if (glm::length(field.launch.direction) < 1e-4f)
            {
                throw FieldError("/launch/direction", "must not be zero");
            }
            field.launch.direction = glm::normalize(field.launch.direction);
            field.launch.minSpeed = Number(*launch, "minSpeed", "/launch", field.launch.minSpeed);
            field.launch.maxSpeed = Number(*launch, "maxSpeed", "/launch", field.launch.maxSpeed);
            field.launch.perSecond = Number(*launch, "perSecond", "/launch", field.launch.perSecond);
            field.launch.jitter = Number(*launch, "jitter", "/launch", field.launch.jitter);
            if (field.launch.minSpeed <= 0.0f || field.launch.maxSpeed < field.launch.minSpeed || field.launch.perSecond <= 0.0f)
            {
                throw FieldError("/launch", "needs 0 < minSpeed <= maxSpeed and a positive perSecond");
            }

            index = 0;
            for (const Json& pocket : Array(root, "pockets", ""))
            {
                const std::string at = JsonPath("/pockets", index++);
                Pocket p;
                p.name = pocket.value("name", "");
                p.kind = ParseKind(pocket.value("kind", ""), JsonPath(at, "kind"));
                p.min = Vec2(pocket.value("min", Json()), JsonPath(at, "min"));
                p.max = Vec2(pocket.value("max", Json()), JsonPath(at, "max"));
                p.payout = static_cast<int>(Number(pocket, "payout", at, 0.0f));
                if (p.max.x <= p.min.x || p.max.y <= p.min.y)
                {
                    throw FieldError(at, "min must be above and left of max");
                }
                field.pockets.push_back(p);
            }
            const auto has = [&](Pocket::Kind kind) {
                return std::any_of(field.pockets.begin(), field.pockets.end(), [&](const Pocket& p) { return p.kind == kind; });
            };
            if (!has(Pocket::Kind::Start) || !has(Pocket::Kind::Out))
            {
                throw FieldError("/pockets", "a machine needs at least a start pocket and an out hole");
            }
            if (const auto gate = root.find("gate"); gate != root.end())
            {
                field.gate = ParseWall(*gate, "/gate");
            }
            if (const auto reels = root.find("reels"); reels != root.end())
            {
                field.reelsMin = Vec2(reels->value("min", Json()), "/reels/min");
                field.reelsMax = Vec2(reels->value("max", Json()), "/reels/max");
            }

            if (const auto rules = root.find("rules"); rules != root.end())
            {
                RulesSettings& r = field.rules;
                const std::string at = "/rules";
                r.odds = static_cast<int>(Number(*rules, "odds", at, static_cast<float>(r.odds)));
                r.maxHeld = static_cast<int>(Number(*rules, "maxHeld", at, static_cast<float>(r.maxHeld)));
                r.reachChance = Number(*rules, "reachChance", at, r.reachChance);
                r.spinSeconds = Number(*rules, "spinSeconds", at, r.spinSeconds);
                r.reachSeconds = Number(*rules, "reachSeconds", at, r.reachSeconds);
                r.resultSeconds = Number(*rules, "resultSeconds", at, r.resultSeconds);
                r.feverRounds = static_cast<int>(Number(*rules, "feverRounds", at, static_cast<float>(r.feverRounds)));
                r.ballsPerRound = static_cast<int>(Number(*rules, "ballsPerRound", at, static_cast<float>(r.ballsPerRound)));
                r.roundSeconds = Number(*rules, "roundSeconds", at, r.roundSeconds);
                r.intervalSeconds = Number(*rules, "intervalSeconds", at, r.intervalSeconds);
                if (r.odds < 1 || r.maxHeld < 1 || r.feverRounds < 1 || r.ballsPerRound < 1
                    || r.spinSeconds <= 0.0f || r.roundSeconds <= 0.0f || r.reachChance < 0.0f || r.reachChance > 1.0f)
                {
                    throw FieldError(at, "odds, maxHeld, feverRounds, ballsPerRound >= 1; positive times; reachChance 0..1");
                }
            }

            // Everything on the board, and no two nails touching.
            const auto inside = [&](glm::vec2 p) {
                return p.x >= field.fieldMin.x && p.x <= field.fieldMax.x && p.y >= field.fieldMin.y && p.y <= field.fieldMax.y;
            };
            for (std::size_t i = 0; i < field.walls.size(); ++i)
            {
                if (!inside(field.walls[i].a) || !inside(field.walls[i].b))
                {
                    throw FieldError("/walls", "wall or arc piece " + std::to_string(i) + " leaves the field");
                }
            }
            for (std::size_t i = 0; i < field.nails.size(); ++i)
            {
                if (!inside(field.nails[i].position))
                {
                    throw FieldError("/nails", "nail " + std::to_string(i) + " is outside the field");
                }
                for (std::size_t j = i + 1; j < field.nails.size(); ++j)
                {
                    if (glm::length(field.nails[i].position - field.nails[j].position)
                        < field.nails[i].radius + field.nails[j].radius)
                    {
                        throw FieldError("/nails", "nails " + std::to_string(i) + " and " + std::to_string(j) + " overlap");
                    }
                }
            }
            if (!inside(field.launch.position))
            {
                throw FieldError("/launch/position", "is outside the field");
            }
            return field;
        }
    }

    PlayfieldParseResult ParsePlayfield(std::string_view text)
    {
        Json root;
        if (std::string error = ParseJsonText(text, root); !error.empty())
        {
            return { std::nullopt, error };
        }
        try
        {
            return { Parse(root), {} };
        }
        catch (const FieldError& error)
        {
            return { std::nullopt, error.what() };
        }
        catch (const Json::exception& error)
        {
            return { std::nullopt, error.what() };
        }
    }

    PlayfieldParseResult LoadPlayfieldFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return { std::nullopt, "cannot open " + path };
        }
        std::stringstream text;
        text << file.rdbuf();
        PlayfieldParseResult result = ParsePlayfield(text.str());
        if (!result.error.empty())
        {
            result.error = path + ":" + result.error;
        }
        return result;
    }
}
