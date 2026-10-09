#include "UI/UiKit.h"

#include "UI/Font.h"
#include "UI/UIRenderer.h"

#include <algorithm>

namespace AtomFramework
{
    const UiTheme& DefaultUiTheme()
    {
        static const UiTheme theme;
        return theme;
    }

    float UiScale(glm::vec2 screen)
    {
        return std::clamp(screen.y / 720.0f, 0.75f, 2.0f);
    }

    std::vector<float> ColumnOffsets(const std::vector<float>& widths, float gap)
    {
        std::vector<float> offsets;
        offsets.reserve(widths.size());
        float x = 0.0f;
        for (const float width : widths)
        {
            offsets.push_back(x);
            x += width + gap;
        }
        return offsets;
    }

    UiKit::UiKit(Atom::UIRenderer& ui, const Atom::Font& title, const Atom::Font& body, const UiTheme& theme)
        : m_ui(ui), m_title(title), m_body(body), m_theme(theme), m_screen(ui.GetScreenSize()),
          m_scale(UiScale(m_screen))
    {
    }

    float UiKit::LineHeight() const
    {
        return m_body.GetLineHeight() * m_scale;
    }

    glm::vec2 UiKit::Measure(std::string_view text) const
    {
        return m_ui.MeasureText(m_body, text, m_scale);
    }

    void UiKit::Panel(glm::vec2 origin, glm::vec2 size, const glm::vec4* edge)
    {
        m_ui.DrawRect(origin, size, m_theme.panel);
        if (edge)
        {
            m_ui.DrawRect(origin, { 3.0f * m_scale, size.y }, *edge);
        }
    }

    void UiKit::Text(std::string_view text, glm::vec2 at, const glm::vec4& color)
    {
        m_ui.DrawText(m_body, text, at, color, m_scale);
    }

    void UiKit::TitleText(std::string_view text, glm::vec2 at, const glm::vec4& color)
    {
        m_ui.DrawText(m_title, text, at, color, m_scale);
    }

    void UiKit::Caption(const std::string& title, const std::string& line, const std::string& keys)
    {
        // A panel off the corner by the margin; the text inside by 2 pads
        // (the accent bar, then air).
        const float margin = Margin();
        const float pad = Pad();
        const float lineHeight = LineHeight();
        const float bottom = m_screen.y - margin;
        const float keysY = bottom - pad - lineHeight;
        const float lineY = keysY - lineHeight * 1.4f;
        const float titleY = lineY - m_title.GetLineHeight() * m_scale;
        const float width = std::max({ Measure(keys).x, Measure(line).x,
                                       m_ui.MeasureText(m_title, title, m_scale).x });
        const glm::vec2 origin{ margin, titleY - pad };
        Panel(origin, { width + 4.0f * pad, bottom - origin.y }, &m_theme.accent);
        const float x = margin + 2.0f * pad;
        TitleText(title, { x, titleY }, m_theme.ink);
        Text(line, { x, lineY }, m_theme.ink);
        Text(keys, { x, keysY }, m_theme.dim);
    }

    void UiKit::HintBar(const std::string& keys, float alpha)
    {
        const glm::vec2 size = Measure(keys);
        const glm::vec2 at{ (m_screen.x - size.x) * 0.5f, m_screen.y - size.y - Margin() * 1.5f };
        glm::vec4 panel = m_theme.panel;
        panel.a *= alpha;
        m_ui.DrawRect(at - glm::vec2{ Pad(), Pad() * 0.5f }, size + glm::vec2{ 2.0f * Pad(), Pad() }, panel);
        glm::vec4 dim = m_theme.dim;
        dim.a *= alpha;
        Text(keys, at, dim);
    }

    void UiKit::Callout(glm::vec2 point, const std::string& head, const std::string& detail, bool on)
    {
        const float pad = Pad();
        const glm::vec2 a = Measure(head);
        const glm::vec2 b = Measure(detail);
        const glm::vec2 size{ std::max(a.x, b.x) + 2.0f * pad, a.y + b.y + 1.5f * pad };
        const glm::vec2 origin = point - glm::vec2{ size.x * 0.5f, size.y + 10.0f * m_scale };
        const glm::vec4& edge = on ? m_theme.accent : m_theme.warn;
        m_ui.DrawRect(point - glm::vec2{ 1.0f * m_scale, 10.0f * m_scale }, { 2.0f * m_scale, 10.0f * m_scale },
                      m_theme.accent);
        Panel(origin, size, &edge);
        Text(head, origin + glm::vec2{ pad, pad * 0.5f }, m_theme.ink);
        Text(detail, origin + glm::vec2{ pad, pad * 0.5f + a.y }, on ? m_theme.dim : m_theme.warn);
    }

