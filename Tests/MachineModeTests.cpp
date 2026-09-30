#include "Input/InputContext.h"
#include "Pachinko/MachineMode.h"

#include <doctest/doctest.h>

#include <glm/trigonometric.hpp>

#include <cmath>
#include <ostream>
#include <set>
#include <string>

using namespace AtomGame;

namespace
{
    // Updates `actions` as if exactly the keys in `down` were held and those
    // in `pressed` went down this frame.
    void Frame(ActionInput& actions, InputContextId context, std::set<SDL_Scancode> down, std::set<SDL_Scancode> pressed = {})
    {
        actions.Update(InputMap::Default(), context,
            [&](SDL_Scancode key) { return down.contains(key); },
            [&](SDL_Scancode key) { return pressed.contains(key); });
    }
}

TEST_CASE("Input contexts: the same key means different things per mode")
{
    ActionInput actions;
    Frame(actions, InputContextId::Machine, { SDL_SCANCODE_SPACE }, { SDL_SCANCODE_SPACE });
    CHECK(actions.Held(InputAction::Launch));
    CHECK_FALSE(actions.Pressed(InputAction::Confirm));

    Frame(actions, InputContextId::Dialogue, { SDL_SCANCODE_SPACE }, { SDL_SCANCODE_SPACE });
    CHECK(actions.Pressed(InputAction::Confirm));
    CHECK_FALSE(actions.Held(InputAction::Launch));

    Frame(actions, InputContextId::Machine, { SDL_SCANCODE_W });
    CHECK_FALSE(actions.Held(InputAction::MoveForward)); // no walking at the machine
    Frame(actions, InputContextId::Exploring, { SDL_SCANCODE_W });
    CHECK(actions.Held(InputAction::MoveForward));
}

TEST_CASE("Input contexts: injected actions behave like keys")
{
    ActionInput actions;
    actions.Inject(InputAction::Launch, true);
    Frame(actions, InputContextId::Machine, {});
    CHECK(actions.Held(InputAction::Launch));
    CHECK(actions.Pressed(InputAction::Launch));   // pressed on the first frame only
    Frame(actions, InputContextId::Machine, {});
    CHECK(actions.Held(InputAction::Launch));
    CHECK_FALSE(actions.Pressed(InputAction::Launch));
    actions.Inject(InputAction::Launch, false);
    Frame(actions, InputContextId::Machine, {});
    CHECK_FALSE(actions.Held(InputAction::Launch));

    actions.InjectPress(InputAction::Leave);
    Frame(actions, InputContextId::Machine, {});
    CHECK(actions.Pressed(InputAction::Leave));
    Frame(actions, InputContextId::Machine, {});
    CHECK_FALSE(actions.Pressed(InputAction::Leave));

    CHECK(ActionFromName("launch") == InputAction::Launch);
    CHECK_FALSE(ActionFromName("jump").has_value());
    CHECK(ActionName(InputAction::StrengthUp) == "strength_up");
}

TEST_CASE("Machine mode: in, play, out, with the camera and fade in step")
{
    MachineMode mode;
    const CameraPose from{ { 0, 1.6f, 0 }, glm::radians(170.0f), 0.2f };
    const CameraPose to{ { 1, 1.4f, -2 }, glm::radians(-170.0f), 0.0f };
    mode.Enter(from, to);
    CHECK(mode.GetPhase() == MachineMode::Phase::Entering);
    CHECK(mode.GetFade() == 0.0f);

    mode.Update(MachineMode::MoveSeconds * 0.5f);
    // Halfway, turning the short way round (170 -> 180/-180 -> -170).
    CHECK(std::abs(std::remainder(mode.GetCamera().yaw - glm::radians(180.0f), glm::radians(360.0f))) < 0.01f);
    mode.Update(MachineMode::MoveSeconds * 0.5f + MachineMode::FadeSeconds + 0.01f);
    CHECK(mode.GetPhase() == MachineMode::Phase::Playing);
    CHECK(mode.GetFade() == 1.0f);
    CHECK(mode.GetCamera().position.z == doctest::Approx(-2.0f));

    mode.Leave();
    mode.Update(MachineMode::FadeSeconds * 0.5f);
    CHECK(mode.GetFade() < 1.0f);
    CHECK(mode.GetCamera().position.z == doctest::Approx(-2.0f)); // fades out before moving
    mode.Update(MachineMode::MoveSeconds + MachineMode::FadeSeconds);
    CHECK_FALSE(mode.IsActive());
    CHECK(mode.GetCamera().position.x == doctest::Approx(0.0f)); // back where it started
}

TEST_CASE("Integer scaling fits the largest whole multiple, centred")
{
    const PixelLayout hd = FitIntegerScale({ 1280, 720 }, 320, 240);
    CHECK(hd.scale == 3);
    CHECK(hd.size.x == 960.0f);
    CHECK(hd.position.x == 160.0f);
    CHECK(hd.position.y == 0.0f);
    CHECK(FitIntegerScale({ 1920, 1080 }, 320, 240).scale == 4);
    CHECK(FitIntegerScale({ 200, 150 }, 320, 240).scale == 1); // never below 1
}
