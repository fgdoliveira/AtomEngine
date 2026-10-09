// The character lab (M36-M38) at the Showcase's pavilion (v0.0.14; the
// demo's lab mode until then). LabViewer holds the viewer's state (orbit,
// clip, toggles); this file feeds it keys and the mouse, puts the camera
// where it says, poses the subject, drives it in drive mode and draws the
// overlays. The keys are the demo lab's.
#include "ShowcaseApp.h"

#include "Assets/Model.h"
#include "Assets/Skin.h"
#include "Physics/CollisionWorld.h"
#include "Platform/Input.h"
#include "Renderer/Renderer.h"
#include "UI/UIRenderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>

namespace Showcase
{
    namespace
    {
        constexpr float OrbitKeyDegreesPerSecond = 90.0f;
        constexpr float MouseDegreesPerPixel = 0.25f;
        constexpr float ZoomKeyStepsPerSecond = 5.0f;
        constexpr float BlendSliderPerSecond = 0.5f;

        // Drive mode (M38).
        constexpr float DriveWalkSpeed = 1.6f; // m/s; the blend's walk end is 1.4
        constexpr float DriveRunSpeed = 4.0f;  // the blend's run end
        constexpr float TurnRate = 10.0f;      // 1/s: how fast the character turns to face its way
        constexpr float ShoulderHeight = 1.3f; // the arm's pivot above the feet
        constexpr float DriveMouseDegreesPerPixel = 0.2f;

        // The shortest signed angle from a to b (radians).
        float AngleTo(float from, float to)
        {
            return std::remainder(to - from, 6.2831853f);
        }

        // 2000s tool look: a navy panel, a lighter title strip, pale text.
        constexpr glm::vec4 PanelColor{ 0.06f, 0.09f, 0.18f, 0.82f };
        constexpr glm::vec4 TitleColor{ 0.20f, 0.36f, 0.62f, 0.92f };
        constexpr glm::vec4 TextColor{ 0.90f, 0.93f, 0.97f, 1.0f };
        constexpr glm::vec4 DimTextColor{ 0.62f, 0.70f, 0.82f, 1.0f };
        constexpr glm::vec4 BoneColor{ 1.0f, 0.82f, 0.25f, 0.95f };
        constexpr glm::vec4 JointColor{ 1.0f, 0.35f, 0.20f, 1.0f };
    }

    // A key, or the same action held / pressed by a scenario.
    bool ShowcaseApp::LabKeyDown(const char* action, SDL_Scancode a, SDL_Scancode b) const
    {
        const Atom::Input& input = const_cast<ShowcaseApp*>(this)->GetInput();
        return input.IsKeyDown(a) || (b != SDL_SCANCODE_UNKNOWN && input.IsKeyDown(b))
            || std::find(m_heldActions.begin(), m_heldActions.end(), action) != m_heldActions.end();
    }

    bool ShowcaseApp::LabKeyPressed(const char* action, SDL_Scancode a, SDL_Scancode b) const
    {
        const Atom::Input& input = const_cast<ShowcaseApp*>(this)->GetInput();
        return input.WasKeyPressed(a) || (b != SDL_SCANCODE_UNKNOWN && input.WasKeyPressed(b))
            || std::find(m_pressedActions.begin(), m_pressedActions.end(), action) != m_pressedActions.end();
    }

    Entity* ShowcaseApp::LabSubject()
    {
        const Level* level = m_levels ? m_levels->GetLevel() : nullptr;
        return level && level->GetData().lab ? FindEntity(level->GetData().lab->subject) : nullptr;
    }

