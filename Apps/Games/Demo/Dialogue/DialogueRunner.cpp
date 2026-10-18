#include "Dialogue/DialogueRunner.h"
#include "Core/Log.h"

#include "World/GameState.h"

#include <algorithm>
#include <iostream>

namespace Demo
{
    using namespace AtomFramework; // v0.0.14: the world layer (levels, world, interaction) lives there

    namespace
    {
        // Codepoint count, so the typewriter never splits a UTF-8 sequence.
        std::size_t CountCodepoints(const std::string& text)
        {
            return static_cast<std::size_t>(std::count_if(text.begin(), text.end(),
                [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
        }

        std::string FirstCodepoints(const std::string& text, std::size_t count)
        {
            std::size_t seen = 0;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80)
                {
                    if (seen == count)
                    {
                        return text.substr(0, i);
                    }
                    ++seen;
                }
            }
            return text;
        }

        // The codepoint at `index` (its UTF-8 bytes); empty past the end.
        std::string_view CodepointAt(const std::string& text, std::size_t index)
        {
            std::size_t seen = 0;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if ((static_cast<unsigned char>(text[i]) & 0xC0) == 0x80)
                {
                    continue;
                }
                if (seen++ == index)
                {
                    std::size_t end = i + 1;
                    while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80)
                    {
                        ++end;
                    }
                    return std::string_view(text).substr(i, end - i);
                }
            }
            return {};
        }
    }

    float DialogueRunner::PauseAfter(std::string_view codepoint)
    {
        if (codepoint == "," || codepoint == ";" || codepoint == ":")
        {
            return 0.12f;
        }
        if (codepoint == "." || codepoint == "!" || codepoint == "?" || codepoint == "\xE2\x80\xA6") // U+2026
        {
            return 0.3f;
        }
        return 0.0f;
    }

    void DialogueRunner::Start(const Dialogue& dialogue, GameState& state)
    {
        m_dialogue = &dialogue;
        m_gameState = &state;
        ATOM_LOG_GAME("Demo") << "Dialogue '" << dialogue.id << "' started\n";
        Enter(dialogue.start);
    }

    void DialogueRunner::Enter(const std::string& nodeId)
    {
        m_node = (nodeId.empty() || nodeId == DialogueEnd) ? nullptr : m_dialogue->Find(nodeId);
        if (!m_node)
        {
            m_state = State::Ended;
            ATOM_LOG_GAME("Demo") << "Dialogue '" << m_dialogue->id << "' ended\n";
            return;
        }

        if (!m_node->setsFlag.empty())
        {
            m_gameState->SetFlag(m_node->setsFlag);
            ATOM_LOG_GAME("Demo") << "Flag set: " << m_node->setsFlag << '\n';
        }
        m_revealed = 0.0f;
        m_pause = 0.0f;
        m_skipped = false;
        ++m_lineSerial;
        m_selection = 0;
        m_state = State::Revealing;
    }

    void DialogueRunner::Update(float deltaSeconds)
    {
        if (m_state != State::Revealing)
        {
            return;
        }
        // Resting after punctuation spends the frame's time first.
        if (m_pause > 0.0f)
        {
            const float rest = std::min(m_pause, deltaSeconds);
            m_pause -= rest;
            deltaSeconds -= rest;
            if (m_pause > 0.0f)
            {
                return;
            }
        }
        const std::size_t total = CountCodepoints(m_node->text);
        const std::size_t before = static_cast<std::size_t>(m_revealed);
        m_revealed += charactersPerSecond * deltaSeconds;
        // Stop at the first punctuation revealed this frame (not the last
        // one of the line: nothing follows it to wait for).
        const std::size_t after = std::min(static_cast<std::size_t>(m_revealed), total);
        for (std::size_t i = before; i < after && i + 1 < total; ++i)
        {
            if (const float pause = PauseAfter(CodepointAt(m_node->text, i)); pause > 0.0f)
            {
                m_revealed = static_cast<float>(i + 1);
                m_pause = pause;
                break;
            }
        }
        if (m_revealed >= static_cast<float>(total))
        {
            m_state = State::WaitingForInput;
        }
    }

    void DialogueRunner::Advance()
    {
        if (m_state == State::Revealing)
        {
            // First press shows the whole line; it doesn't skip it.
            m_revealed = static_cast<float>(CountCodepoints(m_node->text));
            m_pause = 0.0f;
            m_skipped = true;
            m_state = State::WaitingForInput;
        }
        else if (m_state == State::WaitingForInput && GetVisibleChoices().empty())
        {
            Enter(m_node->next);
        }
    }

    void DialogueRunner::MoveSelection(int delta)
    {
        const int count = static_cast<int>(GetVisibleChoices().size());
        if (m_state != State::WaitingForInput || count == 0)
        {
            return;
        }
        m_selection = (m_selection + delta % count + count) % count;
    }

    void DialogueRunner::SelectIndex(int index)
    {
        const int count = static_cast<int>(GetVisibleChoices().size());
        if (m_state == State::WaitingForInput && index >= 0 && index < count)
        {
            m_selection = index;
        }
    }

    void DialogueRunner::Confirm()
    {
        if (m_state != State::WaitingForInput)
        {
            Advance();
            return;
        }

        const std::vector<const DialogueChoice*> choices = GetVisibleChoices();
        if (choices.empty())
        {
            Advance();
            return;
        }

        const DialogueChoice& choice = *choices[std::clamp(m_selection, 0, static_cast<int>(choices.size()) - 1)];
        if (!choice.setsFlag.empty())
        {
            m_gameState->SetFlag(choice.setsFlag);
            ATOM_LOG_GAME("Demo") << "Flag set: " << choice.setsFlag << '\n';
        }
        Enter(choice.next);
    }

    void DialogueRunner::Close()
    {
        m_state = State::Inactive;
        m_pause = 0.0f;
        m_node = nullptr;
        m_dialogue = nullptr;
    }

    std::string DialogueRunner::GetVisibleText() const
    {
        if (!m_node)
        {
            return {};
        }
        return FirstCodepoints(m_node->text, static_cast<std::size_t>(m_revealed));
    }

    std::vector<const DialogueChoice*> DialogueRunner::GetVisibleChoices() const
    {
        std::vector<const DialogueChoice*> visible;
        if (!m_node || !m_gameState)
        {
            return visible;
        }
        for (const DialogueChoice& choice : m_node->choices)
        {
            const bool allowed = choice.requiredFlag.empty() || m_gameState->HasFlag(choice.requiredFlag);
            const bool blocked = !choice.forbiddenFlag.empty() && m_gameState->HasFlag(choice.forbiddenFlag);
            if (allowed && !blocked)
            {
                visible.push_back(&choice);
            }
        }
        return visible;
    }
}
