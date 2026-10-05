#include "Platform/Input.h"

#include <SDL3/SDL.h>

#include <iostream>

namespace Atom
{
    void Input::BeginFrame()
    {
        m_keysPressed.fill(false);
        m_mouseDeltaX = 0.0f;
        m_mouseDeltaY = 0.0f;
        m_wheelDelta = 0.0f;
        m_leftClicked = false;
    }

    void Input::HandleEvent(const SDL_Event& event)
    {
        switch (event.type)
        {
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode < SDL_SCANCODE_COUNT)
            {
                if (!event.key.repeat)
                {
                    m_keysPressed[event.key.scancode] = true;
                }
                m_keysDown[event.key.scancode] = true;
            }
            break;

        case SDL_EVENT_KEY_UP:
            if (event.key.scancode < SDL_SCANCODE_COUNT)
            {
                m_keysDown[event.key.scancode] = false;
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (m_mouseCaptured)
            {
                m_mouseDeltaX += event.motion.xrel;
                m_mouseDeltaY += event.motion.yrel;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                m_leftClicked = true;
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            m_wheelDelta += event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            m_keysDown.fill(false);
            break;

        default:
            break;
        }
    }

    bool Input::SetMouseCaptured(SDL_Window* window, bool captured)
    {
        if (!SDL_SetWindowRelativeMouseMode(window, captured))
        {
            std::cerr
                << "Failed to set relative mouse mode: "
                << SDL_GetError()
                << '\n';
            return false;
        }

        m_mouseCaptured = captured;
        return true;
    }

    bool Input::IsKeyDown(SDL_Scancode key) const
    {
        return key < SDL_SCANCODE_COUNT && m_keysDown[key];
    }

    bool Input::WasKeyPressed(SDL_Scancode key) const
    {
        return key < SDL_SCANCODE_COUNT && m_keysPressed[key];
    }
}
