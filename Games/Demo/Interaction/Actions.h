#pragma once

#include <glm/vec3.hpp>

#include <string>
#include <variant>

namespace Demo
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

    // Sit down at a pachinko machine (M29): the camera moves to `view`, then
    // the game takes over the machine's screen (`screen`, a scene material).
    struct PlayMachine
    {
        std::string screen;
        std::string machine; // the playfield, relative to Assets/ (M31)
        glm::vec3 viewPosition{ 0.0f };
        float viewYawDegrees = 0.0f;
        float viewPitchDegrees = 0.0f;
    };

    // Counters (M33): add to one (only once, if `onceFlag` is given), or
    // spend one for a flag - balls for a prize.
    struct AddCounter
    {
        std::string counter;
        int amount = 0;
        std::string message;
        std::string onceFlag;     // optional: set after the first time
        std::string againMessage; // shown instead once it's been given
    };

    struct Exchange
    {
        std::string counter;
        int cost = 0;
        std::string flag;          // what you get
        std::string message;
        std::string shortMessage;  // not enough
    };

    using Action = std::variant<ShowMessage, SetFlag, StartDialogue, ChangeLevel, PlayAnimation, RunSequence, PlayMachine,
                                AddCounter, Exchange>;
}
