#pragma once

#include "Interaction/Actions.h"

#include <functional>
#include <string>

namespace AtomGame
{
    class GameState;
    class MessageFeed;

    // Performs actions. std::visit makes the compiler check that every
    // alternative of Action is handled: adding an action type without
    // handling it here is a build error, not a silent no-op.
    struct ActionContext
    {
        GameState& state;
        MessageFeed& messages;
        // Hooks into systems the executor shouldn't depend on directly.
        std::function<bool(const std::string& dialogueId)> startDialogue;
    };

    void ExecuteAction(const Action& action, ActionContext& context);
}
