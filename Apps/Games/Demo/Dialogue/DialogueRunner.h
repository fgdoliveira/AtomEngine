#pragma once

#include "Dialogue/Dialogue.h"

#include <string>
#include <string_view>
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
        const Dialogue* GetDialogue() const { return m_dialogue; }
        std::string GetVisibleText() const;
        // Choices whose flag conditions pass right now.
        std::vector<const DialogueChoice*> GetVisibleChoices() const;
        int GetSelection() const { return m_selection; }
        // Codepoints of the line shown so far.
        std::size_t RevealedCount() const { return static_cast<std::size_t>(m_revealed); }
        // The player finished this line early (Advance while revealing).
        bool WasSkipped() const { return m_skipped; }
        // Changes every time a line starts, even the same node again.
        int LineSerial() const { return m_lineSerial; }

        // How long the typewriter rests after a codepoint: a beat after
        // , ; : and a breath after . ! ? and the ellipsis; 0 otherwise.
        static float PauseAfter(std::string_view codepoint);

        float charactersPerSecond = 45.0f;

    private:
        void Enter(const std::string& nodeId);

        const Dialogue* m_dialogue = nullptr;
        GameState* m_gameState = nullptr;
        const DialogueNode* m_node = nullptr;
        State m_state = State::Inactive;
        float m_revealed = 0.0f; // characters shown so far
        float m_pause = 0.0f;    // seconds left resting at punctuation
        bool m_skipped = false;
        int m_lineSerial = 0;
        int m_selection = 0;
    };
}
