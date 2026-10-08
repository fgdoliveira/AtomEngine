#pragma once

#include <glm/vec4.hpp>

namespace Atom
{
    class UIRenderer;
}

namespace Demo
{
    class World2D;

    // Draws a physics world into a 2D canvas in its own units (M30): nails
    // as dots, segments as lines of pixels, balls as small squares, and the
    // last step's impacts as flashes. For debugging and for the first look
    // at a playfield before it has art.
    void DrawPhysicsDebug(Atom::UIRenderer& canvas, const World2D& world,
                          glm::vec4 color = { 0.85f, 0.85f, 0.8f, 1.0f });
}
