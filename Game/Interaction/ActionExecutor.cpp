#include "Interaction/ActionExecutor.h"

#include "Interaction/MessageFeed.h"
#include "World/GameState.h"

#include <iostream>

namespace AtomGame
{
    namespace
    {
        // The classic "overloaded" helper: one lambda per alternative.
        template <typename... Ts>
        struct Overloaded : Ts...
        {
            using Ts::operator()...;
        };
    }

    void ExecuteAction(const Action& action, ActionContext& context)
    {
        std::visit(Overloaded{
            [&](const ShowMessage& show) {
                context.messages.Show(show.text);
            },
            [&](const SetFlag& set) {
                context.state.SetFlag(set.flag);
                if (!set.message.empty())
                {
                    context.messages.Show(set.message);
                }
                std::cout << "Flag set: " << set.flag << '\n';
            },
            [&](const StartDialogue& dialogue) {
                // Wired to the dialogue system in M11.
                std::cout << "StartDialogue '" << dialogue.dialogueId << "' (M11)\n";
                context.messages.Show("...");
            },
            [&](const ChangeLevel& change) {
                // Wired to the level manager in M12.
                std::cout << "ChangeLevel '" << change.level << "' at '"
                          << change.spawn << "' (M12)\n";
                context.messages.Show("The way is shut, for now.");
            },
        }, action);
    }
}
