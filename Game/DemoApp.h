#pragma once

#include "Atmosphere.h"
#include "Character/LabViewer.h"
#include "Character/SpringArm.h"
#include "AudioScape.h"
#include "Core/Application.h"
#include "Core/FrameStatsWindow.h"
#include "Dialogue/Dialogue.h"
#include "Dialogue/DialogueRunner.h"
#include "Dialogue/DialogueView.h"
#include "Flashlight.h"
#include "Interaction/InteractionSystem.h"
#include "Interaction/MessageFeed.h"
#include "Level/FileWatcher.h"
#include "Input/InputContext.h"
#include "Level/LevelManager.h"
#include "Pachinko/MachineMode.h"
#include "Pachinko/PachinkoGame.h"
#include "World/FixedStep.h"
#include "World/PachinkoAttract.h"
#include "PlayerController.h"
#include "Testing/TestScript.h"
#include "Scene/Camera.h"
#include "UI/Font.h"
#include "UneaseDirector.h"
#include "World/GameState.h"
#include "World/GameWorld.h"

#include <array>
#include <cstddef>
#include <fstream>
#include <memory>
#include <optional>
#include <string>

namespace AtomGame
{
    // The game. It owns only what persists for the whole session (player,
    // camera, progress, systems, UI); everything that belongs to a place
    // lives in the current Level, owned by the LevelManager.
    class DemoApp final : public Atom::Application, private TestHooks
    {
    protected:
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
            Viewing,       // M36: the character lab's model viewer
            Driving,       // M38: the lab's third-person drive mode
        };

        // Where play rests between dialogues and transitions: the viewer in
        // the character lab, exploring everywhere else.
        Mode RestingMode() const { return m_lab ? Mode::Viewing : Mode::Exploring; }

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
        // Frame-time log (M46, ATOM_PERF_LOG=1): after an engine warm-up,
        // one line per block of frames - median, p95, mean - and a CSV row
        // if ATOM_PERF_CSV names a file. Real frame times, not the
        // harness's fixed step.
        struct PerfLog
        {
            bool enabled = false;
            int warmupLeft = 300;  // frames: pipelines, caches, allocations - not heat
            std::size_t block = 240;
            int blockIndex = 0;
            Atom::FrameStatsWindow window;
            std::unique_ptr<std::ofstream> csv;
            std::string label;     // what is being measured (the level, a bench half)
        } m_perf;
        void InitializePerfLog();
        void RecordFrameTime(float realSeconds);
        double m_lastRealFrameMs = 0.0;
        double RealFrameMs() const override { return m_lastRealFrameMs; }

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
        void LoadTestScript();
        void UpdateTestScript(float deltaSeconds);

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
        glm::vec3 FeetPosition() const override
        {
            return m_mode == Mode::Driving ? m_driveBody.GetFeetPosition() : m_player.GetFeetPosition();
        }
        bool AnimationPlaying(const std::string& entity) const override;
        bool SetClip(const std::string& entity, const std::string& clip) override;
        std::string ClipName(const std::string& entity) const override;
        bool SetAnimatorParam(const std::string& entity, const std::string& param, float value) override;
        std::string AnimatorState(const std::string& entity) const override;
        bool IsLit(const std::string& entity) const override;
        void Log(const std::string& text) override;

        GameWorld* CurrentWorld();
        const Atom::CollisionWorld* CurrentCollision() const;

        std::string m_assetRoot;

        // Hot reload (M20), on when ATOM_ASSET_ROOT points at the source tree.
        bool m_hotReload = false;
        float m_reloadTimer = 0.0f;
        FileWatcher m_levelFiles;
        FileWatcher m_dialogueFiles;
        FileWatcher m_dataFiles; // M46: Assets/Data (the flashlight)
        void LoadFlashlightSettings();
        std::unique_ptr<TestRunner> m_testRunner;
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
        // The character lab (M36): set while the level has a "lab".
        std::optional<LevelLab> m_lab;
        LabViewer m_viewer;
        void BeginLab(Level& level);
        void UpdateLab(float deltaSeconds);
        Entity* FindLabSubject();
        void ApplyLabPose(); // the viewer's clip and time onto the subject
        void DrawLabOverlay(float scale);
        void DrawSkeleton(const Entity& subject);
        bool m_labScriptedParams = false; // the harness sets them: no demo

        // Drive mode (M38): the subject walks on the player's kind of body,
        // the camera on a spring arm; its feet's events count footsteps.
        PlayerController m_driveBody;
        SpringArm m_arm;
        int m_driveSteps = 0;
        void BeginDrive();
        void EndDrive();
        void UpdateDrive(float deltaSeconds);
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

        std::size_t m_fogPreset = 0;
        bool m_shadowsEnabled = true;
        bool m_bakedLightEnabled = true; // F3: compare with the flat ambient
        int m_postMode = 0; // 0 full, 1 grade only, 2 off

        std::unique_ptr<Atom::Font> m_font;      // prompts, hints, dialogue
        std::unique_ptr<Atom::Font> m_smallFont; // debug overlay, speaker names
        bool m_showDebugOverlay = false;
        float m_hintTime = 0.0f;
        float m_smoothedFrameMs = 0.0f;

        // Documentation switches (harness "set"): a fixed time step for
        // evenly spaced frame sequences, and parts of the frame to leave out.
        float m_fixedDeltaSeconds = 0.0f; // 0 = real time
        bool m_drawWorld = true;          // off: only the cleared frame
        bool m_showHud = true;            // the controls hint
        bool m_sunEnabled = true;

        float m_titleTimer = 0.0f;
        int m_titleFrames = 0;
    };
}