    float UiKit::ColumnsWidth(const std::vector<std::vector<std::string>>& rows) const
    {
        std::vector<float> widths;
        for (const std::vector<std::string>& row : rows)
        {
            widths.resize(std::max(widths.size(), row.size()), 0.0f);
            for (std::size_t c = 0; c < row.size(); ++c)
            {
                widths[c] = std::max(widths[c], Measure(row[c]).x);
            }
        }
        const std::vector<float> offsets = ColumnOffsets(widths, 12.0f * m_scale);
        return widths.empty() ? 0.0f : offsets.back() + widths.back();
    }

    float UiKit::Columns(glm::vec2 at, const std::vector<std::vector<std::string>>& rows,
                         const std::vector<glm::vec4>& colors)
    {
        // A proportional font: spaces can't align columns, so each column
        // starts where the widest cell before it ends.
        std::vector<float> widths;
        for (const std::vector<std::string>& row : rows)
        {
            widths.resize(std::max(widths.size(), row.size()), 0.0f);
            for (std::size_t c = 0; c < row.size(); ++c)
            {
                widths[c] = std::max(widths[c], Measure(row[c]).x);
            }
        }
        const std::vector<float> offsets = ColumnOffsets(widths, 12.0f * m_scale);
        const float lineHeight = LineHeight();
        for (std::size_t r = 0; r < rows.size(); ++r)
        {
            const glm::vec4& color = r < colors.size() ? colors[r] : m_theme.ink;
            for (std::size_t c = 0; c < rows[r].size(); ++c)
            {
                Text(rows[r][c], at + glm::vec2{ offsets[c], lineHeight * static_cast<float>(r) }, color);
            }
        }
        return lineHeight * static_cast<float>(rows.size());
    }

    glm::vec2 UiKit::Menu(glm::vec2 at, const std::vector<std::string>& items, int selected)
    {
        const float lineHeight = m_title.GetLineHeight() * m_scale;
        float width = 0.0f;
        for (const std::string& item : items)
        {
            width = std::max(width, m_ui.MeasureText(m_title, item, m_scale).x);
        }
        const glm::vec2 size{ width + 4.0f * Pad(), lineHeight * static_cast<float>(items.size()) + 2.0f * Pad() };
        Panel(at, size);
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const glm::vec2 row = at + glm::vec2{ 2.0f * Pad(), Pad() + lineHeight * static_cast<float>(i) };
            const bool chosen = static_cast<int>(i) == selected;
            if (chosen)
            {
                m_ui.DrawRect(row - glm::vec2{ Pad(), 0.0f }, { 3.0f * m_scale, lineHeight * 0.8f }, m_theme.accent);
            }
            TitleText(items[i], row, chosen ? m_theme.accent : m_theme.dim);
        }
        return size;
    }

    void UiKit::Toast(const std::string& text, float alpha, float textScale)
    {
        if (alpha <= 0.0f || text.empty())
        {
            return;
        }
        const float scale = m_scale * textScale;
        const float maxWidth = std::min(m_screen.x * 0.7f, 900.0f * scale);
        const std::string wrapped = m_ui.WrapText(m_title, text, maxWidth, scale);
        const glm::vec2 size = m_ui.MeasureText(m_title, wrapped, scale);
        const glm::vec2 padding{ 22.0f * scale, 12.0f * scale };
        const glm::vec2 position{ (m_screen.x - size.x) * 0.5f, m_screen.y - size.y - 70.0f * scale };
        glm::vec4 panel = m_theme.panel;
        panel.a *= alpha;
        glm::vec4 ink = m_theme.ink;
        ink.a *= alpha;
        m_ui.DrawRect(position - padding, size + padding * 2.0f, panel);
        m_ui.DrawText(m_title, wrapped, position, ink, scale);
    }
}
