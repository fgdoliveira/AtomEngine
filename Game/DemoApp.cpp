#include "DemoApp.h"

#include <SDL3/SDL.h>

#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>

#include <cstdio>
#include <string>

namespace AtomGame
{
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

        // East end of the street, looking west along it.
        m_player.SetFeetPosition(glm::vec3{ 36.0f, 0.0f, 1.0f });
        m_camera.SetRotation(-glm::half_pi<float>(), 0.0f);

        return GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
    }

    void DemoApp::OnUpdate(float deltaSeconds)
    {
        UpdateMouseCapture();

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

        char title[160];
        std::snprintf(
            title,
            sizeof(title),
            "AtomEngine | %.0f fps | draws %u/%u | pos %.1f %.2f %.1f",
            m_titleFrames / m_titleTimer,
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
