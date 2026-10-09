#pragma once

#include "BoothSynth.h"
#include "Features/Features.h"
#include "Character/LabViewer.h"
#include "Character/PlayerController.h"
#include "Character/SpringArm.h"
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

#include <SDL3/SDL_scancode.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <array>
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
        bool SetClip(const std::string& entity, const std::string& clip) override;
        std::string ClipName(const std::string& entity) const override;
        bool SetAnimatorParam(const std::string& entity, const std::string& param, float value) override;
        bool HoldAction(const std::string& action, bool held) override;
        bool PressAction(const std::string& action) override;
        std::optional<float> Stat(const std::string& name) const override;
        bool SetEnvironment(const std::string& name, float seconds) override;
        std::string EnvironmentName() const override;
        std::uint32_t ParticleCount() const override;
        std::uint32_t ReflectionDraws() const override;
        std::uint32_t WaterDraws() const override;
        double RealFrameMs() const override { return m_diagnostics.RealFrameMs(); }
        std::string ReloadLevel() override;
        void RequestLevel(const std::string& level, const std::string& spawn) override;
        glm::vec3 FeetPosition() const override
        {
            return m_mode == Mode::Driving ? m_driveBody.GetFeetPosition() : m_player.GetFeetPosition();
        }
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
        void UpdateBooth();

        // The character lab (M36-M38, the demo's until v0.0.14; ShowcaseLab.cpp):
        // at the pavilion, E opens the model viewer around the character,
        // Tab hands it to the keys, E or Esc goes back to walking.
        enum class Mode { Walking, Viewing, Driving };
        bool LabKeyDown(const char* action, SDL_Scancode a, SDL_Scancode b = SDL_SCANCODE_UNKNOWN) const;
        bool LabKeyPressed(const char* action, SDL_Scancode a, SDL_Scancode b = SDL_SCANCODE_UNKNOWN) const;
        Entity* LabSubject();
        void BeginLab();
        void EndLab();
        void UpdateLab(float deltaSeconds);
        void ApplyLabPose();
        void BeginDrive();
        void EndDrive();
        void UpdateDrive(float deltaSeconds);
        void DrawSkeleton(const Entity& subject);
        void DrawLabOverlay();

        // The lens (v0.0.14, ShowcaseLens.cpp): Tab shows how the place is
        // made - a callout per feature in view, its live cost, its source
        // and manual section; 1-9 and 0 switch a feature off and on.
        static int FeatureIndex(std::string_view id);
        void SetFeature(int index, bool on);
        void ApplyFeatures(); // after a level (re)load: the switches survive it
        void UpdateLens();
        void DrawLens();
        std::string FeatureCost(int index) const;
        std::optional<glm::vec2> Project(const glm::vec3& world) const;
        void SendToBooth(BoothCommand command, int argument, float value);
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
        // The lab: the viewer's state, the driven body and its camera arm,
        // and where the subject stood (put back when the lab closes).
        Mode m_mode = Mode::Walking;
        LabViewer m_viewer;
        PlayerController m_driveBody;
        SpringArm m_arm;
        bool m_labScriptedParams = false; // a scenario sets the animator's parameters itself
        int m_driveSteps = 0;
        glm::vec3 m_labHome{ 0.0f };
        float m_labHomeYaw = 0.0f;
        // The lens: open or not, each feature's switch, the one last touched.
        bool m_lens = false;
        std::array<bool, 10> m_featureOn{ true, true, true, true, true, true, true, true, true, true };
        int m_lensFocus = 0;
        float m_smoothedMs = 0.0f;

        // Input a scenario holds or presses, as keys would (TestHooks).
        std::vector<std::string> m_heldActions;
        std::vector<std::string> m_pressedActions;

        // The synth booth: started on its first note; the stream is declared
        // after the sequencer it plays, so it stops first.
        BoothSynth m_booth;
        std::unique_ptr<Atom::SynthStream> m_audio;
        float m_cutoff = 2400.0f; // Hz, as the booth starts
        bool m_delay = false;
        bool m_drawWorld = true; // "set world off": draw nothing but the UI (captures)
        bool m_showHud = true;   // "set hud off": no caption or lab panel
        float m_time = 0.0f;
        glm::vec3 m_arrivalEye{ 0.0f };
        float m_arrivalYaw = 0.0f;
    };
}
