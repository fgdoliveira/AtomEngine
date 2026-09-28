#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AtomGame
{
    // How far the camera is from where the last level load placed it
    // (spawn position at eye height, spawn yaw).
    struct ArrivalError
    {
        float distance = 0.0f;   // metres
        float yawDegrees = 0.0f; // absolute, wrapped to [0, 180]
    };

    // What a test script can see and do in the game. The game implements
    // this; unit tests implement it with a fake. Every action goes through
    // the same code a player's input would.
    class TestHooks
    {
    public:
        virtual ~TestHooks() = default;

        virtual bool TeleportTo(const std::string& entity, float distance) = 0;
        virtual void Teleport(const glm::vec3& feet, float yawDegrees) = 0;
        virtual bool Face(const std::string& entity) = 0;
        // Name of what the interaction system would pick right now.
        virtual std::string CurrentTarget() = 0;
        virtual bool Interact() = 0;           // like pressing E
        virtual bool Choose(int index) = 0;    // 0-based among visible choices
        virtual void Advance() = 0;            // E/Space in dialogue

        virtual bool HasFlag(const std::string& flag) const = 0;
        virtual std::string LevelName() const = 0;
        virtual std::string ModeName() const = 0;  // exploring | dialogue | transitioning
        virtual std::string Message() const = 0;
        virtual std::string DialogueNodeId() const = 0;
        virtual std::size_t VoiceCount() const = 0; // looping voices (leak checks)
        virtual std::string SurfaceName() const = 0; // footstep surface underfoot
        virtual ArrivalError Arrival() const = 0;
        // Animation of a named entity (M19): time into its clip, nullopt if
        // it has none; and whether it is still playing.
        virtual std::optional<float> AnimationTime(const std::string& entity) const = 0;
        virtual bool AnimationPlaying(const std::string& entity) const = 0;
        // Hot reload (M20): reloads the current level in place; empty string
        // on success, else the error.
        virtual std::string ReloadLevel() = 0;
        virtual glm::vec3 FeetPosition() const = 0;
        virtual void Log(const std::string& text) = 0;
    };

    struct TestCommand
    {
        int line = 0;
        std::string name;
        std::vector<std::string> args;
    };

    struct TestScriptParseResult
    {
        std::vector<TestCommand> commands;
        std::string error; // empty on success
    };

    // One command per line; '#' starts a comment. Unknown commands and wrong
    // argument counts are rejected up front, before anything runs.
    TestScriptParseResult ParseTestScript(std::string_view text);

    // Runs a parsed script a step at a time, one command per frame (waits
    // span frames), so the game gets to update between actions. Besides the
    // script's own checks, every level change is checked: on the first
    // frame in a new level the camera must be at its spawn.
    class TestRunner
    {
    public:
        explicit TestRunner(std::vector<TestCommand> commands, float timeoutSeconds = 180.0f)
            : m_commands(std::move(commands)), m_timeout(timeoutSeconds) {}

        void Update(float deltaSeconds, TestHooks& game);

        bool IsFinished() const { return m_finished; }
        bool Passed() const { return m_finished && m_failure.empty(); }
        const std::string& GetFailure() const { return m_failure; }

    private:
        // Returns true when the command is complete and the next may run.
        bool Execute(const TestCommand& command, float deltaSeconds, TestHooks& game);
        void Fail(const TestCommand& command, const std::string& why);
        void CheckArrival(TestHooks& game);

        std::vector<TestCommand> m_commands;
        std::size_t m_next = 0;
        float m_elapsed = 0.0f;       // in the current command
        float m_total = 0.0f;
        float m_timeout;
        bool m_finished = false;
        std::string m_failure;
        std::string m_level; // level seen on the previous frame
        float m_animationStart = 0.0f; // expect_animating: time when it began
    };
}
