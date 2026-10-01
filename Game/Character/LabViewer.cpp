#include "Character/LabViewer.h"

#include "Character/Animator.h" // MakeSyncedBlend

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace AtomGame
{
    void LabViewer::Reset(const Orbit& orbit, std::vector<ViewerClip> clips,
                          std::string_view blendFrom, std::string_view blendTo)
    {
        *this = LabViewer{};
        m_orbit = orbit;
        m_orbit.distance = std::clamp(m_orbit.distance, MinDistance, MaxDistance);
        m_orbit.pitchDegrees = std::clamp(m_orbit.pitchDegrees, MinPitch, MaxPitch);
        m_clips = std::move(clips);
        m_clip = m_clips.empty() ? -1 : 0;
        for (std::size_t i = 0; i < m_clips.size(); ++i)
        {
            if (m_clips[i].name == blendFrom) m_blendFrom = static_cast<int>(i);
            if (m_clips[i].name == blendTo) m_blendTo = static_cast<int>(i);
        }
    }

    void LabViewer::Update(const ViewerInput& input, float deltaSeconds)
    {
        m_orbit.yawDegrees = std::remainder(m_orbit.yawDegrees + input.orbitYawDegrees, 360.0f);
        m_orbit.pitchDegrees = std::clamp(m_orbit.pitchDegrees + input.orbitPitchDegrees, MinPitch, MaxPitch);
        // Each notch moves a fixed share of the distance: zooming feels the
        // same close up and far away.
        m_orbit.distance = std::clamp(m_orbit.distance * std::pow(0.88f, input.zoomSteps),
                                      MinDistance, MaxDistance);

        if (input.selectMode >= 0)
        {
            SelectMode(static_cast<ViewerMode>(input.selectMode));
        }
        if (input.selectClip >= 0)
        {
            SelectClip(input.selectClip);
        }
        SetBlendWeight(m_blendWeight + input.blendDelta);
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

        m_animatorSeconds = 0.0f;
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

    bool LabViewer::SelectMode(ViewerMode mode)
    {
        if (mode == ViewerMode::Blend && (m_blendFrom < 0 || m_blendTo < 0))
        {
            return false;
        }
        if (mode != m_mode)
        {
            m_mode = mode;
            m_fade = 0.0f;
            m_phase = 0.0f;
            m_demoTime = 0.0f;
        }
        m_bindPose = false;
        return true;
    }

    bool LabViewer::SelectClip(int clip)
    {
        if (clip < 0 || clip >= static_cast<int>(m_clips.size()))
        {
            return false;
        }
        if (m_mode != ViewerMode::Clips)
        {
            m_mode = ViewerMode::Clips;
            m_fade = 0.0f;
        }
        else if (clip != m_clip && m_clip >= 0 && !m_bindPose)
        {
            // Crossfade: the old clip keeps playing while its weight falls.
            m_previousClip = m_clip;
            m_previousTime = m_time;
            m_fade = 1.0f;
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

    void LabViewer::SetBlendWeight(float weight)
    {
        m_blendWeight = std::clamp(weight, 0.0f, 1.0f);
    }

    float LabViewer::GetBlendCycle() const
    {
        if (m_blendFrom < 0 || m_blendTo < 0)
        {
            return 0.0f;
        }
        return MakeSyncedBlend(m_clips[m_blendFrom].duration, m_clips[m_blendTo].duration, m_blendWeight).duration;
    }

    void LabViewer::Advance(float seconds)
    {
        // In the viewer every clip loops, the jump included.
        const auto wrap = [](float time, float duration) {
            return duration > 0.0f ? std::fmod(time, duration) : 0.0f;
        };
        switch (m_mode)
        {
        case ViewerMode::Clips:
            m_time = wrap(m_time + seconds, GetClipDuration());
            if (m_fade > 0.0f)
            {
                m_previousTime = wrap(m_previousTime + seconds, m_clips[m_previousClip].duration);
                m_fade = std::max(0.0f, m_fade - seconds / CrossfadeSeconds);
            }
            break;
        case ViewerMode::Blend:
            if (const float cycle = GetBlendCycle(); cycle > 0.0f)
            {
                m_phase = std::fmod(m_phase + seconds / cycle, 1.0f);
            }
            break;
        case ViewerMode::StateMachine:
            m_animatorSeconds = seconds;
            m_demoTime = std::fmod(m_demoTime + seconds, DemoSeconds);
            break;
        }
    }

    std::vector<Atom::ClipSample> LabViewer::GetSamples() const
    {
        std::vector<Atom::ClipSample> samples;
        if (m_bindPose)
        {
            return samples;
        }
        if (m_mode == ViewerMode::Clips && m_clip >= 0)
        {
            if (m_fade > 0.0f)
            {
                samples.push_back({ m_previousClip, m_previousTime, m_fade });
            }
            samples.push_back({ m_clip, m_time, 1.0f - m_fade });
        }
        else if (m_mode == ViewerMode::Blend && m_blendFrom >= 0 && m_blendTo >= 0)
        {
            // Both cycles at the same phase: the feet agree.
            samples.push_back({ m_blendFrom, m_phase * m_clips[m_blendFrom].duration, 1.0f - m_blendWeight });
            samples.push_back({ m_blendTo, m_phase * m_clips[m_blendTo].duration, m_blendWeight });
        }
        return samples;
    }

    LabViewer::DemoParams LabViewer::Demo(float time)
    {
        // 0-2 stand, 2-6 speed up to 4.5 m/s, 6-8 run (a jump's flight
        // from 6.6 to 7.5),
        // 8-12 slow down, 12-14 stand.
        constexpr float TopSpeed = 4.5f;
        time = std::fmod(std::max(time, 0.0f), DemoSeconds);
        DemoParams params;
        if (time < 2.0f) params.speed = 0.0f;
        else if (time < 6.0f) params.speed = TopSpeed * (time - 2.0f) / 4.0f;
        else if (time < 8.0f) params.speed = TopSpeed;
        else if (time < 12.0f) params.speed = TopSpeed * (12.0f - time) / 4.0f;
        params.grounded = !(time >= 6.6f && time < 7.5f);
        return params;
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
