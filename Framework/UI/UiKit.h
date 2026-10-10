#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace Atom
{
    class Font;
    class UIRenderer;
}

namespace AtomFramework
{
    // The look every app's player UI shares (v0.0.14, M89): one set of
    // colours (sRGB, straight alpha, as UIRenderer takes them), one margin,
    // one rule for scaling with the window. The accent is the developer
    // tools' too (DevTools::Initialize), so F10 and the game agree.
    struct UiTheme
    {
        glm::vec4 panel{ 0.04f, 0.04f, 0.05f, 0.80f };
        glm::vec4 ink{ 0.94f, 0.93f, 0.88f, 1.0f };   // text
        glm::vec4 dim{ 0.70f, 0.70f, 0.66f, 1.0f };   // secondary text, key hints
        glm::vec4 accent{ 0.93f, 0.62f, 0.32f, 1.0f }; // titles, what's selected or on
        glm::vec4 warn{ 0.85f, 0.35f, 0.30f, 1.0f };  // off, failed
        float margin = 16.0f; // from the window's edges, at scale 1
        float pad = 8.0f;     // inside a panel, at scale 1
    };
    const UiTheme& DefaultUiTheme();

    // Laid out for a 720-line window, scaled with its height (0.75x-2x).
    float UiScale(glm::vec2 screen);
    // Where each column starts, from widths measured and a gap between.
    std::vector<float> ColumnOffsets(const std::vector<float>& widths, float gap);

    // A frame's player UI: the theme's widgets on the engine's UIRenderer.
    // Immediate mode, like everything UIRenderer draws: made, drawn with
    // and dropped within a frame.
    class UiKit
    {
    public:
        UiKit(Atom::UIRenderer& ui, const Atom::Font& title, const Atom::Font& body,
              const UiTheme& theme = DefaultUiTheme());

        float Scale() const { return m_scale; }
        glm::vec2 Screen() const { return m_screen; }
        float Margin() const { return m_theme.margin * m_scale; }
        float Pad() const { return m_theme.pad * m_scale; }
        float LineHeight() const; // the body font's, scaled
        const UiTheme& Theme() const { return m_theme; }
        glm::vec2 Measure(std::string_view text) const; // in the body font
        Atom::UIRenderer& Renderer() { return m_ui; }
        const Atom::Font* TitleFont() const { return &m_title; }

        // A panel; `edge` draws the accent bar down its left side.
        void Panel(glm::vec2 origin, glm::vec2 size, const glm::vec4* edge = nullptr);
        void Text(std::string_view text, glm::vec2 at, const glm::vec4& color);
        void TitleText(std::string_view text, glm::vec2 at, const glm::vec4& color);

        // Bottom left: where you are (title font), what's here, the keys.
        void Caption(const std::string& title, const std::string& line, const std::string& keys);
        // Bottom centre: one line of keys on a panel.
        void HintBar(const std::string& keys, float alpha = 1.0f);
        // A label pinned above a point on screen: a heading and a detail.
        void Callout(glm::vec2 point, const std::string& head, const std::string& detail, bool on);
        // Rows of cells in measured columns from `at`; a colour per row.
        // Returns the height drawn.
        float Columns(glm::vec2 at, const std::vector<std::vector<std::string>>& rows,
                      const std::vector<glm::vec4>& colors);
        // The width Columns would take.
        float ColumnsWidth(const std::vector<std::vector<std::string>>& rows) const;
        // A vertical list with one item selected (the title and settings
        // screens): returns the size drawn.
        glm::vec2 Menu(glm::vec2 at, const std::vector<std::string>& items, int selected);
        // A message centred above the bottom edge, wrapped, fading with alpha.
        void Toast(const std::string& text, float alpha, float textScale = 1.0f);

    private:
        Atom::UIRenderer& m_ui;
        const Atom::Font& m_title;
        const Atom::Font& m_body;
        const UiTheme& m_theme;
        glm::vec2 m_screen;
        float m_scale;
    };
}
