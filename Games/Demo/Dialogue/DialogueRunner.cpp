#include "Dialogue/DialogueRunner.h"

#include "World/GameState.h"

#include <algorithm>
#include <iostream>

namespace AtomGame
{
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
    }

    void DialogueRunner::Start(const Dialogue& dialogue, GameState& state)
    {
        m_dialogue = &dialogue;
        m_gameState = &state;
        std::cout << "Dialogue '" << dialogue.id << "' started\n";
        Enter(dialogue.start);
    }

    void DialogueRunner::Enter(const std::string& nodeId)
    {
        m_node = (nodeId.empty() || nodeId == DialogueEnd) ? nullptr : m_dialogue->Find(nodeId);
        if (!m_node)
        {
            m_state = State::Ended;
            std::cout << "Dialogue '" << m_dialogue->id << "' ended\n";
            return;
        }

        if (!m_node->setsFlag.empty())
        {
            m_gameState->SetFlag(m_node->setsFlag);
            std::cout << "Flag set: " << m_node->setsFlag << '\n';
        }
        m_revealed = 0.0f;
        m_selection = 0;
        m_state = State::Revealing;
    }

    void DialogueRunner::Update(float deltaSeconds)
    {
        if (m_state != State::Revealing)
        {
            return;
        }
        m_revealed += charactersPerSecond * deltaSeconds;
        if (m_revealed >= static_cast<float>(CountCodepoints(m_node->text)))
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
            std::cout << "Flag set: " << choice.setsFlag << '\n';
        }
        Enter(choice.next);
    }

    void DialogueRunner::Close()
    {
        m_state = State::Inactive;
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
