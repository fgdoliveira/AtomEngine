#pragma once

#include <string>
#include <variant>

namespace AtomGame
{
    // What happens when the player interacts with something. Each action is
    // plain data, so it can later be read from level files (M12); code that
    // performs it lives in one place (ActionExecutor), not in the objects.

    struct ShowMessage
    {
        std::string text;
    };

    struct SetFlag
    {
        std::string flag;
        std::string message; // optional feedback shown to the player
    };

    struct StartDialogue
    {
        std::string dialogueId; // M11
    };

    struct ChangeLevel
    {
        std::string level; // M12
        std::string spawn;
    };

    struct PlayAnimation
    {
        std::string entity; // M19: which entity's model plays
        std::string clip;
        std::string message; // optional
    };

    struct RunSequence
    {
        std::string id; // M26: one of the level's "sequences"
    };

    using Action = std::variant<ShowMessage, SetFlag, StartDialogue, ChangeLevel, PlayAnimation, RunSequence>;
}