    void ShowcaseApp::BeginLab()
    {
        Entity* subject = LabSubject();
        if (!subject || !subject->renderable || !subject->renderable->model)
        {
            return;
        }
        const LevelLab& lab = *m_levels->GetLevel()->GetData().lab;
        m_labHome = subject->position;
        m_labHomeYaw = subject->renderable->yaw;

        // The viewer's clips are the model's, in file order (keys 1-4).
        const Atom::Model& model = *subject->renderable->model;
        std::vector<ViewerClip> clips;
        for (int i = 0; const Atom::AnimationClip* clip = model.GetClip(i); ++i)
        {
            clips.push_back(ViewerClip{ clip->name, clip->duration });
        }
        LabViewer::Orbit orbit;
        orbit.target = subject->position + lab.target;
        orbit.distance = lab.distance;
        orbit.yawDegrees = lab.yawDegrees;
        orbit.pitchDegrees = lab.pitchDegrees;
        m_viewer.Reset(orbit, std::move(clips));
        m_labScriptedParams = false;
        if (subject->animator)
        {
            // The viewer owns the clock: the level mustn't advance it too.
            subject->animator->SetEnabled(false);
        }
        m_mode = Mode::Viewing;
        ApplyLabPose();
        m_camera.SetPosition(m_viewer.GetEye());
        m_camera.SetRotation(m_viewer.GetCameraYaw(), m_viewer.GetCameraPitch());
    }

    void ShowcaseApp::EndLab()
    {
        // The character goes back to its spot and its loop; the camera to
        // the player, who never moved.
        if (Entity* subject = LabSubject(); subject && subject->renderable)
        {
            subject->position = m_labHome;
            subject->renderable->yaw = m_labHomeYaw;
            subject->poseSamples.clear();
            if (subject->animated)
            {
                subject->animated.reset();
            }
            if (subject->animator)
            {
                subject->animator->ForceState("idle");
                subject->animator->SetEnabled(true);
            }
        }
        GetRenderer().SetSkinWeightsView(false);
        m_mode = Mode::Walking;
        m_player.Teleport(m_player.GetFeetPosition(), m_camera);
        Face(LabSubject() ? LabSubject()->name : std::string{});
    }

