#include "DriftApp.h"

#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Renderer/Renderer.h"

#include <SDL3/SDL.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace Drift
{
    namespace
    {
        // three.js treats hex colours as sRGB and lights in linear space.
        glm::vec3 Linear(std::uint32_t hex)
        {
            const auto channel = [](std::uint32_t v) {
                const float c = static_cast<float>(v) / 255.0f;
                return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
            };
            return { channel((hex >> 16) & 0xFF), channel((hex >> 8) & 0xFF), channel(hex & 0xFF) };
        }

        // three.js Color.setHSL (sRGB), returned linear.
        glm::vec3 FromHsl(float h, float s, float l)
        {
            h = h - std::floor(h);
            const auto hue = [](float p, float q, float t) {
                t = t - std::floor(t);
                if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
                if (t < 0.5f) return q;
                if (t < 2.0f / 3.0f) return p + (q - p) * 6.0f * (2.0f / 3.0f - t);
                return p;
            };
            const float q = l <= 0.5f ? l * (1.0f + s) : l + s - l * s;
            const float p = 2.0f * l - q;
            const glm::vec3 srgb{ hue(p, q, h + 1.0f / 3.0f), hue(p, q, h), hue(p, q, h - 1.0f / 3.0f) };
            const auto lin = [](float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
            return { lin(srgb.r), lin(srgb.g), lin(srgb.b) };
        }

        glm::vec3 Mix(glm::vec3 a, glm::vec3 b, float t) { return a + (b - a) * t; }
    }

    DriftApp::DriftApp(std::vector<std::string> arguments) : m_arguments(std::move(arguments))
    {
    }

    bool DriftApp::OnInitialize()
    {
        SDL_SetWindowTitle(GetWindow().GetSDLWindow(), "DRIFT");
        const char* base = SDL_GetBasePath();
        m_assetRoot = std::string(base ? base : "") + "Assets/Drift/";

        m_ship = Atom::Model::Load(GetRenderer(), m_assetRoot + "ship.glb");
        if (!m_ship)
        {
            std::cerr << "Missing " << m_assetRoot << "ship.glb\n";
            return false;
        }

        if (const char* seconds = SDL_getenv("ATOM_DRIFT_SECONDS"); seconds && *seconds)
        {
            m_autopilotSeconds = static_cast<float>(SDL_atof(seconds));
        }
        if (const char* capture = SDL_getenv("ATOM_DRIFT_CAPTURE"); capture && *capture)
        {
            m_capturePath = capture;
        }
        std::cout << "DRIFT ready" << (m_autopilotSeconds ? " (autopilot)" : "") << '\n';
        return true;
    }

    ShipInput DriftApp::ReadInput() const
    {
        if (m_autopilotSeconds)
        {
            // A fixed weave, so automated runs exercise steering and boost.
            return { std::sin(m_time * 0.9f), std::sin(m_time * 0.6f) * 0.6f, std::fmod(m_time, 4.0f) > 3.0f };
        }
        const Atom::Input& input = const_cast<DriftApp*>(this)->GetInput();
        const auto down = [&](SDL_Scancode a, SDL_Scancode b) { return input.IsKeyDown(a) || input.IsKeyDown(b); };
        // The mouse adds a 0.35 share, from its position in the window (-1..1).
        float mx = 0.0f;
        float my = 0.0f;
        int width = 0;
        int height = 0;
        SDL_Window* window = const_cast<DriftApp*>(this)->GetWindow().GetSDLWindow();
        SDL_GetWindowSize(window, &width, &height);
        if (width > 0 && height > 0 && (SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_FOCUS))
        {
            float x = 0.0f;
            float y = 0.0f;
            SDL_GetMouseState(&x, &y);
            mx = x / static_cast<float>(width) * 2.0f - 1.0f;
            my = -(y / static_cast<float>(height) * 2.0f - 1.0f);
        }
        ShipInput result;
        result.x = (down(SDL_SCANCODE_D, SDL_SCANCODE_RIGHT) ? 1.0f : 0.0f) - (down(SDL_SCANCODE_A, SDL_SCANCODE_LEFT) ? 1.0f : 0.0f) + mx * 0.35f;
        result.y = (down(SDL_SCANCODE_W, SDL_SCANCODE_UP) ? 1.0f : 0.0f) - (down(SDL_SCANCODE_S, SDL_SCANCODE_DOWN) ? 1.0f : 0.0f) + my * 0.35f;
        result.boost = down(SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT);
        return result;
    }

    void DriftApp::ApplyAtmosphere(float flow)
    {
        Atom::SceneLighting lighting;
        // HemisphereLight(0xffd9b0, 0x1a2a4a, 1.4) and DirectionalLight
        // (0xfff0dd, 2.2) from (-4, 8, 3). three.js and AtomEngine scale
        // light differently: these factors were matched by eye.
        lighting.skyColor = Linear(0xffd9b0) * 0.75f;
        lighting.groundColor = Linear(0x1a2a4a) * 0.75f;
        lighting.sunColor = Linear(0xfff0dd) * 1.1f;
        lighting.sunDirection = glm::normalize(glm::vec3{ -4.0f, 8.0f, 3.0f });
        lighting.bakedLight = 0.0f;
        lighting.shadowsEnabled = false; // the original has none

        // Fog(color, 60, 420) is linear; the engine's is exponential. With no
        // height falloff it's uniform, and 0.0035/m matches it near 240 m.
        lighting.fogColor = FromHsl(0.93f + flow * 0.1f, 0.4f, 0.12f + flow * 0.05f);
        lighting.fogDensity = 0.0035f;
        lighting.fogHeightFalloff = 0.0f;

        // The sky: the original's shader blends low / mid / top bands by flow;
        // the engine's gradient has horizon and zenith - mid and top.
        lighting.skyGradient = true;
        lighting.skyHorizon = Mix({ 0.30f, 0.13f, 0.25f }, { 0.62f, 0.24f, 0.18f }, flow);
        lighting.skyZenith = { 0.06f, 0.04f, 0.14f };
        lighting.sunGlow = 0.0f;
        lighting.sunSize = 0.0f;

        lighting.glowStrength = 0.45f; // M79 makes it follow flow
        GetRenderer().SetLighting(lighting);
    }

    void DriftApp::OnUpdate(float deltaSeconds)
    {
        const float dt = std::min(deltaSeconds, 0.05f); // as the original clamps
        m_time += dt;
        if (GetInput().WasKeyPressed(SDL_SCANCODE_ESCAPE))
        {
            RequestQuit(0);
            return;
        }

        const float forward = m_flight.Update(dt, m_flow, ReadInput());
        ApplyAtmosphere(m_flow);

        Atom::Renderer& renderer = GetRenderer();
        const CameraPose& camera = m_flight.Camera();
        renderer.SetCamera(camera.View(), glm::radians(camera.fovDegrees), 0.1f, 2000.0f);
        const glm::mat4 model = m_flight.ModelMatrix();
        m_ship->Submit(renderer, model);

        // The engine's glow: PointLight(0xffa050, 4 + boost*10 + sin(30t)*0.8,
        // distance 8) at the ship's tail (local 0, 0, 2).
        Atom::LiveLight engine;
        engine.position = glm::vec3(model * glm::vec4{ 0.0f, 0.0f, 2.0f, 1.0f });
        engine.radius = 8.0f;
        engine.color = Linear(0xffa050) * (0.25f * (4.0f + m_flight.boost * 10.0f + std::sin(m_time * 30.0f) * 0.8f));
        renderer.SubmitLiveLight(engine);

        if (m_autopilotSeconds)
        {
            if (!m_capturePath.empty() && !m_captured && m_time >= *m_autopilotSeconds - 0.5f)
            {
                renderer.RequestCapture(m_capturePath, true);
                m_captured = true;
            }
            if (m_time >= *m_autopilotSeconds)
            {
                std::cout << "DRIFT autopilot: flew " << static_cast<int>(-m_flight.position.z) << " m at "
                          << static_cast<int>(forward) << " m/s\n";
                RequestQuit(0);
            }
        }
    }

    void DriftApp::OnShutdown()
    {
        m_ship.reset(); // before the renderer goes (M53)
    }
}
