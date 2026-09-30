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
            [&](const AddCounter& add) {
                if (!add.onceFlag.empty() && context.state.HasFlag(add.onceFlag))
                {
                    context.messages.Show(add.againMessage.empty() ? "..." : add.againMessage);
                    return;
                }
                context.state.AddToCounter(add.counter, add.amount);
                if (!add.onceFlag.empty())
                {
                    context.state.SetFlag(add.onceFlag);
                }
                if (!add.message.empty())
                {
                    context.messages.Show(add.message);
                }
                std::cout << "Counter " << add.counter << " += " << add.amount << '\n';
            },
            [&](const Exchange& exchange) {
                if (!context.state.Spend(exchange.counter, exchange.cost))
                {
                    context.messages.Show(exchange.shortMessage.empty() ? "Not enough." : exchange.shortMessage);
                    return;
                }
                context.state.SetFlag(exchange.flag);
                if (!exchange.message.empty())
                {
                    context.messages.Show(exchange.message);
                }
                std::cout << "Exchanged " << exchange.cost << " " << exchange.counter << " for " << exchange.flag << '\n';
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
