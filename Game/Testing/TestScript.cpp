#include "Testing/TestScript.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace AtomGame
{
    namespace
    {
        struct Arity
        {
            std::size_t min;
            std::size_t max;
        };

        // The command set, with how many arguments each takes.
        const std::unordered_map<std::string, Arity>& Commands()
        {
            static const std::unordered_map<std::string, Arity> commands{
                { "wait", { 1, 1 } },               // seconds
                { "teleport", { 4, 4 } },           // x y z yaw
                { "teleport_to", { 1, 2 } },        // entity [distance]
                { "face", { 1, 1 } },               // entity
                { "expect_target", { 1, 1 } },      // entity | none
                { "interact", { 0, 1 } },           // [entity it must be]
                { "choose", { 1, 1 } },             // 1-based choice
                { "advance", { 0, 0 } },
                { "finish_dialogue", { 0, 0 } },
                { "expect_flag", { 1, 1 } },
                { "expect_no_flag", { 1, 1 } },
                { "expect_level", { 1, 1 } },
                { "wait_for_level", { 1, 2 } },     // name [timeout]
                { "expect_mode", { 1, 1 } },
                { "wait_for_mode", { 1, 2 } },      // mode [timeout]
                { "wait_for_sequence", { 0, 1 } },  // [timeout]: the running sequence ends
                { "expect_message", { 1, 32 } },    // words that must appear
                { "expect_dialogue_node", { 1, 1 } },
                { "expect_voices_max", { 1, 1 } },
                { "expect_surface", { 1, 1 } },     // footstep surface name
                { "expect_zone", { 1, 1 } },        // cell: its ambience has faded in
                { "expect_screens", { 1, 1 } },     // min scene draws showing a live screen
                { "expect_animating", { 1, 1 } },   // entity: its clip advances
                { "wait_for_animation", { 1, 2 } }, // entity [timeout]: one-shot done
                { "reload_level", { 0, 0 } },       // hot reload in place
                { "goto_level", { 1, 2 } },         // level [spawn]: change as a door would
                { "expect_near", { 3, 4 } },        // x y z [metres]: the player's feet
                { "screenshot", { 1, 2 } },         // stem [ui]: out/img/<stem>.png
                { "capture", { 3, 4 } },            // stem count every [ui]: stem_000.png ...
                { "pan", { 9, 11 } },               // x0 y0 z0 yaw0 x1 y1 z1 yaw1 seconds [stem [ui]]
                { "set", { 2, 2 } },                // what value (msaa, fog, fov, fixed_dt, ...)
                { "hold_action", { 2, 2 } },        // action on|off (launch, leave, ...)
                { "press_action", { 1, 1 } },       // action: one frame
                { "expect_counter", { 3, 3 } },     // name op value (op: == >= <= > <)
                { "set_counter", { 2, 2 } },        // name value
                { "log", { 0, 64 } },
                { "quit", { 0, 0 } },
            };
            return commands;
        }

        bool ToFloat(const std::string& text, float& value)
        {
            const char* end = text.data() + text.size();
            return std::from_chars(text.data(), end, value).ptr == end;
        }

        std::string Join(const std::vector<std::string>& words)
        {
            std::string result;
            for (const std::string& word : words)
            {
                result += (result.empty() ? "" : " ") + word;
            }
            return result;
        }
    }

    TestScriptParseResult ParseTestScript(std::string_view text)
    {
        TestScriptParseResult result;
        std::istringstream lines{ std::string(text) };
        std::string line;
        int number = 0;

        while (std::getline(lines, line))
        {
            ++number;
            if (const std::size_t hash = line.find('#'); hash != std::string::npos)
            {
                line.resize(hash);
            }

            std::istringstream words(line);
            TestCommand command{ number };
            if (!(words >> command.name))
            {
                continue; // blank or comment
            }
            for (std::string word; words >> word;)
            {
                command.args.push_back(word);
            }

            const auto known = Commands().find(command.name);
            if (known == Commands().end())
            {
                result.error = "line " + std::to_string(number) + ": unknown command '" + command.name + "'";
                return result;
            }
            if (command.args.size() < known->second.min || command.args.size() > known->second.max)
            {
                result.error = "line " + std::to_string(number) + ": wrong number of arguments for '" + command.name + "'";
                return result;
            }
            result.commands.push_back(std::move(command));
        }
        return result;
    }

    float EasePan(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    void TestRunner::Fail(const TestCommand& command, const std::string& why)
    {
        m_failure = "line " + std::to_string(command.line) + " (" + command.name + "): " + why;
        m_finished = true;
    }

    void TestRunner::CheckArrival(TestHooks& game)
    {
        const std::string level = game.LevelName();
        if (level == m_level)
        {
            return;
        }
        m_level = level;

        // The first frame of a level is drawn from wherever the camera is,
        // so it must already be at the spawn.
        constexpr float MaxDistance = 0.01f; // metres
        constexpr float MaxYaw = 0.5f;       // degrees
        const ArrivalError error = game.Arrival();
        if (error.distance > MaxDistance || error.yawDegrees > MaxYaw)
        {
            std::ostringstream why;
            why << "entering level '" << level << "': camera is " << error.distance
                << " m and " << error.yawDegrees << " deg from the spawn";
            m_failure = why.str();
            m_finished = true;
        }
    }

    void TestRunner::Update(float deltaSeconds, TestHooks& game)
    {
        if (m_finished)
        {
            return;
        }

        CheckArrival(game);
        if (m_finished)
        {
            return;
        }

        m_total += deltaSeconds;
        if (m_total > m_timeout)
        {
            m_failure = "script timed out after " + std::to_string(static_cast<int>(m_timeout)) + " s";
            m_finished = true;
            return;
        }

        if (m_next >= m_commands.size())
        {
            m_finished = true;
            return;
        }

        const TestCommand& command = m_commands[m_next];
        m_elapsed += deltaSeconds;
        if (Execute(command, deltaSeconds, game) && !m_finished)
        {
            ++m_next;
            m_elapsed = 0.0f;
            m_frames = 0;
            m_captured = 0;
            m_requested = false;
            if (m_next >= m_commands.size())
            {
                m_finished = true;
            }
        }
    }

    bool TestRunner::Execute(const TestCommand& command, float /*deltaSeconds*/, TestHooks& game)
    {
        const std::string& name = command.name;
        const auto& args = command.args;

        const auto number = [&](std::size_t index, float fallback) {
            float value = fallback;
            if (index < args.size() && !ToFloat(args[index], value))
            {
                Fail(command, "'" + args[index] + "' is not a number");
            }
            return value;
        };

        if (name == "screenshot")
        {
            // Requested on one frame, written when that frame is presented;
            // the command ends once the file exists.
            if (!m_requested)
            {
                m_capturePath = game.Capture(args[0], args.size() > 1 && args[1] == "ui");
                m_requested = true;
                return false;
            }
            if (game.CapturePending())
            {
                return false;
            }
            if (!std::filesystem::exists(m_capturePath))
            {
                Fail(command, "no file at " + m_capturePath);
            }
            return true;
        }
        if (name == "capture")
        {
            // A numbered sequence: one frame every `every`, `count` in all.
            const int count = static_cast<int>(number(1, 1.0f));
            const int every = std::max(1, static_cast<int>(number(2, 1.0f)));
            if (game.CapturePending())
            {
                return false;
            }
            if (m_captured >= count)
            {
                return true;
            }
            if (m_frames++ % every == 0)
            {
                char suffix[16];
                std::snprintf(suffix, sizeof(suffix), "_%03d", m_captured++);
                game.Capture(args[0] + suffix, args.size() > 3 && args[3] == "ui");
            }
            return false;
        }
        if (name == "pan")
        {
            const glm::vec3 from{ number(0, 0.0f), number(1, 0.0f), number(2, 0.0f) };
            const glm::vec3 to{ number(4, 0.0f), number(5, 0.0f), number(6, 0.0f) };
            const float seconds = std::max(number(8, 1.0f), 0.001f);
            const float t = EasePan(m_elapsed / seconds);
            game.Teleport(from + (to - from) * t, number(3, 0.0f) + (number(7, 0.0f) - number(3, 0.0f)) * t);
            if (args.size() > 9)
            {
                // Filmed: one image per frame of the pan.
                char suffix[16];
                std::snprintf(suffix, sizeof(suffix), "_%03d", m_captured++);
                game.Capture(args[9] + suffix, args.size() > 10 && args[10] == "ui");
            }
            return m_elapsed >= seconds;
        }
        if (name == "hold_action" || name == "press_action")
        {
            const bool ok = name == "press_action" ? game.PressAction(args[0])
                                                   : game.HoldAction(args[0], args[1] == "on");
            if (!ok)
            {
                Fail(command, "unknown action '" + args[0] + "'");
            }
            return true;
        }
        if (name == "expect_counter")
        {
            const int actual = game.GetCounter(args[0]);
            const int expected = static_cast<int>(number(2, 0.0f));
            const std::string& op = args[1];
            const bool ok = op == "==" ? actual == expected : op == ">=" ? actual >= expected
                          : op == "<=" ? actual <= expected : op == ">" ? actual > expected
                          : op == "<" ? actual < expected : false;
            if (!ok)
            {
                Fail(command, args[0] + " is " + std::to_string(actual));
            }
            return true;
        }
        if (name == "set_counter")
        {
            game.SetCounter(args[0], static_cast<int>(number(1, 0.0f)));
            return true;
        }
        if (name == "set")
        {
            if (!game.Set(args[0], args[1]))
            {
                Fail(command, "cannot set '" + args[0] + "' to '" + args[1] + "'");
            }
            return true;
        }
        if (name == "wait")
        {
            return m_elapsed >= number(0, 0.0f);
        }
        if (name == "teleport")
        {
            game.Teleport({ number(0, 0), number(1, 0), number(2, 0) }, number(3, 0));
            return true;
        }
        if (name == "teleport_to")
        {
            if (!game.TeleportTo(args[0], number(1, 1.3f)))
            {
                Fail(command, "no entity '" + args[0] + "' in this level");
            }
            return true;
        }
        if (name == "face")
        {
            if (!game.Face(args[0]))
            {
                Fail(command, "no entity '" + args[0] + "' in this level");
            }
            return true;
        }
        if (name == "expect_target")
        {
            const std::string target = game.CurrentTarget();
            const std::string wanted = args[0] == "none" ? "" : args[0];
            if (target != wanted)
            {
                Fail(command, "target is '" + target + "', expected '" + wanted + "'");
            }
            return true;
        }
        if (name == "interact")
        {
            // Exactly what E does - including that targeting must pick it.
            const std::string target = game.CurrentTarget();
            if (!args.empty() && target != args[0])
            {
                Fail(command, "would interact with '" + target + "', not '" + args[0] + "'");
                return true;
            }
            if (!game.Interact())
            {
                Fail(command, "nothing to interact with");
            }
            return true;
        }
        if (name == "choose")
        {
            const int choice = static_cast<int>(number(0, 1.0f));
            if (game.ModeName() != "dialogue" || !game.Choose(choice - 1))
            {
                Fail(command, "choice " + args[0] + " is not available");
            }
            return true;
        }
        if (name == "advance")
        {
            game.Advance();
            return true;
        }
        if (name == "finish_dialogue")
        {
            // Press on through lines until the conversation closes. Fails
            // if a choice is waiting (the script must make it).
            if (game.ModeName() != "dialogue")
            {
                return true;
            }
            if (m_elapsed > 30.0f)
            {
                Fail(command, "dialogue did not finish");
                return true;
            }
            game.Advance();
            return false;
        }
        if (name == "expect_flag" || name == "expect_no_flag")
        {
            const bool wanted = name == "expect_flag";
            if (game.HasFlag(args[0]) != wanted)
            {
                Fail(command, "flag '" + args[0] + (wanted ? "' is not set" : "' is set"));
            }
            return true;
        }
        if (name == "expect_level")
        {
            if (game.LevelName() != args[0])
            {
                Fail(command, "in level '" + game.LevelName() + "'");
            }
            return true;
        }
        if (name == "wait_for_level" || name == "wait_for_mode")
        {
            const bool level = name == "wait_for_level";
            const std::string current = level ? game.LevelName() : game.ModeName();
            // A level only counts once the fade has finished.
            if (current == args[0] && (!level || game.ModeName() != "transitioning"))
            {
                return true;
            }
            if (m_elapsed > number(1, 10.0f))
            {
                Fail(command, "still '" + current + "'");
                return true;
            }
            return false;
        }
        if (name == "wait_for_sequence")
        {
            if (game.ModeName() != "sequence")
            {
                return true;
            }
            if (m_elapsed > number(0, 30.0f))
            {
                Fail(command, "the sequence is still running");
                return true;
            }
            return false;
        }
        if (name == "expect_mode")
        {
            if (game.ModeName() != args[0])
            {
                Fail(command, "mode is '" + game.ModeName() + "'");
            }
            return true;
        }
        if (name == "expect_message")
        {
            const std::string wanted = Join(args);
            if (game.Message().find(wanted) == std::string::npos)
            {
                Fail(command, "message is '" + game.Message() + "'");
            }
            return true;
        }
        if (name == "expect_dialogue_node")
        {
            if (game.DialogueNodeId() != args[0])
            {
                Fail(command, "dialogue node is '" + game.DialogueNodeId() + "'");
            }
            return true;
        }
        if (name == "expect_voices_max")
        {
            const auto limit = static_cast<std::size_t>(number(0, 0.0f));
            if (game.VoiceCount() > limit)
            {
                Fail(command, std::to_string(game.VoiceCount()) + " voices playing (leak?)");
            }
            return true;
        }
        if (name == "expect_animating")
        {
            const std::optional<float> time = game.AnimationTime(args[0]);
            if (!time)
            {
                Fail(command, "'" + args[0] + "' has no animation");
                return true;
            }
            if (m_elapsed <= 0.02f)
            {
                m_animationStart = *time; // first frame of the command
                return false;
            }
            if (m_elapsed < 0.5f)
            {
                return false;
            }
            if (*time == m_animationStart || !game.AnimationPlaying(args[0]))
            {
                Fail(command, "'" + args[0] + "' is not animating");
            }
            return true;
        }
        if (name == "wait_for_animation")
        {
            if (!game.AnimationTime(args[0]))
            {
                Fail(command, "'" + args[0] + "' has no animation");
                return true;
            }
            if (!game.AnimationPlaying(args[0]))
            {
                return true;
            }
            if (m_elapsed > number(1, 10.0f))
            {
                Fail(command, "'" + args[0] + "' is still playing");
                return true;
            }
            return false;
        }
        if (name == "goto_level")
        {
            game.RequestLevel(args[0], args.size() > 1 ? args[1] : std::string{});
            return true;
        }
        if (name == "reload_level")
        {
            if (const std::string error = game.ReloadLevel(); !error.empty())
            {
                Fail(command, "reload failed: " + error);
            }
            return true;
        }
        if (name == "expect_near")
        {
            const glm::vec3 wanted{ number(0, 0), number(1, 0), number(2, 0) };
            const glm::vec3 feet = game.FeetPosition();
            const glm::vec3 d = feet - wanted;
            if (std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z) > number(3, 0.1f))
            {
                std::ostringstream where;
                where << "player is at " << feet.x << ' ' << feet.y << ' ' << feet.z;
                Fail(command, where.str());
            }
            return true;
        }
        if (name == "expect_screens")
        {
            const auto [targets, draws] = game.ScreenStats();
            if (targets == 0 || draws < static_cast<std::uint32_t>(number(0, 1.0f)))
            {
                Fail(command, std::to_string(targets) + " render textures, " + std::to_string(draws) + " screen draws");
            }
            return true;
        }
        if (name == "expect_zone")
        {
            if (game.ZoneLevel(args[0]) < 0.9f)
            {
                Fail(command, "zone '" + args[0] + "' is at " + std::to_string(game.ZoneLevel(args[0])));
            }
            return true;
        }
        if (name == "expect_surface")
        {
            if (game.SurfaceName() != args[0])
            {
                Fail(command, "surface is '" + game.SurfaceName() + "'");
            }
            return true;
        }
        if (name == "log")
        {
            game.Log(Join(args));
            return true;
        }
        if (name == "quit")
        {
            m_next = m_commands.size();
            m_finished = true;
            return true;
        }

        Fail(command, "not implemented");
        return true;
    }
}
