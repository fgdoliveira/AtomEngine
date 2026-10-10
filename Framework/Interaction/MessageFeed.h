#pragma once

#include <string>

namespace Atom
{
    class Font;
    class UIRenderer;
}

namespace AtomFramework
{
    class UiKit;

    // One line of feedback at the bottom of the screen ("It's locked."),
    // fading out after a few seconds. A new message replaces the old one.
    class MessageFeed
    {
    public:
        void Show(std::string text, float seconds = 4.0f);
        void Update(float deltaSeconds);
        // The UI kit's toast (M89), its text `textScale` times the title size.
        void Draw(UiKit& kit, float textScale = 1.0f) const;

        const std::string& GetText() const { return m_text; }
        bool IsVisible() const { return m_remaining > 0.0f; }

    private:
        std::string m_text;
        float m_remaining = 0.0f;
    };
}
