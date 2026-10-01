// The character lab (M36): DemoApp's model-viewer mode. LabViewer holds the
// state (orbit, clip, toggles); this file feeds it keys and the mouse, puts
// the camera where it says, poses the subject and draws the overlays.
#include "DemoApp.h"

#include "Assets/Model.h"
#include "Assets/Skin.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>

namespace AtomGame
{
    namespace
    {
        constexpr float OrbitKeyDegreesPerSecond = 90.0f;
        constexpr float MouseDegreesPerPixel = 0.25f;
        constexpr float ZoomKeyStepsPerSecond = 5.0f;
        constexpr float BlendSliderPerSecond = 0.5f;

        // 2000s tool look: a navy panel, a lighter title strip, pale text.
        constexpr glm::vec4 PanelColor{ 0.06f, 0.09f, 0.18f, 0.82f };
        constexpr glm::vec4 TitleColor{ 0.20f, 0.36f, 0.62f, 0.92f };
        constexpr glm::vec4 TextColor{ 0.90f, 0.93f, 0.97f, 1.0f };
        constexpr glm::vec4 DimTextColor{ 0.62f, 0.70f, 0.82f, 1.0f };
        constexpr glm::vec4 BoneColor{ 1.0f, 0.82f, 0.25f, 0.95f };
        constexpr glm::vec4 JointColor{ 1.0f, 0.35f, 0.20f, 1.0f };
    }

    Entity* DemoApp::FindLabSubject()
    {
        GameWorld* world = CurrentWorld();
        Entity* found = nullptr;
        if (world && m_lab)
        {
            world->ForEach([&](EntityId, Entity& entity) {
                if (entity.name == m_lab->subject)
                {
                    found = &entity;
                }
            });
        }
        return found;
    }

    void DemoApp::BeginLab(Level& level)
    {
        m_lab = level.GetData().lab;
        GetRenderer().SetSkinWeightsView(false);
        Entity* subject = FindLabSubject();
        if (!m_lab || !subject || !subject->renderable || !subject->renderable->model)
        {
            m_lab.reset();
            return;
        }

        // The viewer's clips are the model's, in file order (keys 1-4).
        const Atom::Model& model = *subject->renderable->model;
        std::vector<ViewerClip> clips;
        for (int i = 0; const Atom::AnimationClip* clip = model.GetClip(i); ++i)
        {
            clips.push_back(ViewerClip{ clip->name, clip->duration });
        }
        LabViewer::Orbit orbit;
        orbit.target = subject->position + m_lab->target;
        orbit.distance = m_lab->distance;
        orbit.yawDegrees = m_lab->yawDegrees;
        orbit.pitchDegrees = m_lab->pitchDegrees;
        m_viewer.Reset(orbit, std::move(clips));
        m_labScriptedParams = false;
        if (subject->animator)
        {
            // The viewer owns the clock: the level mustn't advance it too.
            subject->animator->SetEnabled(false);
        }
        ApplyLabPose();

        // The fade-in shows the orbit view, so that is where we "arrive".
        m_camera.SetPosition(m_viewer.GetEye());
        m_camera.SetRotation(m_viewer.GetCameraYaw(), m_viewer.GetCameraPitch());
        m_arrivalEye = m_viewer.GetEye();
        m_arrivalYaw = m_viewer.GetCameraYaw();
    }

