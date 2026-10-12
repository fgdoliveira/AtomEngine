#pragma once

#include "UI/UIRenderer.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <cstdint>

namespace Demo
{
    // Small pixel-art drawing helpers for the 320x240 canvases (the attract
    // loop, the pachinko game): everything is rectangles.

    // A 7-segment digit, `size` pixels, strokes `thick` wide. Bits a..g.
    inline void DrawDigit(Atom::UIRenderer& canvas, int digit, glm::vec2 at, glm::vec2 size, float thick, glm::vec4 color)
    {
        constexpr std::uint8_t Segments[10] = {
            0b1111110, 0b0110000, 0b1101101, 0b1111001, 0b0110011,
            0b1011011, 0b1011111, 0b1110000, 0b1111111, 0b1111011,
        };
        const std::uint8_t bits = Segments[((digit % 10) + 10) % 10];
        const float w = size.x, h = size.y, half = h * 0.5f;
        const glm::vec2 horizontal{ w, thick }, vertical{ thick, half };
        const auto on = [&](int bit) { return (bits >> (6 - bit)) & 1; };
        if (on(0)) canvas.DrawRect(at, horizontal, color);                                          // a
        if (on(1)) canvas.DrawRect(at + glm::vec2{ w - thick, 0.0f }, vertical, color);             // b
        if (on(2)) canvas.DrawRect(at + glm::vec2{ w - thick, half }, vertical, color);             // c
        if (on(3)) canvas.DrawRect(at + glm::vec2{ 0.0f, h - thick }, horizontal, color);           // d
        if (on(4)) canvas.DrawRect(at + glm::vec2{ 0.0f, half }, vertical, color);                  // e
        if (on(5)) canvas.DrawRect(at, vertical, color);                                            // f
        if (on(6)) canvas.DrawRect(at + glm::vec2{ 0.0f, half - thick * 0.5f }, horizontal, color); // g
    }

    // `digits` 7-segment digits of `value`, right-aligned in a row starting at `at`.
    inline void DrawNumber(Atom::UIRenderer& canvas, std::uint32_t value, int digits, glm::vec2 at, glm::vec2 size,
                           float thick, float gap, glm::vec4 color)
    {
        for (int i = digits - 1; i >= 0; --i)
        {
            DrawDigit(canvas, static_cast<int>(value % 10), at + glm::vec2{ i * (size.x + gap), 0.0f }, size, thick, color);
            value /= 10;
        }
    }
}
