#pragma once

#include <map>
#include <memory>
#include <string>

namespace Atom
{
    class Font;
    class Renderer;
}

namespace AtomFramework
{
    // A typeface at every size the UI draws it (v0.0.14, M90). A font is a
    // bitmap atlas rasterised at one pixel height; drawn scaled, it blurs -
    // so the kit asks for the exact on-screen height and draws at 1:1. A
    // size is rasterised the first time it's asked for (a window resize,
    // fullscreen), then kept.
    class UiFonts
    {
    public:
        UiFonts(Atom::Renderer& renderer, std::string path);
        ~UiFonts();

        // At `pixelHeight` (rounded to whole pixels); null if the file can't load.
        const Atom::Font* Get(float pixelHeight);
        void Clear(); // before the renderer goes

    private:
        Atom::Renderer& m_renderer;
        std::string m_path;
        std::map<int, std::unique_ptr<Atom::Font>> m_sizes;
    };
}
