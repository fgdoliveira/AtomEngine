#pragma once

#include "Testing/TestScript.h"

#include <string>

namespace Atom
{
    class AudioSystem;
    class Camera;
    class Renderer;
}

namespace AtomFramework
{
    class LevelManager;
    struct Entity;
    class PlayerController;
    struct ViewToggles;

    // The scenario harness for an app that is a level, a walking player and
    // a camera (v0.0.14, M91): every TestHooks hook such an app can answer,
    // answered. A new app gets scenarios (teleport, expect_level,
    // expect_animating, capture, set msaa/fog/...) by owning one of these -
    // Samples/HelloAtom does - and overrides what it adds on top.
    class BasicTestHooks : public TestHooks
    {
    public:
        struct View
        {
            Atom::Renderer* renderer = nullptr;
            Atom::AudioSystem* audio = nullptr;
            LevelManager* levels = nullptr;
            PlayerController* player = nullptr;
            Atom::Camera* camera = nullptr;
            ViewToggles* toggles = nullptr; // "set" view switches, if the app has them
            std::string outputRoot;         // captures go to <outputRoot>out/img/
        };
        explicit BasicTestHooks(View view) : m_view(std::move(view)) {}

        bool TeleportTo(const std::string& entity, float distance) override;
        void Teleport(const glm::vec3& feet, float yawDegrees) override;
        bool Face(const std::string& entity) override;
        std::string LevelName() const override;
        std::string ModeName() const override;
        std::size_t VoiceCount() const override;
        ArrivalError Arrival() const override { return {}; }
        std::optional<float> AnimationTime(const std::string& entity) const override;
        bool AnimationPlaying(const std::string& entity) const override;
        std::string ReloadLevel() override;
        void RequestLevel(const std::string& level, const std::string& spawn) override;
        glm::vec3 FeetPosition() const override;
        void Log(const std::string& text) override;
        std::string Capture(const std::string& stem, bool includeUi) override;
        bool CapturePending() const override;
        bool Set(const std::string& what, const std::string& value) override;
        std::optional<float> Stat(const std::string& name) const override;

    private:
        const Entity* Find(const std::string& name) const;
        View m_view;
    };
}
