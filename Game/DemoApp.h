#pragma once

#include "Atmosphere.h"
#include "AudioScape.h"
#include "Core/Application.h"
#include "Dialogue/Dialogue.h"
#include "Dialogue/DialogueRunner.h"
#include "Dialogue/DialogueView.h"
#include "Interaction/MessageFeed.h"
#include "Level/LevelManager.h"
#include "PlayerController.h"
#include "Testing/TestScript.h"
#include "Scene/Camera.h"
#include "UI/Font.h"
#include "UneaseDirector.h"
#include "World/GameState.h"
#include "World/GameWorld.h"

#include <cstddef>
#include <memory>
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
        };

        void OnLevelUnloading(Level& outgoing);
        void OnLevelLoaded(Level& incoming, const SpawnPoint& spawn);

        void UpdateMouseCapture();
        void UpdateRenderSettings();
        void ApplyLighting();
        void UpdateWindowTitle(float deltaSeconds);
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
        ArrivalError Arrival() const override;
        std::optional<float> AnimationTime(const std::string& entity) const override;
        bool AnimationPlaying(const std::string& entity) const override;
        void Log(const std::string& text) override;

        GameWorld* CurrentWorld();
        const Atom::CollisionWorld* CurrentCollision() const;

        std::string m_assetRoot;
        std::unique_ptr<TestRunner> m_testRunner;
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

        float m_titleTimer = 0.0f;
        int m_titleFrames = 0;
    };
}
