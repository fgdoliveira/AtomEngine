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
                if (!context.startDialogue || !context.startDialogue(dialogue.dialogueId))
                {
                    std::cerr << "No dialogue '" << dialogue.dialogueId << "'\n";
                    context.messages.Show("...");
                }
            },
            [&](const PlayAnimation& play) {
                if (!context.playAnimation || !context.playAnimation(play.entity, play.clip))
                {
                    std::cerr << "Cannot play '" << play.clip << "' on '" << play.entity << "'\n";
                    context.messages.Show("Nothing happens.");
                    return;
                }
                if (!play.message.empty())
                {
                    context.messages.Show(play.message);
                }
            },
            [&](const PlayMachine& machine) {
                if (!context.playMachine || !context.playMachine(machine))
                {
                    context.messages.Show("The machine doesn't respond.");
                }
            },
            [&](const RunSequence& run) {
                if (!context.runSequence || !context.runSequence(run.id))
                {
                    std::cerr << "Cannot run sequence '" << run.id << "'\n";
                }
            },
            [&](const ChangeLevel& change) {
                if (context.changeLevel)
                {
                    context.changeLevel(change.level, change.spawn);
                }
                else
                {
                    context.messages.Show("The way is shut.");
                }
            },
        }, action);
    }
}
