#pragma once

namespace Atom
{
    class Font;
    class UIRenderer;
}

namespace AtomGame
{
    class DialogueRunner;

    // Presentation only: reads the runner's state and draws it. Swapping
    // this for a different look never touches dialogue logic.
    class DialogueView
    {
    public:
        void Draw(
            Atom::UIRenderer& ui,
            const Atom::Font& font,
            const Atom::Font& smallFont,
            const DialogueRunner& runner,
            float scale,
            float time
        ) const;
    };
}