    void ShowcaseApp::UpdateLab(float deltaSeconds)
    {
        const Atom::Input& input = GetInput();
        const auto axis = [&](bool positive, bool negative) { return (positive ? 1.0f : 0.0f) - (negative ? 1.0f : 0.0f); };

        ViewerInput in;
        in.orbitYawDegrees = axis(LabKeyDown("orbit_right", SDL_SCANCODE_RIGHT), LabKeyDown("orbit_left", SDL_SCANCODE_LEFT))
            * OrbitKeyDegreesPerSecond * deltaSeconds;
        in.orbitPitchDegrees = axis(LabKeyDown("orbit_up", SDL_SCANCODE_UP), LabKeyDown("orbit_down", SDL_SCANCODE_DOWN))
            * OrbitKeyDegreesPerSecond * deltaSeconds;
        if (input.IsMouseCaptured())
        {
            // Like turning the subject with your hand: drag right, it turns
            // right (the camera goes left); drag up, you see it from above.
            in.orbitYawDegrees -= input.GetMouseDeltaX() * MouseDegreesPerPixel;
            in.orbitPitchDegrees += input.GetMouseDeltaY() * MouseDegreesPerPixel;
        }
        in.zoomSteps = input.GetWheelDelta()
            + axis(LabKeyDown("zoom_in", SDL_SCANCODE_PAGEUP), LabKeyDown("zoom_out", SDL_SCANCODE_PAGEDOWN))
                * ZoomKeyStepsPerSecond * deltaSeconds;
        constexpr SDL_Scancode clipKeys[] = { SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4 };
        constexpr const char* clipActions[] = { "clip_1", "clip_2", "clip_3", "clip_4" };
        for (int i = 0; i < 4; ++i)
        {
            if (LabKeyPressed(clipActions[i], clipKeys[i]))
            {
                in.selectClip = i;
            }
        }
        if (LabKeyPressed("mode_blend", SDL_SCANCODE_5))
        {
            in.selectMode = static_cast<int>(ViewerMode::Blend);
        }
        if (LabKeyPressed("mode_animator", SDL_SCANCODE_6))
        {
            in.selectMode = static_cast<int>(ViewerMode::StateMachine);
        }
        in.blendDelta = axis(LabKeyDown("blend_up", SDL_SCANCODE_X, SDL_SCANCODE_RIGHTBRACKET),
                             LabKeyDown("blend_down", SDL_SCANCODE_Z, SDL_SCANCODE_LEFTBRACKET))
            * BlendSliderPerSecond * deltaSeconds;
        in.speedStep = (LabKeyPressed("faster", SDL_SCANCODE_EQUALS, SDL_SCANCODE_KP_PLUS) ? 1 : 0)
            - (LabKeyPressed("slower", SDL_SCANCODE_MINUS, SDL_SCANCODE_KP_MINUS) ? 1 : 0);
        in.togglePause = LabKeyPressed("pause", SDL_SCANCODE_SPACE);
        in.step = LabKeyPressed("step", SDL_SCANCODE_PERIOD);
        in.toggleBindPose = LabKeyPressed("toggle_bind", SDL_SCANCODE_B);
        in.toggleSkeleton = LabKeyPressed("toggle_skeleton", SDL_SCANCODE_K);
        in.toggleWeights = LabKeyPressed("toggle_weights", SDL_SCANCODE_W);

        m_viewer.Update(in, deltaSeconds);
        m_camera.SetPosition(m_viewer.GetEye());
        m_camera.SetRotation(m_viewer.GetCameraYaw(), m_viewer.GetCameraPitch());

        // The state machine demo: the script plays the gameplay that would
        // set the parameters (unless a test sets them itself).
        Entity* subject = LabSubject();
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

    void ShowcaseApp::ApplyLabPose()
    {
        Entity* subject = LabSubject();
        if (!subject || !subject->renderable || !subject->renderable->model || m_mode == Mode::Driving)
        {
            return; // the drive poses the subject from its animator
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

    void ShowcaseApp::BeginDrive()
    {
        Entity* subject = LabSubject();
        if (!subject || !subject->animator)
        {
            return; // nothing to drive
        }
        // The character at 0.8 scale: a slimmer, shorter body than the player's.
        m_driveBody.radius = 0.32f;
        m_driveBody.bodyHeight = 1.7f;
        m_driveBody.stepHeight = 0.3f;
        m_driveBody.walkSpeed = DriveWalkSpeed;
        m_driveBody.jogSpeed = DriveRunSpeed;
        m_driveBody.Place(subject->position);
        // The arm starts behind it, as third-person games do: W walks the
        // way it faces. (Yaw 0 puts the arm on +Z; it faces +Z at yaw 0.)
        m_arm.Reset(glm::degrees(subject->renderable->yaw) + 180.0f, 15.0f);
        subject->animator->ForceState("idle");
        subject->animator->SetParam("speed", 0.0f);
        subject->animator->SetParam("grounded", 1.0f);
        subject->animator->TakeEvents();
        m_mode = Mode::Driving;
        UpdateDrive(0.0f);
    }

    void ShowcaseApp::EndDrive()
    {
        // Back to the viewer, orbiting the subject wherever it stands.
        if (const Entity* subject = LabSubject())
        {
            LabViewer::Orbit orbit = m_viewer.GetOrbit();
            orbit.target = subject->position + m_levels->GetLevel()->GetData().lab->target;
            orbit.yawDegrees = m_arm.GetYawDegrees();
            m_viewer.SetOrbit(orbit);
        }
        m_mode = Mode::Viewing;
        ApplyLabPose();
        m_camera.SetPosition(m_viewer.GetEye());
        m_camera.SetRotation(m_viewer.GetCameraYaw(), m_viewer.GetCameraPitch());
    }

    void ShowcaseApp::UpdateDrive(float deltaSeconds)
    {
        Entity* subject = LabSubject();
        if (!subject || !subject->animator || !subject->renderable)
        {
            m_mode = Mode::Viewing;
            return;
        }
        if (LabKeyPressed("toggle_skeleton", SDL_SCANCODE_K))
        {
            m_viewer.SetSkeleton(!m_viewer.ShowsSkeleton());
        }

        // The camera first: movement is relative to where it looks.
        const Atom::Input& input = GetInput();
        const auto axis = [&](bool positive, bool negative) { return (positive ? 1.0f : 0.0f) - (negative ? 1.0f : 0.0f); };
        float yaw = -axis(LabKeyDown("orbit_right", SDL_SCANCODE_RIGHT), LabKeyDown("orbit_left", SDL_SCANCODE_LEFT))
            * OrbitKeyDegreesPerSecond * deltaSeconds;
        float pitch = axis(LabKeyDown("orbit_up", SDL_SCANCODE_UP), LabKeyDown("orbit_down", SDL_SCANCODE_DOWN))
            * OrbitKeyDegreesPerSecond * deltaSeconds;
        if (input.IsMouseCaptured())
        {
            yaw -= input.GetMouseDeltaX() * DriveMouseDegreesPerPixel;
            pitch += input.GetMouseDeltaY() * DriveMouseDegreesPerPixel;
        }
        m_arm.Orbit(yaw, pitch);
        const float cameraYaw = m_arm.GetCameraYaw();
        const glm::vec3 forward{ std::sin(cameraYaw), 0.0f, -std::cos(cameraYaw) };
        const glm::vec3 right{ std::cos(cameraYaw), 0.0f, std::sin(cameraYaw) };

        glm::vec2 move{ axis(LabKeyDown("move_right", SDL_SCANCODE_D), LabKeyDown("move_left", SDL_SCANCODE_A)),
                        axis(LabKeyDown("move_forward", SDL_SCANCODE_W), LabKeyDown("move_back", SDL_SCANCODE_S)) };
        if (glm::dot(move, move) > 1.0f)
        {
            move = glm::normalize(move);
        }
        const float speed = LabKeyDown("jog", SDL_SCANCODE_LSHIFT) ? DriveRunSpeed : DriveWalkSpeed;
        const glm::vec3 target = (forward * move.y + right * move.x) * speed;
        m_driveBody.Move(target, LabKeyPressed("jump", SDL_SCANCODE_SPACE), CurrentCollision(), deltaSeconds);

        // The character goes where the body is (its feet eased up steps)
        // and turns, smoothly, to face the way it moves.
        const glm::vec3 feet = m_driveBody.GetFeetPosition();
        subject->position = glm::vec3{ feet.x, m_driveBody.GetVisualFeetY(), feet.z };
        const glm::vec3 velocity = m_driveBody.GetVelocity();
        const float groundSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
        if (groundSpeed > 0.3f)
        {
            // Model forward is +Z: yaw 0 faces +Z, positive yaw turns to +X.
            const float facing = std::atan2(velocity.x, velocity.z);
            float& current = subject->renderable->yaw;
            current += AngleTo(current, facing) * (1.0f - std::exp(-TurnRate * deltaSeconds));
        }

        // The animator reads what the body does, not what the keys say:
        // walking into a wall is standing still.
        Animator& animator = *subject->animator;
        animator.SetParam("speed", groundSpeed);
        animator.SetParam("grounded", m_driveBody.IsGrounded() ? 1.0f : 0.0f);
        animator.Update(deltaSeconds);
        subject->poseSamples = animator.GetSamples();
        for (const std::string& event : animator.TakeEvents())
        {
            if (event == "foot" && m_driveBody.IsGrounded())
            {
                ++m_driveSteps;
            }
        }

        // The spring arm: pulled in by walls between the character and the camera.
        const Atom::CollisionWorld* world = CurrentCollision();
        m_arm.Update(subject->position + glm::vec3{ 0.0f, ShoulderHeight, 0.0f },
            [world](const glm::vec3& from, const glm::vec3& to) -> std::optional<float> {
                if (!world)
                {
                    return std::nullopt;
                }
                const auto hit = world->Raycast(from, to);
                return hit ? std::optional<float>{ hit->distance } : std::nullopt;
            }, deltaSeconds);
        m_camera.SetPosition(m_arm.GetEye());
        m_camera.SetRotation(m_arm.GetCameraYaw(), m_arm.GetCameraPitch());
        GetRenderer().SetSkinWeightsView(false);
    }

    void ShowcaseApp::DrawSkeleton(const Entity& subject)
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

    void ShowcaseApp::DrawLabOverlay()
    {
        const Entity* subject = LabSubject();
        if (subject && m_viewer.ShowsSkeleton())
        {
            DrawSkeleton(*subject);
        }
        if (!m_showHud)
        {
            return;
        }

        Atom::UIRenderer& ui = GetRenderer().GetUI();
        const glm::vec2 screen = ui.GetScreenSize();
        const float scale = std::clamp(screen.y / 720.0f, 0.75f, 2.0f);
        const float padding = 10.0f * scale;
        const glm::vec2 origin{ 16.0f * scale };

        // The panel: what is shown, and how.
        char clipLine[200];
        const char* paused = m_viewer.IsPaused() ? "   PAUSED" : "";
        const Animator* animator = subject && subject->animator ? &*subject->animator : nullptr;
        const bool driving = m_mode == Mode::Driving;
        if (driving && animator)
        {
            const glm::vec3 v = m_driveBody.GetVelocity();
            std::snprintf(clipLine, sizeof(clipLine), "Drive  [%s]   %.1f m/s   %s   arm %.1f m   steps %d",
                animator->GetStateName().c_str(), std::sqrt(v.x * v.x + v.z * v.z),
                m_driveBody.IsGrounded() ? "grounded" : "in the air", m_arm.GetCurrentLength(), m_driveSteps);
        }
        else if (m_viewer.ShowsBindPose())
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

        const std::string title = "CHARACTER LAB  -  " + (subject ? subject->name : std::string{}) + (driving ? "  -  DRIVE" : "");
        const glm::vec2 titleSize = ui.MeasureText(*m_smallFont, title, scale);
        const glm::vec2 lineSize = ui.MeasureText(*m_smallFont, clipLine, scale);
        const glm::vec2 toggleSize = ui.MeasureText(*m_smallFont, toggles, scale);
        const float width = std::max({ titleSize.x, lineSize.x, toggleSize.x, 300.0f * scale }) + 2.0f * padding;
        const float titleHeight = titleSize.y + padding;
        const bool slider = !driving && m_viewer.GetMode() == ViewerMode::Blend && !m_viewer.ShowsBindPose();
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
        const char* help = driving
            ? "WASD move   Shift run   Space jump   arrows / mouse camera   K skeleton   Tab viewer   E leave"
            : "1-4 clip   5 blend  Z/X slider   6 state machine   -/+ speed   Space pause   . step"
              "   arrows / mouse orbit   wheel zoom   Tab drive   E leave";
        const glm::vec2 helpSize = ui.MeasureText(*m_smallFont, help, scale);
        const glm::vec2 helpAt{ (screen.x - helpSize.x) * 0.5f, screen.y - helpSize.y - 24.0f * scale };
        ui.DrawRect(helpAt - glm::vec2{ padding, padding * 0.5f }, helpSize + glm::vec2{ 2.0f * padding, padding },
            PanelColor);
        ui.DrawText(*m_smallFont, help, helpAt, DimTextColor, scale);
    }

    // --- The harness's lab hooks ------------------------------------------

    bool ShowcaseApp::SetClip(const std::string& entityName, const std::string& clipName)
    {
        const Entity* subject = LabSubject();
        if (m_mode != Mode::Walking && subject && entityName == subject->name)
        {
            const bool found = m_viewer.SelectClip(clipName);
            if (found)
            {
                m_viewer.FinishCrossfade(); // show it now (tests check the pose)
            }
            ApplyLabPose();
            return found;
        }
        return false;
    }

    std::string ShowcaseApp::ClipName(const std::string& entityName) const
    {
        const Entity* entity = FindEntity(entityName);
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

    bool ShowcaseApp::SetAnimatorParam(const std::string& entityName, const std::string& param, float value)
    {
        Entity* entity = FindEntity(entityName);
        if (!entity || !entity->animator)
        {
            return false;
        }
        entity->animator->SetParam(param, value);
        if (const Entity* subject = LabSubject(); subject && subject == entity)
        {
            m_labScriptedParams = true;
        }
        return true;
    }

    bool ShowcaseApp::HoldAction(const std::string& action, bool held)
    {
        std::erase(m_heldActions, action);
        if (held)
        {
            m_heldActions.push_back(action);
        }
        return true;
    }

    bool ShowcaseApp::PressAction(const std::string& action)
    {
        m_pressedActions.push_back(action); // seen by this frame's lab, then gone
        return true;
    }
}
