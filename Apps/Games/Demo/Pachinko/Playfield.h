#pragma once

#include "Pachinko/PachinkoRules.h"
#include "Pachinko/Physics2D.h"

#include <glm/vec2.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Demo
{
    // A pachinko machine's playfield as data (M31): Assets/Machines/<name>.json.
    // Coordinates are pixels on the 320x240 screen, y down.
    //
    //   field    the board's bounds (every shape must lie inside)
    //   walls    straight segments: [x0, y0, x1, y1]
    //   arcs     curved rails, split into short segments
    //   nails    single points and rows (y, from x0 to x1 every `spacing`,
    //            leaving out the `skip` ranges)
    //   launch   where balls leave the launcher, which way, how fast
    //   pockets  rectangles that catch a ball: start (spins the lottery),
    //            attacker (the fever gate), side (a small payout), out
    //            (lost), foul (fell back down the lane: returned)
    //   gate     the attacker's lid: a segment present while it's closed
    //   reels    where the lottery's reels are drawn

    struct Pocket
    {
        enum class Kind { Start, Attacker, Side, Out, Foul };
        std::string name;
        Kind kind = Kind::Out;
        glm::vec2 min{ 0.0f };
        glm::vec2 max{ 0.0f };
        int payout = 0; // balls paid per ball caught

        bool Contains(glm::vec2 p) const
        {
            return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
        }
    };

    struct LaunchSettings
    {
        glm::vec2 position{ 0.0f };
        glm::vec2 direction{ 0.0f, -1.0f }; // normalised on load
        float minSpeed = 250.0f;
        float maxSpeed = 450.0f;
        float perSecond = 1.7f;  // balls fired while the handle is held
        float jitter = 0.02f;    // relative speed noise (seeded)
    };

    struct Playfield
    {
        std::string name;
        glm::vec2 fieldMin{ 0.0f };
        glm::vec2 fieldMax{ 0.0f };
        std::vector<Segment> walls;
        std::vector<Nail> nails;
        LaunchSettings launch;
        std::vector<Pocket> pockets;
        std::optional<Segment> gate;
        glm::vec2 reelsMin{ 0.0f };
        glm::vec2 reelsMax{ 0.0f };
        RulesSettings rules;     // M32: odds, fever
    };

    struct PlayfieldParseResult
    {
        std::optional<Playfield> playfield;
        std::string error; // JSON Pointer to the offending value, like levels
    };

    PlayfieldParseResult ParsePlayfield(std::string_view text);
    PlayfieldParseResult LoadPlayfieldFile(const std::string& path);
}
