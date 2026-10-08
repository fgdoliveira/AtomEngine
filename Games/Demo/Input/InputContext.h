#pragma once

#include <SDL3/SDL_scancode.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace Atom
{
    class Input;
}

namespace Demo
{
    // Input contexts (M29). Gameplay asks for *actions* ("launch", "leave"),
    // never for keys. Which keys trigger an action depends on the active
    // *context*: Space confirms a line in a dialogue and fires a ball at a
    // pachinko machine; W walks while exploring and does nothing at the
    // machine. One map holds every binding, so rebinding would be one table.

    enum class InputAction
    {
        MoveForward,
        MoveBack,
        MoveLeft,
        MoveRight,
        Jog,
        Interact,
        Confirm,      // dialogue: continue / choose
        ChoiceUp,
        ChoiceDown,
        Launch,       // machine: hold to fire balls
        StrengthUp,   // machine: turn the launch knob
        StrengthDown,
        Leave,        // machine: stand up
        Buy,          // machine: tokens for balls (M33)
        // Character lab viewer (M36).
        OrbitLeft,
        OrbitRight,
        OrbitUp,
        OrbitDown,
        ZoomIn,
        ZoomOut,
        Clip1,
        Clip2,
        Clip3,
        Clip4,
        Slower,
        Faster,
        Pause,
        StepFrame,
        ToggleBindPose,
        ToggleSkeleton,
        ToggleWeights,
        ModeBlend,    // M37: walk/run blend
        ModeAnimator, // M37: the state machine demo
        BlendDown,
        BlendUp,
        Jump,         // M38: drive mode
        ToggleDrive,  // M38: Tab, viewer <-> drive
        ToggleLight,  // M44: the flashlight
        Count,
    };

    enum class InputContextId
    {
        Exploring,
        Dialogue,
        Machine,
        Viewer, // M36: the character lab's model viewer
        Driving, // M38: the lab's third-person drive mode
    };

    // "launch" <-> InputAction::Launch, for scripts and error messages.
    std::string_view ActionName(InputAction action);
    std::optional<InputAction> ActionFromName(std::string_view name);

    struct Binding
    {
        InputContextId context;
        InputAction action;
        SDL_Scancode key;
    };

    // The bindings of every context.
    class InputMap
    {
    public:
        static InputMap Default();
        const std::vector<Binding>& GetBindings() const { return m_bindings; }
        void Bind(InputContextId context, InputAction action, SDL_Scancode key)
        {
            m_bindings.push_back({ context, action, key });
        }

    private:
        std::vector<Binding> m_bindings;
    };

    // This frame's actions in the active context: held and just pressed,
    // from the keyboard (through the map) and from anything injected - the
    // test harness holds and presses actions the same way a player would.
    class ActionInput
    {
    public:
        // `keyDown(key)` / `keyPressed(key)` read the keyboard; separated from
        // Atom::Input so the mapping can be unit-tested without SDL.
        template <typename Down, typename Pressed>
        void Update(const InputMap& map, InputContextId context, Down keyDown, Pressed keyPressed)
        {
            m_context = context;
            for (std::size_t i = 0; i < m_held.size(); ++i)
            {
                m_held[i] = m_injectedHeld[i];
                m_pressed[i] = m_injectedPress[i] || (m_injectedHeld[i] && !m_wasInjectedHeld[i]);
                m_wasInjectedHeld[i] = m_injectedHeld[i];
                m_injectedPress[i] = false;
            }
            for (const Binding& binding : map.GetBindings())
            {
                if (binding.context != context)
                {
                    continue;
                }
                const auto index = static_cast<std::size_t>(binding.action);
                m_held[index] = m_held[index] || keyDown(binding.key);
                m_pressed[index] = m_pressed[index] || keyPressed(binding.key);
            }
        }
        void Update(const InputMap& map, InputContextId context, const Atom::Input& input);

        bool Held(InputAction action) const { return m_held[static_cast<std::size_t>(action)]; }
        bool Pressed(InputAction action) const { return m_pressed[static_cast<std::size_t>(action)]; }
        InputContextId GetContext() const { return m_context; }

        // Harness: hold an action until released, or press it for one frame.
        void Inject(InputAction action, bool held) { m_injectedHeld[static_cast<std::size_t>(action)] = held; }
        void InjectPress(InputAction action) { m_injectedPress[static_cast<std::size_t>(action)] = true; }

    private:
        static constexpr std::size_t ActionCount = static_cast<std::size_t>(InputAction::Count);
        std::array<bool, ActionCount> m_held{};
        std::array<bool, ActionCount> m_pressed{};
        std::array<bool, ActionCount> m_injectedHeld{};
        std::array<bool, ActionCount> m_wasInjectedHeld{};
        std::array<bool, ActionCount> m_injectedPress{};
        InputContextId m_context = InputContextId::Exploring;
    };
}
