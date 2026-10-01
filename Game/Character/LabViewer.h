#pragma once

#include "Assets/Skin.h" // ClipSample

#include <glm/vec3.hpp>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace AtomGame
{
    // The character lab's viewer (M36): a model viewer as 2000s tools had
    // them. An orbit camera turns around the subject; the clips play,
    // pause, step and change speed; overlays show what skinning does (the
    // skeleton, the weights, the bind pose).
    //
    // M37 adds three ways to animate: single clips (crossfaded when you
    // switch), a walk/run blend on a slider, and the subject's animation
    // state machine driven by a scripted demo.
    //
    // Pure state: input comes in as a ViewerInput, the camera and the pose
    // to draw come out. DemoApp wires it to keys, the subject entity and
    // the renderer; tests drive it directly.

    struct ViewerClip
    {
        std::string name;
        float duration = 0.0f;
    };

    enum class ViewerMode
    {
        Clips,        // 1-4: one clip at a time
        Blend,        // 5: walk/run by the slider
        StateMachine, // 6: the animator, fed by a demo script
    };

    // One frame's requests, already translated from keys and mouse.
    struct ViewerInput
    {
        float orbitYawDegrees = 0.0f;   // + turns the camera to the right round the subject
        float orbitPitchDegrees = 0.0f; // + raises the camera
        float zoomSteps = 0.0f;         // + moves in
        int selectClip = -1;            // 0-based, -1 = none
        int selectMode = -1;            // a ViewerMode, -1 = none
        float blendDelta = 0.0f;        // slider change (Blend mode)
        int speedStep = 0;              // +1 faster, -1 slower
        bool togglePause = false;
        bool step = false;              // one frame forward (pauses)
        bool toggleBindPose = false;
        bool toggleSkeleton = false;
        bool toggleWeights = false;
    };

    class LabViewer
    {
    public:
        struct Orbit
        {
            glm::vec3 target{ 0.0f, 0.9f, 0.0f };
            float distance = 3.6f;
            float yawDegrees = 0.0f;   // 0: camera on +Z looking toward -Z
            float pitchDegrees = 12.0f; // elevation above the target
        };

        static constexpr float MinDistance = 1.2f;
        static constexpr float MaxDistance = 9.0f;
        static constexpr float MinPitch = -10.0f;
        static constexpr float MaxPitch = 80.0f;
        static constexpr float StepSeconds = 1.0f / 30.0f; // one frame of the clips
        static constexpr float CrossfadeSeconds = 0.3f;
        static constexpr std::array<float, 7> Speeds{ 0.1f, 0.25f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f };

        // `blendFrom`/`blendTo` name the clips of the Blend mode (if absent,
        // the mode is unavailable).
        void Reset(const Orbit& orbit, std::vector<ViewerClip> clips,
                   std::string_view blendFrom = "Walk", std::string_view blendTo = "Run");
        void Update(const ViewerInput& input, float deltaSeconds);

        // Clips, by index or name; returns false if there is no such clip.
        // Switching from another clip crossfades into it.
        bool SelectClip(int clip);
        bool SelectClip(std::string_view name);
        bool SelectMode(ViewerMode mode);
        ViewerMode GetMode() const { return m_mode; }

        // -1 while the bind pose is shown.
        int GetPoseClip() const { return m_bindPose ? -1 : m_clip; }
        int GetClip() const { return m_clip; }
        const std::string& GetClipName() const;
        float GetClipDuration() const;
        float GetTime() const { return m_time; }
        float GetSpeed() const { return Speeds[m_speedIndex]; }
        bool IsPaused() const { return m_paused; }
        bool IsCrossfading() const { return m_fade > 0.0f; }
        void FinishCrossfade() { m_fade = 0.0f; }

        // Blend mode: the slider (0 = all walk, 1 = all run), where both
        // cycles are (0..1), and the blended cycle's length.
        float GetBlendWeight() const { return m_blendWeight; }
        void SetBlendWeight(float weight);
        float GetPhase() const { return m_phase; }
        float GetBlendCycle() const;

        // What to draw in the Clips and Blend modes (empty for the bind
        // pose; the StateMachine mode draws the animator's samples).
        std::vector<Atom::ClipSample> GetSamples() const;

        // StateMachine mode: how far the animator advances this frame (the
        // viewer's speed, pause and step apply), and the demo's clock.
        float GetAnimatorSeconds() const { return m_animatorSeconds; }
        float GetDemoTime() const { return m_demoTime; }
        // The demo script: stand, speed up to a run, jump at full speed,
        // slow down, stand; 14 s, then again.
        struct DemoParams
        {
            float speed = 0.0f;
            bool grounded = true;
        };
        static constexpr float DemoSeconds = 14.0f;
        static DemoParams Demo(float time);

        bool ShowsBindPose() const { return m_bindPose; }
        bool ShowsSkeleton() const { return m_skeleton; }
        bool ShowsWeights() const { return m_weights; }
        void SetBindPose(bool on) { m_bindPose = on; }
        void SetSkeleton(bool on) { m_skeleton = on; }
        void SetWeights(bool on) { m_weights = on; }
        void SetPaused(bool on) { m_paused = on; }

        // The camera: where it is and where it looks (yaw/pitch in radians,
        // in Atom::Camera's convention).
        const Orbit& GetOrbit() const { return m_orbit; }
        void SetOrbit(const Orbit& orbit) { m_orbit = orbit; }
        glm::vec3 GetEye() const;
        float GetCameraYaw() const;
        float GetCameraPitch() const;

    private:
        void Advance(float seconds);

        Orbit m_orbit;
        std::vector<ViewerClip> m_clips;
        ViewerMode m_mode = ViewerMode::Clips;
        int m_clip = -1;
        float m_time = 0.0f;
        int m_previousClip = -1; // fading out
        float m_previousTime = 0.0f;
        float m_fade = 0.0f;     // its weight
        int m_blendFrom = -1;
        int m_blendTo = -1;
        float m_blendWeight = 0.0f;
        float m_phase = 0.0f;
        float m_animatorSeconds = 0.0f;
        float m_demoTime = 0.0f;
        std::size_t m_speedIndex = 3; // 1x
        bool m_paused = false;
        bool m_bindPose = false;
        bool m_skeleton = false;
        bool m_weights = false;
    };
}