    void DemoApp::UpdateLab(float deltaSeconds)
    {
        const Atom::Input& input = GetInput();
        const auto axis = [&](InputAction positive, InputAction negative) {
            return (m_actions.Held(positive) ? 1.0f : 0.0f) - (m_actions.Held(negative) ? 1.0f : 0.0f);
        };

        ViewerInput in;
        in.orbitYawDegrees = axis(InputAction::OrbitRight, InputAction::OrbitLeft)
            * OrbitKeyDegreesPerSecond * deltaSeconds;
        in.orbitPitchDegrees = axis(InputAction::OrbitUp, InputAction::OrbitDown)
            * OrbitKeyDegreesPerSecond * deltaSeconds;
        if (input.IsMouseCaptured())
        {
            // Like turning the subject with your hand: drag right, it turns
            // right (the camera goes left); drag up, you see it from above.
            in.orbitYawDegrees -= input.GetMouseDeltaX() * MouseDegreesPerPixel;
            in.orbitPitchDegrees += input.GetMouseDeltaY() * MouseDegreesPerPixel;
        }
        in.zoomSteps = input.GetWheelDelta()
            + axis(InputAction::ZoomIn, InputAction::ZoomOut) * ZoomKeyStepsPerSecond * deltaSeconds;

        const InputAction clipKeys[] = { InputAction::Clip1, InputAction::Clip2, InputAction::Clip3, InputAction::Clip4 };
        for (int i = 0; i < 4; ++i)
        {
            if (m_actions.Pressed(clipKeys[i]))
            {
                in.selectClip = i;
            }
        }
        if (m_actions.Pressed(InputAction::ModeBlend))
        {
            in.selectMode = static_cast<int>(ViewerMode::Blend);
        }
        if (m_actions.Pressed(InputAction::ModeAnimator))
        {
            in.selectMode = static_cast<int>(ViewerMode::StateMachine);
        }
        in.blendDelta = axis(InputAction::BlendUp, InputAction::BlendDown) * BlendSliderPerSecond * deltaSeconds;
        in.speedStep = (m_actions.Pressed(InputAction::Faster) ? 1 : 0) - (m_actions.Pressed(InputAction::Slower) ? 1 : 0);
        in.togglePause = m_actions.Pressed(InputAction::Pause);
        in.step = m_actions.Pressed(InputAction::StepFrame);
        in.toggleBindPose = m_actions.Pressed(InputAction::ToggleBindPose);
        in.toggleSkeleton = m_actions.Pressed(InputAction::ToggleSkeleton);
        in.toggleWeights = m_actions.Pressed(InputAction::ToggleWeights);

        m_viewer.Update(in, deltaSeconds);
        m_camera.SetPosition(m_viewer.GetEye());
        m_camera.SetRotation(m_viewer.GetCameraYaw(), m_viewer.GetCameraPitch());

        // The state machine demo: the script plays the gameplay that would
        // set the parameters (unless a test sets them itself).
        Entity* subject = FindLabSubject();
        if (subject && subject->animator && m_viewer.GetMode() == ViewerMode::StateMachine)
        {
            if (!m_labScriptedParams)
            {
                const LabViewer::DemoParams demo = LabViewer::Demo(m_viewer.GetDemoTime());
                subject->animator->SetParam("speed", demo.speed);
                subject->animator->SetParam("grounded", demo.grounded ? 1.0f : 0.0f);
            }
            subject->animator->Update(m_viewer.GetAnimatorSeconds());
        }
        ApplyLabPose();
    }

    void DemoApp::ApplyLabPose()
    {
        Entity* subject = FindLabSubject();
        if (!subject || !subject->renderable || !subject->renderable->model)
        {
            return;
        }
        // The viewer owns the clock: the level only draws what it's told.
        // Bind pose: no samples and no clip, so the rest pose is drawn.
        Animated& animated = subject->animated ? *subject->animated : subject->animated.emplace();
        animated.clip = -1;
        animated.playing = false;
        if (m_viewer.ShowsBindPose())
        {
            subject->poseSamples.clear();
        }
        else if (m_viewer.GetMode() == ViewerMode::StateMachine && subject->animator)
        {
            subject->poseSamples = subject->animator->GetSamples();
        }
        else
        {
            subject->poseSamples = m_viewer.GetSamples();
        }
        GetRenderer().SetSkinWeightsView(m_viewer.ShowsWeights());
    }

    bool DemoApp::SetClip(const std::string& entityName, const std::string& clipName)
    {
        if (m_lab && entityName == m_lab->subject)
        {
            const bool found = m_viewer.SelectClip(clipName);
            if (found)
            {
                // Show it now, not after a crossfade (tests check the pose).
                m_viewer.FinishCrossfade();
            }
            ApplyLabPose();
            return found;
        }
        GameWorld* world = CurrentWorld();
        bool found = false;
        if (world)
        {
            world->ForEach([&](EntityId, Entity& entity) {
                if (found || entity.name != entityName || !entity.renderable || !entity.renderable->model)
                {
                    return;
                }
                const int clip = entity.renderable->model->FindClip(clipName);
                if (clip < 0)
                {
                    return;
                }
                found = true;
                Animated& a = entity.animated ? *entity.animated : entity.animated.emplace();
                a.clip = clip;
                a.duration = entity.renderable->model->GetClip(clip)->duration;
                a.time = 0.0f;
                a.loop = true;
                a.playing = true;
            });
        }
        return found;
    }

    std::string DemoApp::ClipName(const std::string& entityName) const
    {
        const Entity* entity = const_cast<DemoApp*>(this)->FindEntity(entityName);
        if (!entity || !entity->renderable || !entity->renderable->model)
        {
            return {};
        }
        // Blended: the clip with the most weight.
        int shown = entity->animated ? entity->animated->clip : -1;
        float heaviest = 0.0f;
        for (const Atom::ClipSample& sample : entity->poseSamples)
        {
            if (sample.weight > heaviest)
            {
                heaviest = sample.weight;
                shown = sample.clip;
            }
        }
        const Atom::AnimationClip* clip = entity->renderable->model->GetClip(shown);
        return clip ? clip->name : std::string{};
    }

