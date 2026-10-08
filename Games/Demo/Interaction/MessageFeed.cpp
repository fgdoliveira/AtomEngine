#include "Interaction/MessageFeed.h"

#include "UI/Font.h"
#include "UI/UIRenderer.h"

#include <algorithm>

namespace AtomGame
{
    void MessageFeed::Show(std::string text, float seconds)
    {
        m_text = std::move(text);
        m_remaining = seconds;
    }

    void MessageFeed::Update(float deltaSeconds)
    {
        m_remaining = std::max(0.0f, m_remaining - deltaSeconds);
    }

    void MessageFeed::Draw(Atom::UIRenderer& ui, const Atom::Font& font, float scale) const
    {
        if (m_remaining <= 0.0f)
        {
            return;
        }

        const float alpha = std::clamp(m_remaining / 0.6f, 0.0f, 1.0f);
        const glm::vec2 screen = ui.GetScreenSize();
        const float maxWidth = std::min(screen.x * 0.7f, 900.0f * scale);
        const std::string wrapped = ui.WrapText(font, m_text, maxWidth, scale);
        const glm::vec2 size = ui.MeasureText(font, wrapped, scale);

        const glm::vec2 padding{ 22.0f * scale, 12.0f * scale };
        const glm::vec2 position{
            (screen.x - size.x) * 0.5f,
            screen.y - size.y - 70.0f * scale
        };
        ui.DrawRect(position - padding, size + padding * 2.0f,
            { 0.03f, 0.03f, 0.035f, 0.78f * alpha });
        ui.DrawText(font, wrapped, position, { 0.93f, 0.91f, 0.85f, alpha }, scale);
    }
}
