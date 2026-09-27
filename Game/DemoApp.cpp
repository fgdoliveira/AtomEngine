#include "DemoApp.h"

#include <SDL3/SDL.h>

namespace AtomGame
{
    bool DemoApp::OnInitialize()
    {
        return GetInput().SetMouseCaptured(GetWindow().GetSDLWindow(), true);
    }

    void DemoApp::OnUpdate(float /*deltaSeconds*/)
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
