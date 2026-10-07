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

        Atom::Renderer& renderer = GetRenderer();
        m_ship = Atom::Model::Load(renderer, m_assetRoot + "ship.glb");
        m_ring = Atom::Model::Load(renderer, m_assetRoot + "ring.glb");
        m_orb = Atom::Model::Load(renderer, m_assetRoot + "orb.glb");
        m_rock = Atom::Model::Load(renderer, m_assetRoot + "rock.glb");
        const std::string font = m_assetRoot + "Fonts/SpaceGrotesk.ttf";
        m_titleFont = Atom::Font::Load(renderer, font, 88.0f);
        m_comboFont = Atom::Font::Load(renderer, font, 42.0f);
        m_smallFont = Atom::Font::Load(renderer, font, 12.0f);
        if (!m_ship || !m_ring || !m_orb || !m_rock || !m_titleFont || !m_comboFont || !m_smallFont)
        {
            std::cerr << "Missing DRIFT assets in " << m_assetRoot << '\n';
            return false;
        }

        // The look (M79), as the original's toon.js: every material toon;
        // an inverted-hull outline (0.045, #07060f) on all but the orbs.
        const auto style = [](Atom::Model& model, bool outline) {
            for (Atom::Material& material : model.GetMaterials())
            {
                material.toon = true;
                material.outline = outline ? 0.045f : 0.0f;
                material.outlineColor = Linear(0x07060f);
            }
        };
        style(*m_ship, true);
        style(*m_ring, true);
        style(*m_rock, true);
        style(*m_orb, false);

        const std::uint8_t white[4]{ 255, 255, 255, 255 };
        m_white = renderer.CreateTexture(1, 1, white, false);
        renderer.SetParticleAtlas(m_white.get(), 1);

        if (const char* seconds = SDL_getenv("ATOM_DRIFT_SECONDS"); seconds && *seconds)
        {
            m_autopilotSeconds = static_cast<float>(SDL_atof(seconds));
            // Straight into the run - unless ATOM_DRIFT_TITLE=1 keeps the
            // title screen up (to check it in an automated run).
            const char* title = SDL_getenv("ATOM_DRIFT_TITLE");
            if (!(title && SDL_strcmp(title, "1") == 0))
            {
                m_running = true;
                m_titleFade = 0.0f;
            }
        }
        if (const char* capture = SDL_getenv("ATOM_DRIFT_CAPTURE"); capture && *capture)
        {
            m_capturePath = capture;
        }
        std::uint32_t seed = static_cast<std::uint32_t>(SDL_GetTicksNS());
        if (const char* fixed = SDL_getenv("ATOM_DRIFT_SEED"); fixed && *fixed)
        {
            seed = static_cast<std::uint32_t>(SDL_atoi(fixed));
        }
        m_world = std::make_unique<World>(seed);
        std::cout << "DRIFT ready" << (m_autopilotSeconds ? " (autopilot)" : "") << ", course seed " << seed << '\n';
        return true;
    }

    ShipInput DriftApp::ReadInput() const
    {
        if (m_autopilotSeconds)
        {
            // A simple pilot: aim at the path 20 m ahead (through the gates),
            // with a little weave and a boost now and then.
            const glm::vec3& p = m_flight.position;
            const glm::vec2 c = Path(p.z - 20.0f);
            return { std::clamp((c.x - p.x) / 2.5f + std::sin(m_time * 0.9f) * 0.2f, -1.0f, 1.0f),
                     std::clamp((c.y - p.y) / 2.5f, -1.0f, 1.0f), std::fmod(m_time, 4.0f) > 3.0f };
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

        // UnrealBloomPass(strength 0.6, radius 0.5, threshold 0.85), its
        // strength following flow: 0.45 + flow * 0.7.
        lighting.glowStrength = 0.45f + flow * 0.7f;
        lighting.glowThreshold = 0.85f;
        m_fogDensity = lighting.fogDensity;
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

        // The title screen: a click launches (the original's start overlay).
        if (!m_running && GetInput().WasLeftClicked())
        {
            m_running = true;
        }
        if (m_running)
        {
            m_titleFade = std::max(0.0f, m_titleFade - dt / 0.6f);
        }

        // Behind the title the ship glides idle (flow 0.2, no controls).
        const float previousZ = m_flight.position.z;
        const float forward = m_running ? m_flight.Update(dt, m_flow.value, ReadInput())
                                        : m_flight.Update(dt, 0.2f, ShipInput{});
        m_world->Update(dt, m_time, m_flight.position.z);
        if (m_running)
        {
            const FlowEvents events = m_flow.Check(*m_world, m_flight.position, previousZ, dt);
            if (events.orbs > 0 || events.rockHit)
            {
                m_chainPop = 1.0f; // the counter changed: pop it
            }
            if (events.rockHit)
            {
                m_flight.shake = 1.0f;
            }
        }
        ApplyAtmosphere(m_flow.value);

        Atom::Renderer& renderer = GetRenderer();
        const CameraPose& camera = m_flight.Camera();
        renderer.SetCamera(camera.View(), glm::radians(camera.fovDegrees), 0.1f, 2000.0f);
        const glm::mat4 model = m_flight.ModelMatrix();
        m_ship->Submit(renderer, model);
        SubmitWorld();
        m_speedField.Update(dt, forward);
        SubmitSpeedField(forward);
        DrawHud(dt);

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
                          << static_cast<int>(forward) << " m/s; rings " << m_flow.ringsPassed << " passed, "
                          << m_flow.ringsMissed << " missed; orbs " << m_flow.orbsCollected << "; rocks "
                          << m_flow.rocksHit << "; flow " << static_cast<int>(m_flow.value * 100.0f) << "%\n";
                RequestQuit(0);
            }
        }
    }

    void DriftApp::SubmitWorld()
    {
        Atom::Renderer& renderer = GetRenderer();
        for (const Thing& t : m_world->Things())
        {
            if (!t.visible)
            {
                continue;
            }
            const Atom::Model& model = t.kind == Kind::Ring ? *m_ring : t.kind == Kind::Orb ? *m_orb : *m_rock;
            model.Submit(renderer, t.Matrix());
        }
    }

    void DriftApp::SubmitSpeedField(float forward)
    {
        // The original draws stars and streaks with fog off; the engine's
        // particles are fogged (lerp(colour, fog, f)). So each colour is
        // pre-compensated - (target - fog * f) / (1 - f) - to come out as
        // the target after the shader's fog.
        const glm::vec3 fog = GetRenderer().GetLighting().fogColor;
        const glm::vec3 eye = m_flight.Camera().position;
        const auto unfogged = [&](glm::vec3 target, const glm::vec3& at) {
            const float f = std::min(0.95f, 1.0f - std::exp(-m_fogDensity * glm::distance(at, eye)));
            return (target - fog * f) / (1.0f - f);
        };
        const glm::vec3& ship = m_flight.position;
        m_particles.clear();

        // PointsMaterial(0xf3e9d8, size 0.35, opacity 0.8) in a box riding
        // at 0.9 of the ship's lateral position.
        const glm::vec3 starBox{ ship.x * 0.9f, ship.y * 0.9f, ship.z };
        const glm::vec3 starColor = Linear(0xf3e9d8);
        for (const glm::vec3& star : m_speedField.Stars())
        {
            Atom::Particle p{};
            p.position = starBox + star;
            p.size = 0.35f;
            p.color = glm::vec4{ unfogged(starColor, p.position), 0.8f };
            m_particles.push_back(p);
        }

        // LineBasicMaterial(0xffb27a): segments from the head back along -Z,
        // as long and as opaque as the speed says.
        const float opacity = SpeedField::StreakOpacity(forward);
        if (opacity > 0.0f)
        {
            const float length = SpeedField::StreakLength(forward);
            const glm::vec3 streakColor = Linear(0xffb27a);
            for (const glm::vec3& head : m_speedField.Streaks())
            {
                Atom::Particle p{};
                p.position = ship + head - glm::vec3{ 0.0f, 0.0f, length * 0.5f }; // the segment's middle
                p.size = 0.04f;
                p.stretch = length;
                p.color = glm::vec4{ unfogged(streakColor, p.position), opacity };
                m_particles.push_back(p);
            }
        }
        GetRenderer().SetParticleStreak(glm::vec3{ 0.0f, 0.0f, 1.0f });
        GetRenderer().SubmitParticles(m_particles);
    }

    void DriftApp::DrawHud(float dt)
    {
        // The original's HUD (index.html): sRGB colours, window pixels.
        const glm::vec4 ink{ 243 / 255.0f, 233 / 255.0f, 216 / 255.0f, 1.0f };
        const glm::vec4 dim{ ink.r, ink.g, ink.b, 0.35f };
        const glm::vec4 hot{ 1.0f, 138 / 255.0f, 92 / 255.0f, 1.0f };
        Atom::UIRenderer& ui = GetRenderer().GetUI();
        const glm::vec2 screen = ui.GetScreenSize();
        if (screen.x <= 0.0f)
        {
            return;
        }

        // Top left: the controls.
        ui.DrawText(*m_smallFont, "WASD / ARROWS  -  STEER\nSHIFT  -  BOOST\nM  -  MUTE", { 32.0f, 32.0f }, dim);

        // Top right: the chain, popping (scale 1.25, hot) when it changes.
        m_chainPop = std::max(0.0f, m_chainPop - dt / 0.25f);
        const std::string chain = std::to_string(m_flow.chain);
        const float scale = 1.0f + 0.25f * m_chainPop;
        const glm::vec2 chainSize = ui.MeasureText(*m_comboFont, chain, scale);
        ui.DrawText(*m_comboFont, chain, { screen.x - 32.0f - chainSize.x, 28.0f }, ink + (hot - ink) * m_chainPop, scale);
        const glm::vec2 label = ui.MeasureText(*m_smallFont, "C H A I N");
        ui.DrawText(*m_smallFont, "C H A I N", { screen.x - 32.0f - label.x, 28.0f + chainSize.y }, dim);

        // Bottom centre: the flow bar.
        const float width = std::min(320.0f, screen.x * 0.6f);
        const float left = (screen.x - width) * 0.5f;
        const float bar = screen.y - 36.0f;
        const std::string percent = std::to_string(static_cast<int>(std::lround(m_flow.value * 100.0f))) + "%";
        ui.DrawText(*m_smallFont, "F L O W", { left, bar - 22.0f }, dim);
        const glm::vec2 pct = ui.MeasureText(*m_smallFont, percent);
        ui.DrawText(*m_smallFont, percent, { left + width - pct.x, bar - 22.0f }, dim);
        ui.DrawRect({ left, bar }, { width, 2.0f }, dim);
        ui.DrawRect({ left, bar }, { width * m_flow.value, 2.0f }, hot);

        // The title over the idle glide, fading out once launched.
        if (m_titleFade > 0.0f)
        {
            ui.DrawRect({ 0.0f, 0.0f }, screen, { 13 / 255.0f, 11 / 255.0f, 28 / 255.0f, 0.72f * m_titleFade });
            const char* title = "D R I F T";
            const glm::vec2 t = ui.MeasureText(*m_titleFont, title);
            ui.DrawText(*m_titleFont, title, { (screen.x - t.x) * 0.5f, screen.y * 0.5f - t.y }, { ink.r, ink.g, ink.b, m_titleFade });
            const char* prompt = "C L I C K   T O   L A U N C H";
            const glm::vec2 p = ui.MeasureText(*m_smallFont, prompt);
            ui.DrawText(*m_smallFont, prompt, { (screen.x - p.x) * 0.5f, screen.y * 0.5f + 12.0f }, { dim.r, dim.g, dim.b, dim.a * m_titleFade });
        }
    }

    void DriftApp::OnShutdown()
    {
        // GPU objects go before the renderer (M53).
        m_ship.reset();
        m_ring.reset();
        m_orb.reset();
        m_rock.reset();
        m_titleFont.reset();
        m_comboFont.reset();
        m_smallFont.reset();
        GetRenderer().SetParticleAtlas(nullptr, 1);
        m_white.reset();
    }
}
