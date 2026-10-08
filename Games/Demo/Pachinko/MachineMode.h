#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Demo
{
    // Sitting down at a pachinko machine (M29), as an explicit state machine:
    //
    //   Inactive -> Entering -> Playing -> Leaving -> Inactive
    //
    // Entering eases the camera from the player's eye to a point in front of
    // the machine's screen, then fades the 2D game in over the whole window;
    // Leaving plays the same backwards. Pure (no rendering, no input): the
    // game reads the camera pose and the fade and does the rest.
    struct CameraPose
    {
        glm::vec3 position{ 0.0f };
        float yaw = 0.0f;   // radians
        float pitch = 0.0f; // radians
    };

    class MachineMode
    {
    public:
        enum class Phase
        {
            Inactive,
            Entering,
            Playing,
            Leaving,
        };

        static constexpr float MoveSeconds = 1.1f; // camera to the screen
        static constexpr float FadeSeconds = 0.35f; // screen to fullscreen

        void Enter(const CameraPose& from, const CameraPose& to);
        void Leave();                     // from Playing only
        void Update(float deltaSeconds);

        Phase GetPhase() const { return m_phase; }
        bool IsActive() const { return m_phase != Phase::Inactive; }
        CameraPose GetCamera() const;     // where the camera is now
        float GetFade() const;            // 0: the 3D view, 1: the 2D game fills the window
        const CameraPose& GetReturnPose() const { return m_from; }

    private:
        float Progress() const;           // 0..1 of the camera move

        Phase m_phase = Phase::Inactive;
        float m_time = 0.0f;              // in the current phase
        CameraPose m_from;
        CameraPose m_to;
    };

    // The largest whole-number scale at which a width x height image fits
    // in the window, centred: pixel art stays sharp (every source pixel
    // becomes an equal square), the rest is border. At least 1.
    struct PixelLayout
    {
        int scale = 1;
        glm::vec2 position{ 0.0f }; // top-left, window pixels
        glm::vec2 size{ 0.0f };
    };
    PixelLayout FitIntegerScale(glm::vec2 window, int width, int height);
}
