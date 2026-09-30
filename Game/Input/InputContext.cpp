#include "Input/InputContext.h"

#include "Platform/Input.h"

namespace AtomGame
{
    namespace
    {
        constexpr std::string_view Names[] = {
            "move_forward", "move_back", "move_left", "move_right", "jog", "interact",
            "confirm", "choice_up", "choice_down", "launch", "strength_up", "strength_down", "leave",
        };
        static_assert(std::size(Names) == static_cast<std::size_t>(InputAction::Count));
    }

    std::string_view ActionName(InputAction action)
    {
        return Names[static_cast<std::size_t>(action)];
    }

    std::optional<InputAction> ActionFromName(std::string_view name)
    {
        for (std::size_t i = 0; i < std::size(Names); ++i)
        {
            if (Names[i] == name)
            {
                return static_cast<InputAction>(i);
            }
        }
        return std::nullopt;
    }

    InputMap InputMap::Default()
    {
        using C = InputContextId;
        InputMap map;
        map.Bind(C::Exploring, InputAction::MoveForward, SDL_SCANCODE_W);
        map.Bind(C::Exploring, InputAction::MoveBack, SDL_SCANCODE_S);
        map.Bind(C::Exploring, InputAction::MoveLeft, SDL_SCANCODE_A);
        map.Bind(C::Exploring, InputAction::MoveRight, SDL_SCANCODE_D);
        map.Bind(C::Exploring, InputAction::Jog, SDL_SCANCODE_LSHIFT);
        map.Bind(C::Exploring, InputAction::Interact, SDL_SCANCODE_E);

        map.Bind(C::Dialogue, InputAction::Confirm, SDL_SCANCODE_E);
        map.Bind(C::Dialogue, InputAction::Confirm, SDL_SCANCODE_SPACE);
        map.Bind(C::Dialogue, InputAction::Confirm, SDL_SCANCODE_RETURN);
        map.Bind(C::Dialogue, InputAction::ChoiceUp, SDL_SCANCODE_W);
        map.Bind(C::Dialogue, InputAction::ChoiceUp, SDL_SCANCODE_UP);
        map.Bind(C::Dialogue, InputAction::ChoiceDown, SDL_SCANCODE_S);
        map.Bind(C::Dialogue, InputAction::ChoiceDown, SDL_SCANCODE_DOWN);

        map.Bind(C::Machine, InputAction::Launch, SDL_SCANCODE_SPACE);
        map.Bind(C::Machine, InputAction::StrengthUp, SDL_SCANCODE_UP);
        map.Bind(C::Machine, InputAction::StrengthUp, SDL_SCANCODE_D);
        map.Bind(C::Machine, InputAction::StrengthDown, SDL_SCANCODE_DOWN);
        map.Bind(C::Machine, InputAction::StrengthDown, SDL_SCANCODE_A);
        map.Bind(C::Machine, InputAction::Leave, SDL_SCANCODE_Q);
        map.Bind(C::Machine, InputAction::Leave, SDL_SCANCODE_BACKSPACE);
        return map;
    }

    void ActionInput::Update(const InputMap& map, InputContextId context, const Atom::Input& input)
    {
        Update(map, context,
            [&](SDL_Scancode key) { return input.IsKeyDown(key); },
            [&](SDL_Scancode key) { return input.WasKeyPressed(key); });
    }
}
