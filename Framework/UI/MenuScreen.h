#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Atom
{
    class Input;
}

namespace AtomFramework
{
    class UiKit;

    // A title or pause menu (v0.0.14, M90): a heading, a line under it,
    // a list of choices. Up/Down (or W/S) move, Enter or Space chooses.
    // The navigation is pure (Navigate) so it is tested without a window.
    class MenuScreen
    {
    public:
        MenuScreen() = default;
        MenuScreen(std::string title, std::string subtitle, std::vector<std::string> items);

        // `move`: -1 up, +1 down (wraps). Returns the item chosen.
        std::optional<int> Navigate(int move, bool choose);
        std::optional<int> Update(const Atom::Input& input);

        // Over a darkened frame: the title large, the list under it, the
        // keys along the bottom.
        void Draw(UiKit& kit, float backdrop = 0.55f) const;

        int Selected() const { return m_selected; }
        void Select(int index);
        const std::vector<std::string>& Items() const { return m_items; }
        void SetItems(std::vector<std::string> items);

    private:
        std::string m_title;
        std::string m_subtitle;
        std::vector<std::string> m_items;
        int m_selected = 0;
    };
}
