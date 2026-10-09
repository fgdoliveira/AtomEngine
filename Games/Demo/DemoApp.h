#pragma once

#include "Environment/Atmosphere.h"
#include "Environment/EnvironmentPresets.h"
#include "AudioScape.h"
#include "Core/Application.h"
#include "Core/FrameStatsWindow.h"
#include "Dialogue/Dialogue.h"
#include "Dialogue/DialogueRunner.h"
#include "Dialogue/DialogueView.h"
#include "Environment/EnvironmentController.h"
#include "Flashlight.h"
#include "Interaction/InteractionSystem.h"
#include "Interaction/MessageFeed.h"
#include "Level/FileWatcher.h"
#include "Input/InputContext.h"
#include "Level/LevelManager.h"
#include "Level/ViewToggles.h"
#include "Pachinko/MachineMode.h"
#include "Pachinko/PachinkoGame.h"
#include "World/FixedStep.h"
#include "Pachinko/PachinkoAttract.h"
#include "Character/PlayerController.h"
#include "Settings/Calibration.h"
#include "Settings/GameSettings.h"
#include "Testing/GameDiagnostics.h"
#include "Testing/TestScript.h"
#include "Scene/Camera.h"
#include "UI/Font.h"
#include "UneaseDirector.h"
#include "World/GameState.h"
#include "World/GameWorld.h"

