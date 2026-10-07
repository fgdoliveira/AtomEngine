#include "UI/Font.h"
#include "Core/AssetLog.h"

#include "Renderer/Renderer.h"

#include <SDL3/SDL.h>

#include <stb_rect_pack.h>
#include <stb_truetype.h>

#include <iostream>
#include <iterator>
#include <vector>

namespace Atom
{
    namespace
    {
        constexpr int AtlasSize = 1024;

        // Curly quotes, dashes and the ellipsis show up in dialogue.
        constexpr int Punctuation[] = {
            0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026
        };

        std::uint64_t PairKey(std::uint32_t left, std::uint32_t right)
        {
            return (static_cast<std::uint64_t>(left) << 32) | right;
        }
    }

    std::unique_ptr<Font> Font::Load(
        Renderer& renderer,
        const std::string& path,
        float pixelHeight
    )
    {
        size_t size = 0;
        AssetLog::Opened(path);
        void* file = SDL_LoadFile(path.c_str(), &size);
        if (!file)
        {
            std::cerr << "Failed to load font '" << path << "': " << SDL_GetError() << '\n';
            return nullptr;
        }
        std::vector<unsigned char> ttf(
            static_cast<unsigned char*>(file),
            static_cast<unsigned char*>(file) + size);
        SDL_free(file);

        stbtt_fontinfo info{};
        if (!stbtt_InitFont(&info, ttf.data(), stbtt_GetFontOffsetForIndex(ttf.data(), 0)))
        {
            std::cerr << "Not a usable TrueType font: " << path << '\n';
            return nullptr;
        }

        // Rasterise every glyph into one coverage atlas. 2x horizontal
        // oversampling keeps small text crisp when it lands between pixels.
        std::vector<unsigned char> coverage(AtlasSize * AtlasSize);
        stbtt_pack_context pack{};
        stbtt_PackBegin(&pack, coverage.data(), AtlasSize, AtlasSize, 0, 1, nullptr);
        stbtt_PackSetOversampling(&pack, 2, 1);

        stbtt_packedchar ascii[95];
        stbtt_packedchar latin1[96];
        stbtt_packedchar punctuation[std::size(Punctuation)];

        stbtt_pack_range ranges[3]{};
        ranges[0].font_size = pixelHeight;
        ranges[0].first_unicode_codepoint_in_range = 32;
        ranges[0].num_chars = 95;
        ranges[0].chardata_for_range = ascii;
        ranges[1].font_size = pixelHeight;
        ranges[1].first_unicode_codepoint_in_range = 160;
        ranges[1].num_chars = 96;
        ranges[1].chardata_for_range = latin1;
        ranges[2].font_size = pixelHeight;
        ranges[2].array_of_unicode_codepoints = const_cast<int*>(Punctuation);
        ranges[2].num_chars = static_cast<int>(std::size(Punctuation));
        ranges[2].chardata_for_range = punctuation;

        const bool packed = stbtt_PackFontRanges(&pack, ttf.data(), 0, ranges, 3) != 0;
        stbtt_PackEnd(&pack);
        if (!packed)
        {
            std::cerr << "Font atlas too small for " << pixelHeight << " px: " << path << '\n';
            return nullptr;
        }

        auto font = std::unique_ptr<Font>(new Font());
        font->m_pixelHeight = pixelHeight;

        const float scale = stbtt_ScaleForPixelHeight(&info, pixelHeight);
        int ascent = 0, descent = 0, lineGap = 0;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        font->m_ascent = ascent * scale;
        font->m_lineHeight = (ascent - descent + lineGap) * scale;

        std::vector<std::uint32_t> codepoints;
        const auto addRange = [&](const stbtt_packedchar* chars, int count, auto codepointAt) {
            for (int i = 0; i < count; ++i)
            {
                const stbtt_packedchar& c = chars[i];
                const auto codepoint = static_cast<std::uint32_t>(codepointAt(i));
                font->m_glyphs[codepoint] = Glyph{
                    c.xoff, c.yoff, c.xoff2, c.yoff2,
                    c.x0 / static_cast<float>(AtlasSize), c.y0 / static_cast<float>(AtlasSize),
                    c.x1 / static_cast<float>(AtlasSize), c.y1 / static_cast<float>(AtlasSize),
                    c.xadvance
                };
                codepoints.push_back(codepoint);
            }
        };
        addRange(ascii, 95, [](int i) { return 32 + i; });
        addRange(latin1, 96, [](int i) { return 160 + i; });
        addRange(punctuation, static_cast<int>(std::size(Punctuation)),
            [](int i) { return Punctuation[i]; });

        // Pre-compute kerning so the header needn't expose stb types.
        for (const std::uint32_t left : codepoints)
        {
            for (const std::uint32_t right : codepoints)
            {
                const int kern = stbtt_GetCodepointKernAdvance(
                    &info, static_cast<int>(left), static_cast<int>(right));
                if (kern != 0)
                {
                    font->m_kerning[PairKey(left, right)] = kern * scale;
                }
            }
        }

        // Linear (not sRGB) texture: alpha is coverage, not a colour. One
        // mip level: text is drawn at the size it was baked, and with 2x
        // horizontal oversampling the GPU would otherwise pick level 1 (two
        // texels per pixel across) - half the resolution, a blur that was
        // in every piece of text until this fix.
        std::vector<std::uint8_t> rgba(AtlasSize * AtlasSize * 4, 255);
        for (int i = 0; i < AtlasSize * AtlasSize; ++i)
        {
            rgba[i * 4 + 3] = coverage[i];
        }
        font->m_atlas = renderer.CreateTexture(AtlasSize, AtlasSize, rgba.data(), false, false);
        if (!font->m_atlas)
        {
            return nullptr;
        }

        std::cout
            << "Loaded font '" << path << "' at " << pixelHeight << " px ("
            << font->m_glyphs.size() << " glyphs, "
            << font->m_kerning.size() << " kerning pairs)\n";
        return font;
    }

    const Font::Glyph* Font::FindGlyph(std::uint32_t codepoint) const
    {
        const auto found = m_glyphs.find(codepoint);
        return found != m_glyphs.end() ? &found->second : nullptr;
    }

    float Font::GetKerning(std::uint32_t left, std::uint32_t right) const
    {
        const auto found = m_kerning.find(PairKey(left, right));
        return found != m_kerning.end() ? found->second : 0.0f;
    }
}
