#include "UI/MenuScreen.h"

#include "UI/UiKit.h"

#include "Platform/Input.h"
#include "UI/Font.h"
#include "UI/UIRenderer.h"

#include <algorithm>

namespace AtomFramework
{
    MenuScreen::MenuScreen(std::string title, std::string subtitle, std::vector<std::string> items)
        : m_title(std::move(title)), m_subtitle(std::move(subtitle)), m_items(std::move(items))
    {
    }

    void MenuScreen::Select(int index)
    {
        m_selected = m_items.empty() ? 0 : std::clamp(index, 0, static_cast<int>(m_items.size()) - 1);
    }

    void MenuScreen::SetItems(std::vector<std::string> items)
    {
        m_items = std::move(items);
        Select(m_selected);
    }

    std::optional<int> MenuScreen::Navigate(int move, bool choose)
    {
        if (m_items.empty())
        {
            return std::nullopt;
        }
        const int count = static_cast<int>(m_items.size());
        m_selected = ((m_selected + move) % count + count) % count;
        return choose ? std::optional<int>(m_selected) : std::nullopt;
    }

    std::optional<int> MenuScreen::Update(const Atom::Input& input)
    {
        const auto pressed = [&](SDL_Scancode a, SDL_Scancode b) { return input.WasKeyPressed(a) || input.WasKeyPressed(b); };
        const int move = (pressed(SDL_SCANCODE_DOWN, SDL_SCANCODE_S) ? 1 : 0) - (pressed(SDL_SCANCODE_UP, SDL_SCANCODE_W) ? 1 : 0);
        const bool choose = pressed(SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE) || input.WasKeyPressed(SDL_SCANCODE_KP_ENTER);
        return Navigate(move, choose);
    }

    void MenuScreen::Draw(UiKit& kit, float backdrop) const
    {
        const glm::vec2 screen = kit.Screen();
        kit.Renderer().DrawRect({ 0.0f, 0.0f }, screen, { 0.0f, 0.0f, 0.0f, backdrop });
        const UiTheme& theme = kit.Theme();
        // Left third, a little above the middle: the title, a line, the list.
        const glm::vec2 at{ screen.x * 0.12f, screen.y * 0.28f };
        const float titleScale = 2.0f;
        // A band down the left behind the text, so it reads over any frame.
        float width = std::max(kit.Renderer().MeasureText(*kit.TitleFont(), m_title, kit.Scale() * titleScale).x,
                               kit.Measure(m_subtitle).x);
        for (const std::string& item : m_items)
        {
            width = std::max(width, kit.Renderer().MeasureText(*kit.TitleFont(), item, kit.Scale()).x);
        }
        kit.Renderer().DrawRect({ 0.0f, 0.0f }, { at.x + width + 4.0f * kit.Margin(), screen.y }, theme.panel);
        kit.Renderer().DrawText(*kit.TitleFont(), m_title, at, theme.ink, kit.Scale() * titleScale);
        const float titleHeight = kit.TitleFont()->GetLineHeight() * kit.Scale() * titleScale;
        kit.Text(m_subtitle, at + glm::vec2{ 4.0f * kit.Scale(), titleHeight }, theme.dim);
        kit.Menu(at + glm::vec2{ 0.0f, titleHeight + kit.LineHeight() * 2.0f }, m_items, m_selected);
        kit.HintBar("Up / Down choose    Enter select");
    }
}