#include <array>
#include <cstddef>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Demo
{
    // M76: the settings model and calibration decision live in the shared
    // framework now; the demo uses them as before, unqualified.
    using namespace AtomFramework;

    // The game. It owns only what persists for the whole session (player,
    // camera, progress, systems, UI); everything that belongs to a place
    // lives in the current Level, owned by the LevelManager.
    class DemoApp final : public Atom::Application, private TestHooks
    {
    public:
        // The command line's arguments, after the program name (M60).
        explicit DemoApp(std::vector<std::string> arguments = {}) : m_arguments(std::move(arguments)) {}

    protected:
        StartupConfig OnConfigure() override;
        void OnRenderFailure(Atom::Renderer::Failure failure) override;
        bool OnInitialize() override;
        void OnUpdate(float deltaSeconds) override;
        void OnShutdown() override;

    private:
        // Top-level game state: what input means and what updates.
        enum class Mode
        {
            Exploring,     // walk, look, interact
            InDialogue,    // movement frozen; input drives the conversation
            Transitioning, // fading between levels; input ignored
            InSequence,    // M26: a sequence runs; the player is frozen
            AtMachine,     // M29: sitting at a pachinko machine
        };

        // Where play rests between dialogues and transitions. (The character
        // lab's viewer and drive modes moved to the Showcase in v0.0.14.)
        Mode RestingMode() const { return Mode::Exploring; }

        void OnLevelUnloading(Level& outgoing);
        void OnLevelLoaded(Level& incoming, const SpawnPoint& spawn);
        void OnLevelReloaded(Level& incoming);
        void ConfigureForLevel(Level& level); // lighting, audio, atmosphere
        void UpdateHotReload(float deltaSeconds);
        void WatchLevelFiles();

        void UpdateMouseCapture();
        void UpdateRenderSettings();
        void ApplyLighting();
        void UpdateWindowTitle(float deltaSeconds);
        // Developer tools (M41): the ImGui panels, when F10 shows them.
        void DrawDevTools(float deltaSeconds);
        // Frame-time log, scripted tests and the fixed step (M57: moved out
        // of DemoApp, which keeps the frame order and calls them).
        GameDiagnostics m_diagnostics;

        // Machine-level settings (M60): GPU preference and quality tier,
        // resolved from the command line, the ATOM_* environment and (M61)
        // the saved file, before the GPU is created.
        std::vector<std::string> m_arguments;
        CommandLine m_commandLine;
        ResolvedSettings m_resolvedSettings;
        QualityTier m_qualityTier = QualityTier::High;
        void ApplyQuality(QualityTier tier);

        // Saved per user (M61), under SDL's pref path. Not read or written
        // by scripted tests, benchmarks or --no-settings runs.
        GameSettings m_savedSettings;
        std::string m_settingsPath;
        bool m_settingsPersist = false;
        void LoadSavedSettings();
        void SaveSettings();
        // Auto calibration (M64, DemoAppCalibration.cpp): fixed views of the
        // heaviest scenes measured at each tier, uncapped; Auto then uses
        // the highest tier within budget. Opt-in: --calibrate, "calibrate
        // next launch" or "Calibrate now" in F10.
        struct CalibrationRun
        {
            enum class Phase { Load, Settle, Measure };
            bool active = false;
            bool quitAfter = false; // --calibrate: report, then exit
            std::size_t scene = 0;
            std::size_t tier = 0;
            Phase phase = Phase::Load;
            float timer = 0.0f;
            float settleSeconds = 1.0f;
            float measureSeconds = 3.0f;
            Atom::FrameStatsWindow window;
            std::vector<CalibrationSample> samples;
            std::string returnLevel; // where play resumes afterwards
        } m_calibration;
        bool m_calibrateThisRun = false;
        void StartCalibration(bool quitAfter);
        void UpdateCalibration(float realSeconds);
        void FinishCalibration();

        // ATOM_SIMULATE_SWAPCHAIN_LOSS=<seconds> (M63, development): a lost
        // swapchain that many seconds in, to exercise the fallback.
        std::optional<float> m_simulateLossAt;
        // M62: the machine as the game sees it (--diagnostics), and the
        // short form performance logs carry.
        bool WriteDiagnostics(const std::string& path) const;
        std::string PerfContext() const;
        // A quality mode chosen now (F10, harness): applied at once; saved
        // when `save` and this run persists settings.
        void SetQualityMode(QualityMode mode, bool save);
        // What is drawn right now: a preset's tier, or Custom after F-keys.
        QualityTier CurrentQualityTier() const;
        std::string QualityTierName() const override { return std::string(ToString(CurrentQualityTier())); }
        void MoveWindow(int x, int y) override { SDL_SetWindowPosition(GetWindow().GetSDLWindow(), x, y); }
        double RealFrameMs() const override { return m_diagnostics.RealFrameMs(); }

        std::array<float, 240> m_frameHistory{}; // ms, a ring
        std::size_t m_frameHistoryNext = 0;
        std::optional<bool> m_devToolsCollapse; // harness: collapse/expand all panels once

        // A spot light to try the renderer's (M42) before the flashlight
        // exists: from the Spot light panel or "set spot on". Held at the
        // camera like a torch, or left where it is.
        // The flashlight (M44): owned once "has_flashlight" is set.
        Flashlight m_flashlight;
        static constexpr const char* FlashlightFlag = "has_flashlight";
        void UpdateFlashlight(float deltaSeconds);
        InteractionSystem::Settings TargetSettings() const;
        void ApplyGoneEntities(); // goneWithFlag

        Atom::SpotLight m_devSpot;
        bool m_devSpotOn = false;
        bool m_devSpotFollows = true;
        void DrawOverlay(float deltaSeconds);
        void UpdateInteraction();
        void DrawInteractionPrompt(float scale);
        bool BeginDialogue(const std::string& dialogueId);
        void UpdateDialogue(float deltaSeconds);
        void TurnCameraToward(const glm::vec3& point, float deltaSeconds);
        void InteractWith(const Entity& target);
        const Entity* FindEntity(const std::string& name);

        // TestHooks: what scripted tests may see and do (ATOM_TEST_SCRIPT).
        bool TeleportTo(const std::string& entity, float distance) override;
        void Teleport(const glm::vec3& feet, float yawDegrees) override;
        bool Face(const std::string& entity) override;
        std::string CurrentTarget() override;
        bool Interact() override;
        bool Choose(int index) override;
        void Advance() override;
        bool HasFlag(const std::string& flag) const override;
        std::string LevelName() const override;
        std::string ModeName() const override;
        std::string Message() const override;
        std::string DialogueNodeId() const override;
        std::size_t VoiceCount() const override;
        std::string SurfaceName() const override;
        float ZoneLevel(const std::string& cell) const override;
        std::pair<std::uint32_t, std::uint32_t> ScreenStats() const override;
        std::uint32_t ParticleCount() const override
        {
            return const_cast<DemoApp*>(this)->GetRenderer().GetLastFrameStats().particles;
        }
        std::uint32_t ReflectionDraws() const override
        {
            return const_cast<DemoApp*>(this)->GetRenderer().GetLastFrameStats().reflectionDrawn;
        }
        std::uint32_t WaterDraws() const override
        {
            return const_cast<DemoApp*>(this)->GetRenderer().GetLastFrameStats().waterDraws;
        }
        std::string Capture(const std::string& stem, bool includeUi) override;
        bool CapturePending() const override;
        bool Set(const std::string& what, const std::string& value) override;
        bool HoldAction(const std::string& action, bool held) override;
        bool PressAction(const std::string& action) override;
        int GetCounter(const std::string& name) const override { return m_gameState.GetCounter(name); }
        void SetCounter(const std::string& name, int value) override { m_gameState.SetCounter(name, value); }
        ArrivalError Arrival() const override;
        std::optional<float> AnimationTime(const std::string& entity) const override;
        std::string ReloadLevel() override;
        void RequestLevel(const std::string& level, const std::string& spawn) override
        {
            m_levels->RequestChange(level, spawn);
        }
        glm::vec3 FeetPosition() const override { return m_player.GetFeetPosition(); }
        bool AnimationPlaying(const std::string& entity) const override;
        bool IsLit(const std::string& entity) const override;
        void Log(const std::string& text) override;

        GameWorld* CurrentWorld();
        const Atom::CollisionWorld* CurrentCollision() const;

        AtomFramework::AssetRoots m_assets; // v0.0.14: beside the exe, or the source tree (hot reload)
        std::string m_outputRoot;           // where out/img/ captures go: the exe's folder, or the repository

        // Hot reload (M20), on when ATOM_ASSET_ROOT points at the source tree.
        bool m_hotReload = false;
        bool m_latencyFlash = false; // ATOM_LATENCY_FLASH=1 (M72)
        float m_reloadTimer = 0.0f;
        FileWatcher m_levelFiles;
        FileWatcher m_dialogueFiles;
        FileWatcher m_dataFiles; // M46: Assets/Data (the flashlight)
        void LoadFlashlightSettings();

        // Weather and time of day (M49): presets over the level's own light.
        EnvironmentController m_environment;
        EnvironmentPresets m_presets; // Assets/Environments (M49; the framework's since v0.0.14)
        std::string m_environmentName;                // showing or heading for; "" = the level's own
        float m_environmentSeconds = 3.0f;            // dev tools: how long a switch blends
        ViewToggles m_view; // v0.0.14: F2-F8 and their switches (fog preset, shadows, baked light, post, sun, rain), the framework's
        FileWatcher m_environmentFiles;
        void LoadEnvironmentPresets();
        EnvironmentState ResolveEnvironment(const std::string& name) const;
        std::vector<std::string> OfferedPresets() const;
        void ResetEnvironment();   // the level's default, at once
        void RefreshEnvironment(); // the same preset over an edited or reloaded level, at once
        bool SetEnvironment(const std::string& name, float seconds) override;
        std::string EnvironmentName() const override;
        ModelCache m_modelCache; // declared before the levels: outlives them
        std::unique_ptr<LevelManager> m_levels;

        Atom::Camera m_camera;
        PlayerController m_player;
        AudioScape m_audioScape;
        Atmosphere m_atmosphere;
        UneaseDirector m_unease;

        // Gameplay: persistent progress and feedback.
        GameState m_gameState;
        MessageFeed m_messages;
        EntityId m_target{};

        Mode m_mode = Mode::Exploring;

        // Input contexts (M29): gameplay reads actions, the map says which
        // keys give them in the current mode.
        InputMap m_inputMap = InputMap::Default();
        ActionInput m_actions;

        // The pachinko machine (M29): the camera move and fullscreen view,
        // and the game drawn into the machine's screen.
        MachineMode m_machine;
        PlayMachine m_machinePlay;
        Atom::RenderTexture* m_machineScreen = nullptr;
        std::optional<PachinkoGame> m_machineGame;
        FixedStep m_machineClock;
        // M33: balls and tokens are GameState counters ("balls", "tokens").
        static constexpr int TokensPerBuy = 10;
        static constexpr int BallsPerBuy = 50;
        std::uint32_t m_machineSessions = 0; // seeds each session differently, reproducibly
        bool BeginMachine(const PlayMachine& play);
        void UpdateMachine(float deltaSeconds);
        void DrawMachineView();
        void PlayMachineSounds(const PachinkoGame& game);
        void EndMachine();
        SequenceRunner m_sequence;
        bool RunSequence(const std::string& id);
        void UpdateSequence(float deltaSeconds);
        DialogueLibrary m_dialogues;
        DialogueRunner m_dialogue;
        DialogueView m_dialogueView;
        EntityId m_speaker{}; // who we're talking to

        // Where the last level load put the camera. Until the fade-in ends
        // the camera must stay there: those frames show the new level.
        glm::vec3 m_arrivalEye{ 0.0f };
        float m_arrivalYaw = 0.0f; // radians
        bool m_arriving = false;
        float m_time = 0.0f;


        std::unique_ptr<Atom::Font> m_font;      // prompts, hints, dialogue
        std::unique_ptr<Atom::Font> m_smallFont; // debug overlay, speaker names

        float m_hintTime = 0.0f;
        float m_smoothedFrameMs = 0.0f;

        // Documentation switches (harness "set"): parts of the frame to
        // leave out (the fixed step lives in m_diagnostics).
        bool m_drawWorld = true;          // off: only the cleared frame
        bool m_showHud = true;            // the controls hint

        float m_titleTimer = 0.0f;
        int m_titleFrames = 0;
    };
}
