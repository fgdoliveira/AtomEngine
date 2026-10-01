#include "Character/LabViewer.h"

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace AtomGame
{
    void LabViewer::Reset(const Orbit& orbit, std::vector<ViewerClip> clips)
    {
        *this = LabViewer{};
        m_orbit = orbit;
        m_orbit.distance = std::clamp(m_orbit.distance, MinDistance, MaxDistance);
        m_orbit.pitchDegrees = std::clamp(m_orbit.pitchDegrees, MinPitch, MaxPitch);
        m_clips = std::move(clips);
        m_clip = m_clips.empty() ? -1 : 0;
    }

    void LabViewer::Update(const ViewerInput& input, float deltaSeconds)
    {
        m_orbit.yawDegrees = std::remainder(m_orbit.yawDegrees + input.orbitYawDegrees, 360.0f);
        m_orbit.pitchDegrees = std::clamp(m_orbit.pitchDegrees + input.orbitPitchDegrees, MinPitch, MaxPitch);
        // Each notch moves a fixed share of the distance: zooming feels the
        // same close up and far away.
        m_orbit.distance = std::clamp(m_orbit.distance * std::pow(0.88f, input.zoomSteps),
                                      MinDistance, MaxDistance);

        if (input.selectClip >= 0)
        {
            SelectClip(input.selectClip);
        }
        if (input.speedStep != 0)
        {
            const int index = static_cast<int>(m_speedIndex) + (input.speedStep > 0 ? 1 : -1);
            m_speedIndex = static_cast<std::size_t>(std::clamp(index, 0, static_cast<int>(Speeds.size()) - 1));
        }
        if (input.togglePause)
        {
            m_paused = !m_paused;
        }
        m_bindPose = m_bindPose != input.toggleBindPose;
        m_skeleton = m_skeleton != input.toggleSkeleton;
        m_weights = m_weights != input.toggleWeights;

        if (input.step)
        {
            // Stepping is for looking closely: it holds the clip still.
            m_paused = true;
            Advance(StepSeconds);
        }
        else if (!m_paused)
        {
            Advance(deltaSeconds * GetSpeed());
        }
    }

    bool LabViewer::SelectClip(int clip)
    {
        if (clip < 0 || clip >= static_cast<int>(m_clips.size()))
        {
            return false;
        }
        if (clip != m_clip)
        {
            m_clip = clip;
            m_time = 0.0f;
        }
        m_bindPose = false; // picking a clip means "show me it"
        return true;
    }

    bool LabViewer::SelectClip(std::string_view name)
    {
        for (std::size_t i = 0; i < m_clips.size(); ++i)
        {
            if (m_clips[i].name == name)
            {
                return SelectClip(static_cast<int>(i));
            }
        }
        return false;
    }

    const std::string& LabViewer::GetClipName() const
    {
        static const std::string none;
        return m_clip >= 0 ? m_clips[m_clip].name : none;
    }

    float LabViewer::GetClipDuration() const
    {
        return m_clip >= 0 ? m_clips[m_clip].duration : 0.0f;
    }

    void LabViewer::Advance(float seconds)
    {
        // In the viewer every clip loops, the jump included.
        const float duration = GetClipDuration();
        if (duration <= 0.0f)
        {
            m_time = 0.0f;
            return;
        }
        m_time = std::fmod(m_time + seconds, duration);
    }

    glm::vec3 LabViewer::GetEye() const
    {
        const float yaw = glm::radians(m_orbit.yawDegrees);
        const float pitch = glm::radians(m_orbit.pitchDegrees);
        // Yaw 0 puts the camera on +Z; positive yaw swings it round to +X.
        const glm::vec3 offset{
            std::sin(yaw) * std::cos(pitch),
            std::sin(pitch),
            std::cos(yaw) * std::cos(pitch) };
        return m_orbit.target + offset * m_orbit.distance;
    }

    float LabViewer::GetCameraYaw() const
    {
        // Atom::Camera: yaw 0 looks toward -Z, positive yaw toward +X. The
        // camera faces back along its offset.
        return glm::radians(-m_orbit.yawDegrees);
    }

    float LabViewer::GetCameraPitch() const
    {
        return glm::radians(-m_orbit.pitchDegrees);
    }
}
