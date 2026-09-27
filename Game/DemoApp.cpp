#include "DemoApp.h"

#include <SDL3/SDL.h>

#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>

#include <cstddef>
#include <cstdio>
#include <iterator>
#include <string>

namespace AtomGame
{
    namespace
    {
        struct FogPreset
        {
            const char* name;
            float density; // ~3/density metres until fully fogged
        };

        constexpr FogPreset FogPresets[] = {
            { "dense", 0.13f },
            { "medium", 0.085f },
            { "light", 0.045f },
            { "off", 0.0f },
        };
        constexpr std::size_t DefaultFogPreset = 1;
    }

    bool DemoApp::OnInitialize()
    {
        const char* basePath = SDL_GetBasePath();
        const std::string streetPath =
            std::string(basePath ? basePath : "") + "Assets/Street/";

        m_street = Atom::Model::Load(GetRenderer(), streetPath + "street.glb");
        if (!m_street || !m_collision.Load(streetPath + "street_col.glb"))
        {
            return false;
        }

        m_fogPreset = DefaultFogPreset;
        ApplyLighting();

        // East end of the street, looking west along it.
        m_player.SetFeetPosition(glm::vec3{ 36.0f, 0.0f, 1.0f });
        m_camera.SetRotation(-glm::half_pi<float>(), 0.0f);

        return GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
    }

    void DemoApp::OnUpdate(float deltaSeconds)
    {
        UpdateMouseCapture();
        UpdateRenderSettings();

        m_player.Update(GetInput(), m_camera, &m_collision, deltaSeconds);

        Atom::Renderer& renderer = GetRenderer();
        renderer.SetCamera(
            m_camera.GetViewMatrix(),
            m_camera.verticalFov,
            m_camera.nearPlane,
            m_camera.farPlane
        );

        m_street->Submit(renderer, glm::mat4{ 1.0f });

        UpdateWindowTitle(deltaSeconds);
    }

    void DemoApp::OnShutdown()
    {
        m_street.reset();
    }

    void DemoApp::UpdateWindowTitle(float deltaSeconds)
    {
        m_titleTimer += deltaSeconds;
        ++m_titleFrames;
        if (m_titleTimer < 0.5f)
        {
            return;
        }

        // Stats lag one frame (they describe the last Render()).
        const Atom::FrameStats& stats = GetRenderer().GetLastFrameStats();
        const glm::vec3& feet = m_player.GetFeetPosition();

        char title[192];
        std::snprintf(
            title,
            sizeof(title),
            "AtomEngine | %.0f fps | scene %ux%u %.0f%% MSAA %ux | fog %s"
            " | draws %u/%u | pos %.1f %.2f %.1f",
            m_titleFrames / m_titleTimer,
            stats.sceneWidth,
            stats.sceneHeight,
            GetRenderer().GetSettings().renderScale * 100.0f,
            stats.msaaSamples,
            FogPresets[m_fogPreset].name,
            stats.drawn,
            stats.submitted,
            feet.x,
            feet.y,
            feet.z
        );
        SDL_SetWindowTitle(GetWindow().GetSDLWindow(), title);

        m_titleTimer = 0.0f;
        m_titleFrames = 0;
    }

    void DemoApp::UpdateRenderSettings()
    {
        const Atom::Input& input = GetInput();
        Atom::Renderer& renderer = GetRenderer();
        Atom::RenderSettings settings = renderer.GetSettings();

        // F2: render scale 100 -> 85 -> 75 -> 50 -> 100 %.
        if (input.WasKeyPressed(SDL_SCANCODE_F2))
        {
            constexpr float scales[] = { 1.0f, 0.85f, 0.75f, 0.5f };
            std::size_t next = 0;
            for (std::size_t i = 0; i < std::size(scales); ++i)
            {
                if (settings.renderScale >= scales[i] - 0.001f)
                {
                    next = (i + 1) % std::size(scales);
                    break;
                }
            }
            settings.renderScale = scales[next];
            renderer.SetSettings(settings);
        }

        // F5: fog dense -> medium -> light -> off.
        if (input.WasKeyPressed(SDL_SCANCODE_F5))
        {
            m_fogPreset = (m_fogPreset + 1) % std::size(FogPresets);
            ApplyLighting();
        }

        // F4: MSAA 4x -> 2x -> 1x -> 4x.
        if (input.WasKeyPressed(SDL_SCANCODE_F4))
        {
            settings.msaaSamples = settings.msaaSamples > 1
                ? settings.msaaSamples / 2
                : 4;
            renderer.SetSettings(settings);
        }
    }

    void DemoApp::ApplyLighting()
    {
        // Overcast daylight; the sky is the fog colour.
        Atom::SceneLighting lighting{};
        lighting.fogColor = glm::vec3{ 0.46f, 0.47f, 0.47f };
        lighting.fogDensity = FogPresets[m_fogPreset].density;
        lighting.fogHeightFalloff = 0.08f;
        GetRenderer().SetLighting(lighting);
    }

    void DemoApp::UpdateMouseCapture()
    {
        Atom::Input& input = GetInput();

        // Escape releases the mouse; clicking back in recaptures it.
        if (input.WasKeyPressed(SDL_SCANCODE_ESCAPE))
        {
            if (input.IsMouseCaptured())
            {
                input.SetMouseCaptured(GetWindow().GetSDLWindow(), false);
            }
            else
            {
                RequestQuit();
            }
        }
        else if (!input.IsMouseCaptured()
            && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK))
        {
            input.SetMouseCaptured(GetWindow().GetSDLWindow(), true);
        }
    }
}
