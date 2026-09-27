#include "Testing/TestScript.h"

#include <charconv>
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
                { "expect_message", { 1, 32 } },    // words that must appear
                { "expect_dialogue_node", { 1, 1 } },
                { "expect_voices_max", { 1, 1 } },
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

    void TestRunner::Fail(const TestCommand& command, const std::string& why)
    {
        m_failure = "line " + std::to_string(command.line) + " (" + command.name + "): " + why;
        m_finished = true;
    }

    void TestRunner::Update(float deltaSeconds, TestHooks& game)
    {
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
