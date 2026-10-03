#pragma once

// A scripted stand-in for the game (TestHooks), shared by the harness and
// diagnostics tests (M57: moved out of TestScriptTests.cpp).
#include "Testing/TestScript.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace AtomGameTests
{
    using namespace AtomGame;

    // A scripted stand-in for the game: just enough state to check that the
    // runner sequences, waits and fails correctly.
    struct FakeGame : TestHooks
    {
        std::set<std::string> flags;
        std::string level = "street";
        std::string mode = "exploring";
        std::string target = "keeper";
        std::string message;
        int interactions = 0;
        std::size_t voices = 3;
        float timeUntilLevelChange = -1.0f; // simulated transition
        std::string surface = "dirt";
        ArrivalError arrivalAfterChange{};   // how the next level is entered
        ArrivalError arrival{};

        bool TeleportTo(const std::string& entity, float) override { return entity != "missing"; }
        void Teleport(const glm::vec3& to, float yaw) override { feet = to; yawDegrees = yaw; }
        float yawDegrees = 0.0f;
        bool Face(const std::string& entity) override { return entity != "missing"; }
        std::string CurrentTarget() override { return target; }
        bool Interact() override
        {
            ++interactions;
            message = "You bow.";
            flags.insert("bowed");
            return !target.empty();
        }
        bool Choose(int index) override { return index >= 0 && index < 2; }
        void Advance() override { mode = "exploring"; }
        bool HasFlag(const std::string& flag) const override { return flags.contains(flag); }
        std::string LevelName() const override { return level; }
        std::string ModeName() const override { return mode; }
        std::string Message() const override { return message; }
        std::string DialogueNodeId() const override { return ""; }
        std::size_t VoiceCount() const override { return voices; }
        std::string SurfaceName() const override { return surface; }
        float ZoneLevel(const std::string&) const override { return 0.0f; }
        std::pair<std::uint32_t, std::uint32_t> ScreenStats() const override { return { 0, 0 }; }
        std::string Capture(const std::string& stem, bool) override { captures.push_back(stem); return {}; }
        bool CapturePending() const override { return false; }
        bool Set(const std::string& what, const std::string& value) override
        {
            settings.push_back(what + "=" + value);
            return what != "nonsense";
        }
        std::vector<std::string> captures;
        std::vector<std::string> settings;
        bool HoldAction(const std::string& action, bool) override { return action != "nonsense"; }
        bool PressAction(const std::string& action) override { return action != "nonsense"; }
        std::map<std::string, int> counters;
        int GetCounter(const std::string& name) const override { return counters.contains(name) ? counters.at(name) : 0; }
        void SetCounter(const std::string& name, int value) override { counters[name] = value; }
        ArrivalError Arrival() const override { return arrival; }
        std::optional<float> animationTime;
        bool animationPlaying = false;
        std::optional<float> AnimationTime(const std::string&) const override { return animationTime; }
        bool AnimationPlaying(const std::string&) const override { return animationPlaying; }
        std::string reloadError;
        int reloads = 0;
        glm::vec3 feet{ 0.0f };
        std::string ReloadLevel() override { ++reloads; return reloadError; }
        std::string requestedLevel;
        void RequestLevel(const std::string& level, const std::string&) override { requestedLevel = level; }
        glm::vec3 FeetPosition() const override { return feet; }
        void Log(const std::string&) override {}
    };
}
