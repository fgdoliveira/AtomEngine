#pragma once

#include "Character/PlayerController.h"
#include "Core/Application.h"
#include "Environment/Atmosphere.h"
#include "Environment/EnvironmentController.h"
#include "Environment/EnvironmentPresets.h"
#include "Level/LevelManager.h"
#include "Level/ModelCache.h"
#include "Level/SoundLibrary.h"
#include "Level/ViewToggles.h"
#include "Platform/AssetRoots.h"
#include "Scene/Camera.h"
#include "Testing/GameDiagnostics.h"
#include "Testing/TestScript.h"
#include "UI/Font.h"

#include <glm/vec3.hpp>

#include <memory>
#include <string>
#include <vector>

namespace Showcase
{
    using namespace AtomFramework;

    // The Showcase (v0.0.14): every engine feature in one scene, an area
    // per feature, built only from the engine and the framework - the
    // engine's front door. Walk around (or jump with 1-6), switch the
    // weather (P), the view (F2-F8), look at the numbers (F1, F10).
    class ShowcaseApp : public Atom::Application, public TestHooks
    {
    protected:
        bool OnInitialize() override;
        void OnUpdate(float deltaSeconds) override;
        void OnShutdown() override;

    public:
        // The scenario harness (TestHooks): what the Showcase has.
        bool TeleportTo(const std::string& entity, float distance) override;
        void Teleport(const glm::vec3& feet, float yawDegrees) override;
        bool Face(const std::string& entity) override;
        std::string LevelName() const override;
        std::string ModeName() const override;
        std::size_t VoiceCount() const override;
        ArrivalError Arrival() const override;
        std::optional<float> AnimationTime(const std::string& entity) const override;
        bool AnimationPlaying(const std::string& entity) const override;
        std::string AnimatorState(const std::string& entity) const override;
        bool SetEnvironment(const std::string& name, float seconds) override;
        std::string EnvironmentName() const override;
        std::uint32_t ParticleCount() const override;
        std::uint32_t ReflectionDraws() const override;
        std::uint32_t WaterDraws() const override;
        double RealFrameMs() const override { return m_diagnostics.RealFrameMs(); }
        std::string ReloadLevel() override;
        void RequestLevel(const std::string& level, const std::string& spawn) override;
        glm::vec3 FeetPosition() const override { return m_player.GetFeetPosition(); }
        void Log(const std::string& text) override;
        std::string Capture(const std::string& stem, bool includeUi) override;
        bool CapturePending() const override;
        bool Set(const std::string& what, const std::string& value) override;

    private:
        // An area of the scene: the spawn it starts at, and what it shows.
        struct Area
        {
            const char* spawn;
            const char* title;
            const char* shows;
        };
        static const std::vector<Area>& Areas();
        const Area* AreaAt(const glm::vec3& feet) const; // nearest, within reach

        void OnLevelLoaded(Level& incoming, const SpawnPoint& spawn);
        void ApplyLighting();
        void ResetEnvironment();
        void UpdateMouseCapture();
        void UpdateKeys();
        void UpdatePavilion();
        void DrawCaption();
        const Entity* FindEntity(const std::string& name) const;
        Entity* FindEntity(const std::string& name);
        const Atom::CollisionWorld* CurrentCollision() const;

        // The Showcase plays no level sounds: its one sound is the synth.
        class Silence final : public SoundLibrary
        {
        public:
            Atom::SoundHandle GetSound(std::string_view /*name*/) const override { return nullptr; }
        };

        AssetRoots m_assets;
        std::string m_outputRoot; // out/img/ captures
        Silence m_sounds;
        ModelCache m_modelCache;
        std::unique_ptr<LevelManager> m_levels;
        PlayerController m_player;
        Atom::Camera m_camera;
        ViewToggles m_view;
        Atmosphere m_atmosphere;
        EnvironmentPresets m_presets;
        EnvironmentController m_environment;
        std::string m_environmentName; // "" is the level's own light
        GameDiagnostics m_diagnostics;
        std::unique_ptr<Atom::Font> m_font;
        std::unique_ptr<Atom::Font> m_smallFont;
        float m_time = 0.0f;
        glm::vec3 m_arrivalEye{ 0.0f };
        float m_arrivalYaw = 0.0f;
    };
}
