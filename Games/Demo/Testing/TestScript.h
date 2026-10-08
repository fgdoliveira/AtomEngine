#pragma once

#include "Testing/PairedBench.h"
#include <glm/vec3.hpp>

#include <cstddef>
#include <optional>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace Demo
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
        virtual float ZoneLevel(const std::string& cell) const = 0; // cell ambience faded in, 0..1 (M25)
        // Last frame: render textures drawn into, scene draws sampling one (M27).
        virtual std::pair<std::uint32_t, std::uint32_t> ScreenStats() const = 0;
        virtual ArrivalError Arrival() const = 0;
        // Animation of a named entity (M19): time into its clip, nullopt if
        // it has none; and whether it is still playing.
        virtual std::optional<float> AnimationTime(const std::string& entity) const = 0;
        virtual bool AnimationPlaying(const std::string& entity) const = 0;
        // M36: the clip an entity plays (by name; "" if none / bind pose).
        virtual bool SetClip(const std::string& /*entity*/, const std::string& /*clip*/) { return false; }
        virtual std::string ClipName(const std::string& /*entity*/) const { return {}; }
        // M37: animator parameters and state.
        virtual bool SetAnimatorParam(const std::string& /*entity*/, const std::string& /*param*/, float /*value*/)
        {
            return false;
        }
        virtual std::string AnimatorState(const std::string& /*entity*/) const { return {}; }
        // M49: switch to an environment preset ("level": the level's own),
        // blended over seconds; false for an unknown preset. Its name, or
        // "(blending)" mid-transition.
        virtual bool SetEnvironment(const std::string& /*name*/, float /*seconds*/) { return false; }
        virtual std::string EnvironmentName() const { return "level"; }
        // M50: particles the last frame drew.
        virtual std::uint32_t ParticleCount() const { return 0; }
        // M63: move the window (desktop coordinates), e.g. onto the display
        // of the other adapter of a hybrid laptop.
        virtual void MoveWindow(int /*x*/, int /*y*/) {}
        // M61: the quality tier being drawn (low, balanced, high, custom).
        virtual std::string QualityTierName() const { return "high"; }
        // M53: draws the last frame's planar reflection pass made (0: none).
        virtual std::uint32_t ReflectionDraws() const { return 0; }
        // M48: water surfaces the last frame drew.
        virtual std::uint32_t WaterDraws() const { return 0; }
        // M46: the last frame's real duration (ms), for "bench".
        virtual double RealFrameMs() const { return 0.0; }
        // M44: is the entity's focus point lit by the flashlight?
        virtual bool IsLit(const std::string& /*entity*/) const { return false; }
        // Hot reload (M20): reloads the current level in place; empty string
        // on success, else the error.
        virtual std::string ReloadLevel() = 0;
        // Starts a level change as a door would (with the fade).
        virtual void RequestLevel(const std::string& level, const std::string& spawn) = 0;
        virtual glm::vec3 FeetPosition() const = 0;
        virtual void Log(const std::string& text) = 0;

        // Documentation captures: saves the next frame as out/img/<stem>.png
        // and returns the full path; CapturePending is true until written.
        virtual std::string Capture(const std::string& stem, bool includeUi) = 0;
        virtual bool CapturePending() const = 0;
        // A render or scene switch (msaa, fog, fov, ...); false if unknown
        // or the value is invalid.
        virtual bool Set(const std::string& what, const std::string& value) = 0;
        // Input actions (M29), as a player's keys would give them: held until
        // released, or pressed for one frame. False for an unknown action.
        virtual bool HoldAction(const std::string& action, bool held) = 0;
        virtual bool PressAction(const std::string& action) = 0;
        // Counters (M33).
        virtual int GetCounter(const std::string& name) const = 0;
        virtual void SetCounter(const std::string& name, int value) = 0;
    };

    // The panning camera's easing (smoothstep): 0 -> 0, 1 -> 1, gentle at
    // both ends so a pan starts and stops without a jolt.
    float EasePan(float t);

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
            : m_commands(std::move(commands)), m_timeout(timeoutSeconds)
        {
            // M51: "timeout <seconds>" anywhere in the script raises it (long benchmarks).
            for (const TestCommand& command : m_commands)
            {
                if (command.name == "timeout" && !command.args.empty())
                {
                    const float seconds = std::strtof(command.args[0].c_str(), nullptr);
                    m_timeout = seconds > 0.0f ? seconds : m_timeout;
                }
            }
        }

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
        int m_frames = 0;              // capture: frames spent in the command
        int m_captured = 0;            // capture: images written so far
        bool m_requested = false;      // screenshot/capture: a request is in flight
        std::string m_capturePath;     // screenshot: where it goes
        std::optional<PairedBench> m_bench;   // bench: running
        std::optional<double> m_benchDelta;   // bench: the last result (B - A, ms)
    };
}
