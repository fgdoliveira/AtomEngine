#pragma once

#include "Assets/Model.h"
#include "Atmosphere.h"
#include "AudioScape.h"
#include "Core/Application.h"
#include "Dialogue/Dialogue.h"
#include "Dialogue/DialogueRunner.h"
#include "Dialogue/DialogueView.h"
#include "Interaction/MessageFeed.h"
#include "World/GameState.h"
#include "World/GameWorld.h"
#include "Physics/CollisionWorld.h"
#include "PlayerController.h"
#include "Scene/Camera.h"
#include "UneaseDirector.h"
#include "UI/Font.h"

#include <cstddef>
#include <memory>

namespace AtomGame
{
    class DemoApp final : public Atom::Application
    {
    protected:
        bool OnInitialize() override;
        void OnUpdate(float deltaSeconds) override;
        void OnShutdown() override;

    private:
        void UpdateMouseCapture();
        void UpdateRenderSettings();
        void ApplyLighting();
        void UpdateWindowTitle(float deltaSeconds);
        void DrawOverlay(float deltaSeconds);
        void SpawnStreetEntities();
        void UpdateInteraction(float deltaSeconds);
        void DrawInteractionPrompt(float scale);
        bool BeginDialogue(const std::string& dialogueId);
        void UpdateDialogue(float deltaSeconds);
        void TurnCameraToward(const glm::vec3& point, float deltaSeconds);
        void SubmitEntities();
        void AddBoxCollider(const glm::vec3& center, const glm::vec3& halfExtents);

        // Top-level game state: what input means and what updates.
        enum class Mode
        {
            Exploring,  // walk, look, interact
            InDialogue, // movement frozen; input drives the conversation
        };

        std::unique_ptr<Atom::Model> m_street;
        Atom::CollisionWorld m_collision;

        Atom::Camera m_camera;
        PlayerController m_player;
        AudioScape m_audioScape;
        Atmosphere m_atmosphere;
        UneaseDirector m_unease;

        // Gameplay: persistent progress, this world's entities, feedback.
        GameState m_gameState;
        GameWorld m_world;
        MessageFeed m_messages;
        EntityId m_target{};

        Mode m_mode = Mode::Exploring;
        DialogueLibrary m_dialogues;
        DialogueRunner m_dialogue;
        DialogueView m_dialogueView;
        EntityId m_speaker{};            // who we're talking to
        std::unique_ptr<Atom::Model> m_keeperModel;
        float m_time = 0.0f;

        std::size_t m_fogPreset = 0;
        bool m_shadowsEnabled = true;
        int m_postMode = 0; // 0 full, 1 grade only, 2 off

        std::unique_ptr<Atom::Font> m_font;      // prompts, hints, dialogue
        std::unique_ptr<Atom::Font> m_smallFont; // debug overlay
        bool m_showDebugOverlay = false;
        float m_hintTime = 0.0f;
        float m_smoothedFrameMs = 0.0f;

        float m_titleTimer = 0.0f;
        int m_titleFrames = 0;
    };
}
