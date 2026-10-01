#include "Input/InputContext.h"

#include "Platform/Input.h"

namespace AtomGame
{
    namespace
    {
        constexpr std::string_view Names[] = {
            "move_forward", "move_back", "move_left", "move_right", "jog", "interact",
            "confirm", "choice_up", "choice_down", "launch", "strength_up", "strength_down", "leave", "buy",
            "orbit_left", "orbit_right", "orbit_up", "orbit_down", "zoom_in", "zoom_out",
            "clip_1", "clip_2", "clip_3", "clip_4", "slower", "faster", "pause", "step",
            "toggle_bind", "toggle_skeleton", "toggle_weights",
            "mode_blend", "mode_animator", "blend_down", "blend_up",
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
        map.Bind(C::Machine, InputAction::Buy, SDL_SCANCODE_B);
        map.Bind(C::Machine, InputAction::Buy, SDL_SCANCODE_RETURN);

        // The viewer: arrows orbit (the mouse too, while captured), the
        // number row picks a clip, keys named after what they show.
        map.Bind(C::Viewer, InputAction::OrbitLeft, SDL_SCANCODE_LEFT);
        map.Bind(C::Viewer, InputAction::OrbitRight, SDL_SCANCODE_RIGHT);
        map.Bind(C::Viewer, InputAction::OrbitUp, SDL_SCANCODE_UP);
        map.Bind(C::Viewer, InputAction::OrbitDown, SDL_SCANCODE_DOWN);
        map.Bind(C::Viewer, InputAction::ZoomIn, SDL_SCANCODE_PAGEUP);
        map.Bind(C::Viewer, InputAction::ZoomOut, SDL_SCANCODE_PAGEDOWN);
        map.Bind(C::Viewer, InputAction::Clip1, SDL_SCANCODE_1);
        map.Bind(C::Viewer, InputAction::Clip2, SDL_SCANCODE_2);
        map.Bind(C::Viewer, InputAction::Clip3, SDL_SCANCODE_3);
        map.Bind(C::Viewer, InputAction::Clip4, SDL_SCANCODE_4);
        map.Bind(C::Viewer, InputAction::Slower, SDL_SCANCODE_MINUS);
        map.Bind(C::Viewer, InputAction::Slower, SDL_SCANCODE_KP_MINUS);
        map.Bind(C::Viewer, InputAction::Faster, SDL_SCANCODE_EQUALS);
        map.Bind(C::Viewer, InputAction::Faster, SDL_SCANCODE_KP_PLUS);
        map.Bind(C::Viewer, InputAction::Pause, SDL_SCANCODE_SPACE);
        map.Bind(C::Viewer, InputAction::StepFrame, SDL_SCANCODE_PERIOD);
        map.Bind(C::Viewer, InputAction::ToggleBindPose, SDL_SCANCODE_B);
        map.Bind(C::Viewer, InputAction::ToggleSkeleton, SDL_SCANCODE_K);
        map.Bind(C::Viewer, InputAction::ToggleWeights, SDL_SCANCODE_W);
        map.Bind(C::Viewer, InputAction::ModeBlend, SDL_SCANCODE_5);
        map.Bind(C::Viewer, InputAction::ModeAnimator, SDL_SCANCODE_6);
        map.Bind(C::Viewer, InputAction::BlendDown, SDL_SCANCODE_LEFTBRACKET);
        map.Bind(C::Viewer, InputAction::BlendUp, SDL_SCANCODE_RIGHTBRACKET);
        return map;
    }

    void ActionInput::Update(const InputMap& map, InputContextId context, const Atom::Input& input)
    {
        Update(map, context,
            [&](SDL_Scancode key) { return input.IsKeyDown(key); },
            [&](SDL_Scancode key) { return input.WasKeyPressed(key); });
    }
}
