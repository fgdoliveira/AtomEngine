#include "Interaction/MessageFeed.h"

#include "UI/Font.h"
#include "UI/UIRenderer.h"
#include "UI/UiKit.h"

#include <algorithm>

namespace AtomFramework
{
    void MessageFeed::Show(std::string text, float seconds)
    {
        m_text = std::move(text);
        m_remaining = seconds;
    }

    void MessageFeed::Update(float deltaSeconds)
    {
        m_remaining = std::max(0.0f, m_remaining - deltaSeconds);
    }

    void MessageFeed::Draw(UiKit& kit, float textScale) const
    {
        if (m_remaining <= 0.0f)
        {
            return;
        }

        // The kit's toast (M89): the shared panel and ink, fading out.
        const float alpha = std::clamp(m_remaining / 0.6f, 0.0f, 1.0f);
        kit.Toast(m_text, alpha, textScale);
    }
}
