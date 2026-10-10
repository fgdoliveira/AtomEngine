#include "UI/UiFonts.h"

#include "UI/Font.h"

#include <algorithm>
#include <cmath>

namespace AtomFramework
{
    UiFonts::UiFonts(Atom::Renderer& renderer, std::string path) : m_renderer(renderer), m_path(std::move(path))
    {
    }

    UiFonts::~UiFonts() = default;

    const Atom::Font* UiFonts::Get(float pixelHeight)
    {
        const int size = std::clamp(static_cast<int>(std::lround(pixelHeight)), 6, 256);
        auto found = m_sizes.find(size);
        if (found == m_sizes.end())
        {
            found = m_sizes.emplace(size, Atom::Font::Load(m_renderer, m_path, static_cast<float>(size))).first;
        }
        return found->second.get();
    }

    void UiFonts::Clear()
    {
        m_sizes.clear();
    }
}
