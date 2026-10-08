#include "Pachinko/PhysicsDebugDraw.h"

#include "Pachinko/Physics2D.h"
#include "UI/UIRenderer.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace AtomGame
{
    void DrawPhysicsDebug(Atom::UIRenderer& canvas, const World2D& world, glm::vec4 color)
    {
        for (std::size_t index = 0; index < world.GetSegments().size(); ++index)
        {
            const Segment& segment = world.GetSegments()[index];
            if (!world.IsSegmentEnabled(index))
            {
                continue;
            }
            // One pixel per unit of length: a line made of squares.
            const glm::vec2 delta = segment.b - segment.a;
            const int steps = std::max(1, static_cast<int>(std::ceil(glm::length(delta))));
            for (int i = 0; i <= steps; ++i)
            {
                const glm::vec2 p = segment.a + delta * (static_cast<float>(i) / steps);
                canvas.DrawRect(glm::floor(p), { 1.0f, 1.0f }, color * glm::vec4{ 0.7f, 0.7f, 0.7f, 1.0f });
            }
        }
        for (const Nail& nail : world.GetNails())
        {
            const float size = std::max(1.0f, nail.radius * 2.0f);
            canvas.DrawRect(glm::floor(nail.position - glm::vec2{ size * 0.5f }), { size, size }, color);
        }
        const float r = world.GetSettings().ballRadius;
        for (const Ball& ball : world.GetBalls())
        {
            const glm::vec2 corner = glm::floor(ball.position - glm::vec2{ r });
            canvas.DrawRect(corner, { 2.0f * r, 2.0f * r }, { 0.78f, 0.80f, 0.86f, 1.0f });
            canvas.DrawRect(corner + glm::vec2{ 1.0f }, { 2.0f, 2.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }); // highlight
        }
        for (const Impact& impact : world.GetImpacts())
        {
            canvas.DrawRect(glm::floor(impact.position) - glm::vec2{ 1.0f }, { 3.0f, 3.0f }, { 1.0f, 0.8f, 0.3f, 1.0f });
        }
    }
}
