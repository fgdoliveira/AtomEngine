#pragma once

#include "Assets/Model.h"
#include "Atmosphere.h"
#include "AudioScape.h"
#include "Core/Application.h"
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

        std::unique_ptr<Atom::Model> m_street;
        Atom::CollisionWorld m_collision;

        Atom::Camera m_camera;
        PlayerController m_player;
        AudioScape m_audioScape;
        Atmosphere m_atmosphere;
        UneaseDirector m_unease;

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
