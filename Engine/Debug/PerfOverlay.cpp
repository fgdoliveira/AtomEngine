#include "Debug/PerfOverlay.h"

#include "Renderer/Renderer.h"
#include "UI/Font.h"
#include "UI/UIRenderer.h"

#include <algorithm>

namespace Atom
{
    PerfOverlay::PerfOverlay() = default;
    PerfOverlay::~PerfOverlay() = default;

    bool PerfOverlay::Initialize(Renderer& renderer)
    {
        // ProggyClean is drawn crisp at 13 px (its design size) - the
        // overlay looks as it did when ImGui drew it.
        m_font = Font::LoadFromMemory(renderer, FontData(), "(embedded) ProggyClean.ttf", 13.0f);
        return m_font != nullptr;
    }

    void PerfOverlay::Shutdown()
    {
        m_font.reset();
    }

    void PerfOverlay::Draw(UIRenderer& ui) const
    {
        if (!IsOverlayVisible() || m_lines.empty())
        {
            return;
        }
        // A dark panel in the top-left corner, sized to its widest line.
        constexpr float Margin = 12.0f;
        constexpr float Padding = 8.0f;
        const float lineHeight = m_font->GetLineHeight();
        float width = 0.0f;
        for (const std::string& line : m_lines)
        {
            width = std::max(width, ui.MeasureText(*m_font, line).x);
        }
        const float height = lineHeight * static_cast<float>(m_lines.size());
        ui.DrawRect({ Margin, Margin }, { width + 2.0f * Padding, height + 2.0f * Padding },
                    { 0.06f, 0.05f, 0.05f, 0.85f });
        float y = Margin + Padding;
        for (const std::string& line : m_lines)
        {
            ui.DrawText(*m_font, line, { Margin + Padding, y }, { 0.92f, 0.90f, 0.86f, 1.0f });
            y += lineHeight;
        }
    }
}
