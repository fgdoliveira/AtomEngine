#include "Dialogue/DialogueView.h"

#include "Dialogue/DialogueRunner.h"
#include "UI/Font.h"
#include "UI/UIRenderer.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace Demo
{
    namespace
    {
        constexpr glm::vec4 PanelColor{ 0.03f, 0.03f, 0.035f, 0.86f };
        constexpr glm::vec4 RuleColor{ 0.55f, 0.18f, 0.12f, 0.9f };   // faded torii red
        constexpr glm::vec4 SpeakerColor{ 0.80f, 0.62f, 0.45f, 1.0f };
        constexpr glm::vec4 TextColor{ 0.93f, 0.91f, 0.85f, 1.0f };
        constexpr glm::vec4 ChoiceColor{ 0.62f, 0.62f, 0.58f, 1.0f };
        constexpr glm::vec4 SelectedColor{ 0.97f, 0.95f, 0.88f, 1.0f };
    }

    void DialogueView::Draw(
        Atom::UIRenderer& ui,
        const Atom::Font& font,
        const Atom::Font& smallFont,
        const DialogueRunner& runner,
        float scale,
        float time
    ) const
    {
        const DialogueNode* node = runner.GetNode();
        if (!runner.IsActive() || !node)
        {
            return;
        }

        const glm::vec2 screen = ui.GetScreenSize();
        const float margin = 60.0f * scale;
        const float padding = 26.0f * scale;
        const float width = std::min(screen.x - 2.0f * margin, 1100.0f * scale);
        const float textWidth = width - 2.0f * padding;
        const float textScale = scale * 0.9f;

        // Wrap the full line so text doesn't jump between rows as the
        // typewriter reveals it, then show only the revealed part.
        const std::string fullWrapped = ui.WrapText(font, node->text, textWidth, textScale);
        // Wrapping only turns spaces into newlines, so the revealed prefix
        // has the same byte length in both strings.
        const std::string shown = fullWrapped.substr(0, runner.GetVisibleText().size());

        const auto choices = runner.GetVisibleChoices();
        const bool waiting = runner.GetState() == DialogueRunner::State::WaitingForInput;
        const float lineHeight = font.GetLineHeight() * textScale;
        const float textBlock = ui.MeasureText(font, fullWrapped, textScale).y;
        const float choicesBlock = waiting ? choices.size() * lineHeight * 0.95f : 0.0f;
        const float speakerBlock = smallFont.GetLineHeight() * scale * 1.1f;
        const float height = padding * 2.0f + speakerBlock + textBlock
            + (choices.empty() ? lineHeight * 0.8f : choicesBlock + padding * 0.5f);

        const glm::vec2 origin{ (screen.x - width) * 0.5f, screen.y - height - margin * 0.6f };
        ui.DrawRect(origin, { width, height }, PanelColor);
        ui.DrawRect(origin, { width, 2.0f * scale }, RuleColor);

        glm::vec2 pen = origin + glm::vec2{ padding };
        ui.DrawText(smallFont, node->speaker, pen, SpeakerColor, scale);
        pen.y += speakerBlock;

        ui.DrawText(font, shown, pen, TextColor, textScale);
        pen.y += textBlock + padding * 0.5f;

        if (!waiting)
        {
            return;
        }

        if (choices.empty())
        {
            // Continue marker, gently pulsing.
            const float pulse = 0.55f + 0.45f * std::sin(time * 4.0f);
            const char* marker = "[E]";
            const glm::vec2 size = ui.MeasureText(smallFont, marker, scale);
            ui.DrawText(smallFont, marker,
                { origin.x + width - padding - size.x, origin.y + height - padding - size.y },
                { SelectedColor.r, SelectedColor.g, SelectedColor.b, pulse }, scale);
            return;
        }

        for (std::size_t i = 0; i < choices.size(); ++i)
        {
            const bool selected = static_cast<int>(i) == runner.GetSelection();
            const std::string label = (selected ? "\xC2\xBB  " : "    ")  // » marks the selection
                + std::to_string(i + 1) + ".  " + choices[i]->text;
            ui.DrawText(font, label, pen, selected ? SelectedColor : ChoiceColor, textScale);
            pen.y += lineHeight * 0.95f;
        }
    }
}
