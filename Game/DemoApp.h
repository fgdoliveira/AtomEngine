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
    class DemoApp final : public Atom::Application
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
        void Interact(const Entity& target);

        GameWorld* CurrentWorld();
        const Atom::CollisionWorld* CurrentCollision() const;

        std::string m_assetRoot;
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
        float m_time = 0.0f;

        std::size_t m_fogPreset = 0;
        bool m_shadowsEnabled = true;
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
