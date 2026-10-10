#pragma once

#include "Dialogue/Dialogue.h"

#include <string>
#include <vector>

namespace AtomFramework
{
    class GameState; // v0.0.14: the framework's
}

namespace Demo
{
    using AtomFramework::GameState;

    // Walks a Dialogue as an explicit state machine. It never draws or reads
    // input devices: the game feeds it commands (Advance, MoveSelection,
    // Confirm) and a view renders whatever state it is in.
    //
    //   Inactive --Start--> Revealing --(text done / Advance)--> WaitingForInput
    //   WaitingForInput --Confirm (choice or continue)--> Revealing (next node)
    //                                                 \-> Ended (at "end")
    class DialogueRunner
    {
    public:
        enum class State
        {
            Inactive,
            Revealing,       // typewriter: text appearing
            WaitingForInput, // full line shown; choose or continue
            Ended,           // finished; the game closes it
        };

        void Start(const Dialogue& dialogue, GameState& state);
        void Update(float deltaSeconds);

        // E / Space: finish the typewriter, or continue a choiceless line.
        void Advance();
        void MoveSelection(int delta);
        void SelectIndex(int index);
        // Pick the highlighted choice (or continue if there are none).
        void Confirm();
        void Close();

        State GetState() const { return m_state; }
        bool IsActive() const { return m_state == State::Revealing || m_state == State::WaitingForInput; }

        const DialogueNode* GetNode() const { return m_node; }
        std::string GetVisibleText() const;
        // Choices whose flag conditions pass right now.
        std::vector<const DialogueChoice*> GetVisibleChoices() const;
        int GetSelection() const { return m_selection; }

        float charactersPerSecond = 45.0f;

    private:
        void Enter(const std::string& nodeId);

        const Dialogue* m_dialogue = nullptr;
        GameState* m_gameState = nullptr;
        const DialogueNode* m_node = nullptr;
        State m_state = State::Inactive;
        float m_revealed = 0.0f; // characters shown so far
        int m_selection = 0;
    };
}
