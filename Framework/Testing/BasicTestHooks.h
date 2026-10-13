#pragma once

#include "Testing/TestScript.h"

#include <string>

namespace Atom
{
    class AudioSystem;
    class Camera;
    class DevTools;
    class Renderer;
}

namespace AtomFramework
{
    class DevPanels;
    class LevelManager;
    struct Entity;
    struct SpawnPoint;
    class PlayerController;
    struct ViewToggles;

    // The scenario harness for an app that is a level, a walking player and
    // a camera (v0.0.14, M91): every TestHooks hook such an app can answer,
    // answered. A new app gets scenarios (teleport, expect_level,
    // expect_animating, capture, set msaa/fog/...) by owning one of these -
    // Apps/Samples/HelloAtom does - and overrides what it adds on top.
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
            DevPanels* panels = nullptr;    // M114: "set inspect <entity>" selects in the F10 entity report
            Atom::DevTools* devTools = nullptr; // M114: "set devtools on|off" shows F10
        };
        explicit BasicTestHooks(View view) : m_view(std::move(view)) {}

        // Where the camera must be when a level is entered (M104): the app
        // calls one of these from its level-loaded callback, before the
        // first frame. The spawn's: its feet at the player's eye height,
        // its yaw. Arrival() measures the camera against it; with none
        // given, every arrival fails - an app must say where it lands.
        void ExpectArrival(const glm::vec3& eye, float yawRadians);
        void ExpectSpawn(const SpawnPoint& spawn);

        bool TeleportTo(const std::string& entity, float distance) override;
        void Teleport(const glm::vec3& feet, float yawDegrees) override;
        bool Face(const std::string& entity) override;
        std::string LevelName() const override;
        std::string ModeName() const override;
        std::size_t VoiceCount() const override;
        ArrivalError Arrival() const override;
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
        std::optional<glm::vec3> m_arrivalEye;
        float m_arrivalYaw = 0.0f; // radians
    };
}
