#pragma once

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
    // Pure state: input comes in as a ViewerInput, the camera and the pose
    // to draw come out. DemoApp wires it to keys, the subject entity and
    // the renderer; tests drive it directly.

    struct ViewerClip
    {
        std::string name;
        float duration = 0.0f;
    };

    // One frame's requests, already translated from keys and mouse.
    struct ViewerInput
    {
        float orbitYawDegrees = 0.0f;   // + turns the camera to the right round the subject
        float orbitPitchDegrees = 0.0f; // + raises the camera
        float zoomSteps = 0.0f;         // + moves in
        int selectClip = -1;            // 0-based, -1 = none
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
        static constexpr std::array<float, 7> Speeds{ 0.1f, 0.25f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f };

        void Reset(const Orbit& orbit, std::vector<ViewerClip> clips);
        void Update(const ViewerInput& input, float deltaSeconds);

        // Clips, by index or name; returns false if there is no such clip.
        bool SelectClip(int clip);
        bool SelectClip(std::string_view name);
        // -1 while the bind pose is shown.
        int GetPoseClip() const { return m_bindPose ? -1 : m_clip; }
        int GetClip() const { return m_clip; }
        const std::string& GetClipName() const;
        float GetClipDuration() const;
        float GetTime() const { return m_time; }
        float GetSpeed() const { return Speeds[m_speedIndex]; }
        bool IsPaused() const { return m_paused; }

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
        glm::vec3 GetEye() const;
        float GetCameraYaw() const;
        float GetCameraPitch() const;

    private:
        void Advance(float seconds);

        Orbit m_orbit;
        std::vector<ViewerClip> m_clips;
        int m_clip = -1;
        float m_time = 0.0f;
        std::size_t m_speedIndex = 3; // 1x
        bool m_paused = false;
        bool m_bindPose = false;
        bool m_skeleton = false;
        bool m_weights = false;
    };
}
