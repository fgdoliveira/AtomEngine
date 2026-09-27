#pragma once

#include "Interaction/Actions.h"

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
    };

    void ExecuteAction(const Action& action, ActionContext& context);
}
