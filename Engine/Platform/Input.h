#pragma once

#include <SDL3/SDL_scancode.h>

#include <array>
#include <cstdint>

union SDL_Event;
struct SDL_Window;

namespace Atom
{
    class Input
    {
    public:
        // Call once per frame before polling events.
        void BeginFrame();
        void HandleEvent(const SDL_Event& event);

        bool SetMouseCaptured(SDL_Window* window, bool captured);

        bool IsKeyDown(SDL_Scancode key) const;
        bool WasKeyPressed(SDL_Scancode key) const;

        float GetMouseDeltaX() const
        {
            return m_mouseDeltaX;
        }

        float GetMouseDeltaY() const
        {
            return m_mouseDeltaY;
        }

        bool IsMouseCaptured() const
        {
            return m_mouseCaptured;
        }

        // Wheel notches this frame (M29): positive is away from the user.
        float GetWheelDelta() const
        {
            return m_wheelDelta;
        }

        // The left mouse button went down this frame (M72: the latency
        // measurement's input).
        bool WasLeftClicked() const
        {
            return m_leftClicked;
        }
        // When that click happened (M73): SDL's event timestamp, in
        // SDL_GetTicksNS nanoseconds; 0 without a click.
        std::uint64_t GetLeftClickTimeNS() const
        {
            return m_leftClickNs;
        }

    private:
        std::array<bool, SDL_SCANCODE_COUNT> m_keysDown{};
        std::array<bool, SDL_SCANCODE_COUNT> m_keysPressed{};

        float m_mouseDeltaX = 0.0f;
        float m_mouseDeltaY = 0.0f;
        float m_wheelDelta = 0.0f;
        bool m_leftClicked = false;
        std::uint64_t m_leftClickNs = 0;
        bool m_mouseCaptured = false;
    };
}