    bool DemoApp::SetAnimatorParam(const std::string& entityName, const std::string& param, float value)
    {
        GameWorld* world = CurrentWorld();
        bool found = false;
        if (world)
        {
            world->ForEach([&](EntityId, Entity& entity) {
                if (!found && entity.name == entityName && entity.animator)
                {
                    entity.animator->SetParam(param, value);
                    found = true;
                }
            });
        }
        if (found && m_lab && entityName == m_lab->subject)
        {
            m_labScriptedParams = true;
        }
        return found;
    }

    std::string DemoApp::AnimatorState(const std::string& entityName) const
    {
        const Entity* entity = const_cast<DemoApp*>(this)->FindEntity(entityName);
        return entity && entity->animator ? entity->animator->GetStateName() : std::string{};
    }

    void DemoApp::DrawSkeleton(const Entity& subject)
    {
        const Atom::Model* model = subject.renderable ? subject.renderable->model : nullptr;
        if (!model || !model->IsSkinned())
        {
            return;
        }
        const Atom::Skeleton& skeleton = model->GetSkeleton();
        const Atom::Skin& skin = skeleton.skins.front();

        Atom::Pose pose;
        model->SamplePose(subject.poseSamples, pose); // the rest pose if none
        const std::vector<glm::mat4> world = Atom::ComputeWorldMatrices(skeleton.parents, pose);
        const glm::mat4 transform = EntityModelTransform(subject);

        Atom::UIRenderer& ui = GetRenderer().GetUI();
        const glm::vec2 screen = ui.GetScreenSize();
        const glm::mat4 viewProjection =
            glm::perspective(m_camera.verticalFov, screen.x / screen.y, m_camera.nearPlane, m_camera.farPlane)
            * m_camera.GetViewMatrix();
        // Joint origins to pixels; nothing for points behind the camera.
        const auto project = [&](int node) -> std::optional<glm::vec2> {
            const glm::vec4 clip = viewProjection * transform * world[node][3];
            if (clip.w <= 0.01f)
            {
                return std::nullopt;
            }
            const glm::vec2 ndc{ clip.x / clip.w, clip.y / clip.w };
            return glm::vec2{ (ndc.x * 0.5f + 0.5f) * screen.x, (0.5f - ndc.y * 0.5f) * screen.y };
        };
        const float scale = std::clamp(screen.y / 720.0f, 0.75f, 2.0f);

        // Bones: each joint to its nearest ancestor that is also a joint,
        // drawn as a line of small squares (the UI draws rectangles).
        std::vector<char> isJoint(skeleton.parents.size(), 0);
        for (const int joint : skin.joints)
        {
            isJoint[joint] = 1;
        }
        for (const int joint : skin.joints)
        {
            int parent = skeleton.parents[joint];
            while (parent >= 0 && !isJoint[parent])
            {
                parent = skeleton.parents[parent];
            }
            const auto a = project(joint);
            const auto b = parent >= 0 ? project(parent) : std::nullopt;
            if (!a || !b)
            {
                continue;
            }
            const float length = glm::length(*b - *a);
            const int dots = std::max(1, static_cast<int>(length / (2.0f * scale)));
            const glm::vec2 dot{ 2.0f * scale };
            for (int i = 0; i <= dots; ++i)
            {
                const glm::vec2 p = glm::mix(*a, *b, static_cast<float>(i) / static_cast<float>(dots));
                ui.DrawRect(p - dot * 0.5f, dot, BoneColor);
            }
        }
        for (const int joint : skin.joints)
        {
            if (const auto p = project(joint))
            {
                const glm::vec2 outer{ 7.0f * scale };
                const glm::vec2 inner{ 5.0f * scale };
                ui.DrawRect(*p - outer * 0.5f, outer, { 0.05f, 0.05f, 0.08f, 0.9f });
                ui.DrawRect(*p - inner * 0.5f, inner, JointColor);
            }
        }
    }

