#pragma once

#include "Renderer/Texture.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace Atom
{
    class Renderer;

    // A TrueType font rasterised once, at a fixed pixel height, into a glyph
    // atlas (white RGB, coverage in alpha). Covers ASCII, Latin-1 and common
    // typographic punctuation.
    class Font
    {
    public:
        struct Glyph
        {
            // Quad relative to the pen position on the baseline, in pixels.
            float x0, y0, x1, y1;
            // Atlas coordinates.
            float u0, v0, u1, v1;
            float advance;
        };

        static std::unique_ptr<Font> Load(
            Renderer& renderer,
            const std::string& path,
            float pixelHeight
        );

        const Glyph* FindGlyph(std::uint32_t codepoint) const;
        float GetKerning(std::uint32_t left, std::uint32_t right) const;

        float GetPixelHeight() const { return m_pixelHeight; }
        float GetAscent() const { return m_ascent; }
        float GetLineHeight() const { return m_lineHeight; }
        const Texture& GetAtlas() const { return *m_atlas; }

    private:
        std::unique_ptr<Texture> m_atlas;
        std::unordered_map<std::uint32_t, Glyph> m_glyphs;
        std::unordered_map<std::uint64_t, float> m_kerning; // only non-zero pairs
        float m_pixelHeight = 0.0f;
        float m_ascent = 0.0f;
        float m_lineHeight = 0.0f;
    };
}