    void DemoApp::DrawLabOverlay(float scale)
    {
        if (const Entity* subject = FindLabSubject(); subject && m_viewer.ShowsSkeleton())
        {
            DrawSkeleton(*subject);
        }
        if (!m_showHud)
        {
            return;
        }

        Atom::UIRenderer& ui = GetRenderer().GetUI();
        const glm::vec2 screen = ui.GetScreenSize();
        const float padding = 10.0f * scale;
        const glm::vec2 origin{ 16.0f * scale };

        // The panel: what is shown, and how.
        char clipLine[200];
        const char* paused = m_viewer.IsPaused() ? "   PAUSED" : "";
        const Entity* subject = FindLabSubject();
        const Animator* animator = subject && subject->animator ? &*subject->animator : nullptr;
        if (m_viewer.ShowsBindPose())
        {
            std::snprintf(clipLine, sizeof(clipLine), "Bind pose");
        }
        else if (m_viewer.GetMode() == ViewerMode::Blend)
        {
            std::snprintf(clipLine, sizeof(clipLine), "Blend  Walk %3.0f%% / Run %3.0f%%   cycle %.2f s   phase %.2f   %.2gx%s",
                (1.0f - m_viewer.GetBlendWeight()) * 100.0f, m_viewer.GetBlendWeight() * 100.0f,
                m_viewer.GetBlendCycle(), m_viewer.GetPhase(), m_viewer.GetSpeed(), paused);
        }
        else if (m_viewer.GetMode() == ViewerMode::StateMachine && animator)
        {
            std::snprintf(clipLine, sizeof(clipLine), "State machine  [%s]%s   speed %.1f m/s   grounded %s   %.2gx%s",
                animator->GetStateName().c_str(), animator->IsBlending() ? " (fading)" : "",
                animator->GetParam("speed"), animator->GetParam("grounded") != 0.0f ? "yes" : "no",
                m_viewer.GetSpeed(), paused);
        }
        else
        {
            std::snprintf(clipLine, sizeof(clipLine), "Clip %d  %-6s  %5.2f / %.2f s   %.2gx%s%s",
                m_viewer.GetClip() + 1, m_viewer.GetClipName().c_str(),
                m_viewer.GetTime(), m_viewer.GetClipDuration(), m_viewer.GetSpeed(),
                m_viewer.IsCrossfading() ? "   (crossfade)" : "", paused);
        }
        char toggles[160];
        std::snprintf(toggles, sizeof(toggles), "[B] bind %s   [K] skeleton %s   [W] weights %s",
            m_viewer.ShowsBindPose() ? "ON " : "off",
            m_viewer.ShowsSkeleton() ? "ON " : "off",
            m_viewer.ShowsWeights() ? "ON " : "off");

        const std::string title = "CHARACTER LAB  -  " + m_lab->subject;
        const glm::vec2 titleSize = ui.MeasureText(*m_smallFont, title, scale);
        const glm::vec2 lineSize = ui.MeasureText(*m_smallFont, clipLine, scale);
        const glm::vec2 toggleSize = ui.MeasureText(*m_smallFont, toggles, scale);
        const float width = std::max({ titleSize.x, lineSize.x, toggleSize.x, 300.0f * scale }) + 2.0f * padding;
        const float titleHeight = titleSize.y + padding;
        const bool slider = m_viewer.GetMode() == ViewerMode::Blend && !m_viewer.ShowsBindPose();
        const float sliderHeight = slider ? 10.0f * scale + padding : 0.0f;
        const float bodyHeight = lineSize.y + toggleSize.y + 2.5f * padding + sliderHeight;

        ui.DrawRect(origin, { width, titleHeight }, TitleColor);
        ui.DrawRect(origin + glm::vec2{ 0.0f, titleHeight }, { width, bodyHeight }, PanelColor);
        ui.DrawText(*m_smallFont, title, origin + glm::vec2{ padding, padding * 0.5f }, TextColor, scale);
        glm::vec2 at = origin + glm::vec2{ padding, titleHeight + padding };
        ui.DrawText(*m_smallFont, clipLine, at, TextColor, scale);
        at.y += lineSize.y + padding * 0.5f;
        if (slider)
        {
            // The walk/run slider: a track and a knob.
            const float track = width - 2.0f * padding;
            ui.DrawRect(at + glm::vec2{ 0.0f, 4.0f * scale }, { track, 2.0f * scale }, DimTextColor);
            const float knob = 8.0f * scale;
            ui.DrawRect(at + glm::vec2{ (track - knob) * m_viewer.GetBlendWeight(), 0.0f }, { knob, 10.0f * scale },
                BoneColor);
            at.y += sliderHeight;
        }
        ui.DrawText(*m_smallFont, toggles, at, DimTextColor, scale);

        // The help line along the bottom.
        const char* help = "1-4 clip   5 blend  [ ] slider   6 state machine   -/+ speed   Space pause   . step"
                           "   arrows / mouse orbit   wheel zoom";
        const glm::vec2 helpSize = ui.MeasureText(*m_smallFont, help, scale);
        const glm::vec2 helpAt{ (screen.x - helpSize.x) * 0.5f, screen.y - helpSize.y - 24.0f * scale };
        ui.DrawRect(helpAt - glm::vec2{ padding, padding * 0.5f }, helpSize + glm::vec2{ 2.0f * padding, padding },
            PanelColor);
        ui.DrawText(*m_smallFont, help, helpAt, DimTextColor, scale);
    }
}
