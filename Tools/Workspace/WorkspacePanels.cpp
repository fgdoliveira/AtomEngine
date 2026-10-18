// The workspace's project panels (v0.0.19, M124): Assets, Hierarchy,
// Inspector, Scene and Preview. The games' F10 panels (v0.0.14-v0.0.18)
// rebuilt here, on the preview instead of a running game.
//
// ImGui's two closing rules, which differ (mixing them up is the classic
// ImGui crash, "Missing End()"):
//   Begin()      -> End() ALWAYS, even when Begin() returned false;
//   BeginTable() / TreeNode() / BeginCombo() -> their End or Pop ONLY when
//                   they returned true.
#include "WorkspaceApp.h"

#include "Assets/Model.h"
#include "Core/FrameTiming.h"
#include "Level/Level.h"
#include "Renderer/Renderer.h"
#include "World/GameWorld.h"

#include <SDL3/SDL.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace fs = std::filesystem;
using namespace AtomFramework;

namespace AtomWorkspace
{
    namespace
    {
        constexpr const char* PostNames[] = { "full", "grade", "off" };
        constexpr ImVec4 WarningColour{ 1.0f, 0.8f, 0.3f, 1.0f };

        bool EditColor(const char* label, glm::vec3& color)
        {
            return ImGui::ColorEdit3(label, &color.x, ImGuiColorEditFlags_Float);
        }

        std::string Vec3Json(const glm::vec3& v)
        {
            char text[96];
            std::snprintf(text, sizeof(text), "[%.3g, %.3g, %.3g]", v.x, v.y, v.z);
            return text;
        }

        // The selected entity as it lives in the world (null: not there -
        // gone with a flag, or a name the world doesn't have).
        const Entity* FindLive(const Level& level, const std::string& name)
        {
            const Entity* found = nullptr;
            level.GetWorld().ForEach([&](EntityId, const Entity& entity) {
                if (!found && entity.name == name)
                {
                    found = &entity;
                }
            });
            return found;
        }

        bool IsLevelFile(const fs::path& path)
        {
            return path.extension() == ".json" && path.stem().extension() != ".markers"
                && path.parent_path().filename() == "Levels";
        }
    }

    void WorkspaceApp::ApplyViewSwitch(const std::string& what, const std::string& value)
    {
        // The scenarios' "set" switches - one path for the panel and the tests.
        AtomFramework::ApplyViewSwitch(GetRenderer(), m_preview->View(), &m_preview->Camera(), nullptr, what, value);
    }

    void WorkspaceApp::SelectLevel(int index)
    {
        if (!m_project || index < 0 || index >= static_cast<int>(m_project->levels.size()))
        {
            return;
        }
        m_project->selectedLevel = index;
        const std::string& name = m_project->levels[static_cast<std::size_t>(index)];
        m_inspector.Clear();
        m_skyError.clear();
        const std::string error = m_preview->LoadLevel(name);
        if (error.empty())
        {
            SetStatus("Showing " + m_project->descriptor.name + " / " + name + ".");
        }
        else
        {
            SetStatus(error, true);
        }
    }

    // ---- Assets -----------------------------------------------------------

    void WorkspaceApp::DrawAssetsPanel()
    {
        if (!m_project)
        {
            ImGui::TextDisabled("No project open.");
            return;
        }
        // The project's own files, then the shared content it can use. A
        // level opens with a double-click.
        const fs::path assets = fs::path(m_project->directory) / m_project->descriptor.assets;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNodeEx("##project", ImGuiTreeNodeFlags_SpanAvailWidth, "%s/", m_project->descriptor.assets.c_str()))
        {
            DrawAssetFolder(assets);
            ImGui::TreePop();
        }
        if (!m_engineRoot.empty() && ImGui::TreeNodeEx("##content", ImGuiTreeNodeFlags_SpanAvailWidth, "Content/ (shared)"))
        {
            DrawAssetFolder(fs::path(m_engineRoot) / "Content");
            ImGui::TreePop();
        }
    }

    void WorkspaceApp::DrawAssetFolder(const fs::path& folder)
    {
        // Read only while open: a closed folder costs nothing.
        std::vector<fs::directory_entry> entries;
        std::error_code error;
        for (const fs::directory_entry& entry : fs::directory_iterator(folder, error))
        {
            if (entry.path().filename().string().front() != '.') // .gitkeep and friends
            {
                entries.push_back(entry);
            }
        }
        std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
            if (a.is_directory() != b.is_directory())
            {
                return a.is_directory(); // folders first
            }
            return a.path().filename() < b.path().filename();
        });
        for (const fs::directory_entry& entry : entries)
        {
            const std::string name = entry.path().filename().string();
            if (entry.is_directory())
            {
                if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
                {
                    DrawAssetFolder(entry.path());
                    ImGui::TreePop();
                }
                continue;
            }
            const bool level = IsLevelFile(entry.path());
            const bool current = level && m_preview->GetLevel() && m_preview->GetLevel()->GetName() == entry.path().stem().string();
            ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen
                                                | ImGuiTreeNodeFlags_SpanAvailWidth | (current ? ImGuiTreeNodeFlags_Selected : 0));
            if (level && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && m_project)
            {
                const auto found = std::find(m_project->levels.begin(), m_project->levels.end(), entry.path().stem().string());
                if (found != m_project->levels.end())
                {
                    SelectLevel(static_cast<int>(found - m_project->levels.begin()));
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            {
                ImGui::SetTooltip("%s%s", entry.path().generic_string().c_str(), level ? "\nDouble-click to show it" : "");
            }
        }
    }

    // ---- Hierarchy and Inspector -------------------------------------------

    void WorkspaceApp::DrawHierarchyPanel()
    {
        Level* level = m_preview->GetLevel();
        if (!level)
        {
            ImGui::TextDisabled(m_project ? "No level shown - pick one in the toolbar." : "No project open.");
            return;
        }
        const LevelData& data = level->GetData();
        ImGui::TextUnformatted(level->GetName().c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload"))
        {
            const std::string error = m_preview->Reload();
            SetStatus(error.empty() ? "Reloaded " + level->GetName() + "." : error, !error.empty());
            level = m_preview->GetLevel();
            if (!level)
            {
                return;
            }
        }
        if (!data.warnings.empty())
        {
            ImGui::TextColored(WarningColour, "%zu warning(s) - see the Inspector, or Validate", data.warnings.size());
        }
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##filter", "filter entities by name", m_entityFilter.data(), m_entityFilter.size());

        // The world's entities, as the level built them. Click selects,
        // double-click (or F over the scene) brings the camera to it.
        const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY
                                    | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("entities", 2, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Has", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();
            std::string frame;
            level->GetWorld().ForEach([&](EntityId, const Entity& entity) {
                if (!EntityInspector::Matches(entity.name, m_entityFilter.data()))
                {
                    return;
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(entity.name.c_str());
                const bool selected = m_inspector.SelectedEntity() == entity.name;
                if (ImGui::Selectable(entity.name.c_str(), selected,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                {
                    m_inspector.Select(level->GetName(), entity.name);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        frame = entity.name;
                    }
                }
                ImGui::PopID();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s%s%s%s", entity.renderable ? "model " : "", entity.interactable ? "use " : "",
                                    (entity.animated || entity.animator) ? "anim " : "", entity.hidden ? "hidden" : "");
            });
            ImGui::EndTable();
            if (!frame.empty())
            {
                FrameSelection();
            }
        }
    }

    void WorkspaceApp::DrawInspectorPanel()
    {
        Level* level = m_preview->GetLevel();
        if (!level)
        {
            ImGui::TextDisabled("Nothing to inspect.");
            return;
        }
        const LevelData& data = level->GetData();
        const std::size_t selected = m_inspector.Resolve(level->GetName(), data);
        if (selected == EntityInspector::npos)
        {
            ImGui::TextDisabled("Pick an entity in the Hierarchy.");
            if (!data.warnings.empty())
            {
                ImGui::SeparatorText("Level warnings");
                for (const LevelWarning& warning : data.warnings)
                {
                    ImGui::TextColored(WarningColour, "%s", warning.path.c_str());
                    ImGui::TextWrapped("%s", warning.message.c_str());
                }
            }
            return;
        }
        const EntityData& entity = data.entities[selected];
        ImGui::SeparatorText(entity.name.c_str());
        ImGui::Checkbox("Show in the scene", &m_showBounds);
        ImGui::SameLine();
        if (ImGui::SmallButton("Frame (F)"))
        {
            FrameSelection();
        }
        if (m_showBounds)
        {
            // The legend - the same colours as DrawSelectionBounds.
            ImGui::TextColored({ 1.0f, 0.82f, 0.24f, 1.0f }, "yellow: model bounds (what is drawn)");
            ImGui::TextColored({ 0.31f, 0.86f, 1.0f, 1.0f }, "cyan: collider (what blocks the player)");
            ImGui::TextColored({ 1.0f, 0.63f, 0.86f, 1.0f }, "pink: a point - no model, no collider");
        }
        ImGui::Spacing();
        // Read-only (M114's model): where its position came from, transform,
        // model, collider, interaction, animation, and the live state.
        for (const std::string& line : EntityInspector::Describe(data, selected, FindLive(*level, entity.name)))
        {
            ImGui::TextWrapped("%s", line.c_str());
        }
        for (const LevelWarning* warning : EntityInspector::WarningsFor(data, selected))
        {
            ImGui::TextColored(WarningColour, "warning %s:", warning->path.c_str());
            ImGui::TextWrapped("%s", warning->message.c_str());
        }
    }

    void WorkspaceApp::FrameSelection()
    {
        Level* level = m_preview->GetLevel();
        if (!level || !m_inspector.HasSelection())
        {
            return;
        }
        const Entity* live = FindLive(*level, m_inspector.SelectedEntity());
        if (!live)
        {
            return;
        }
        glm::vec3 center = live->position;
        float radius = 1.0f;
        if (live->renderable && live->renderable->model)
        {
            const Atom::Model& model = *live->renderable->model;
            const float scale = live->renderable->scale;
            center += (model.GetBoundsMin() + model.GetBoundsMax()) * 0.5f * scale; // close enough under a yaw
            radius = glm::length(model.GetBoundsMax() - model.GetBoundsMin()) * 0.5f * scale;
        }
        m_preview->Frame(center, radius);
    }

    // ---- Scene ------------------------------------------------------------

    void WorkspaceApp::DrawScenePanel()
    {
        const ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 2.0f || size.y < 2.0f)
        {
            m_scenePanel.drawn = false; // nothing to see: the tiny target
            return;
        }
        // The renderer's offscreen frame, sized to the panel in pixels (a
        // detached panel on another monitor has its own scale).
        const ImGuiViewport* viewport = ImGui::GetWindowViewport();
        const float scale = viewport && viewport->FramebufferScale.x > 0.0f ? viewport->FramebufferScale.x : 1.0f;
        const auto width = static_cast<std::uint32_t>(size.x * scale);
        const auto height = static_cast<std::uint32_t>(size.y * scale);
        Atom::Renderer& renderer = GetRenderer();
        renderer.SetSceneTarget(width, height);

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##scene", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* list = ImGui::GetWindowDrawList();
        // The size is applied when the renderer next draws, later this
        // frame: until then the texture is the old one, about to be
        // released - so the frame of a resize shows nothing rather than it.
        if (SDL_GPUTexture* texture = renderer.GetSceneTargetTexture();
            texture && renderer.GetSceneTargetWidth() == width && renderer.GetSceneTargetHeight() == height)
        {
            list->AddImage(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture)), origin,
                           { origin.x + size.x, origin.y + size.y });
        }
        else
        {
            list->AddRectFilled(origin, { origin.x + size.x, origin.y + size.y }, IM_COL32(12, 12, 14, 255));
        }

        if (!m_preview->GetLevel())
        {
            const char* text = m_project ? "No level shown - pick one in the toolbar" : "Open a project to see its levels";
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            list->AddText({ origin.x + (size.x - textSize.x) * 0.5f, origin.y + (size.y - textSize.y) * 0.5f },
                          IM_COL32(150, 150, 150, 255), text);
            return;
        }
        if (m_showBounds)
        {
            DrawSelectionBounds(origin.x, origin.y, size.x, size.y);
        }
        char hint[128];
        std::snprintf(hint, sizeof(hint), "right mouse + WASD/QE: fly (%.1f m/s, wheel)   F: frame selection", m_flySpeed);
        list->AddText({ origin.x + 8.0f, origin.y + 6.0f }, IM_COL32(200, 200, 200, 160), hint);
        UpdateFlyCamera(hovered, ImGui::GetIO().DeltaTime);
        if (hovered && !m_flying && ImGui::IsKeyPressed(ImGuiKey_F, false))
        {
            FrameSelection();
        }
    }

    void WorkspaceApp::UpdateFlyCamera(bool hovered, float deltaSeconds)
    {
        // A fly camera, as in most editors: hold the right button over the
        // scene to look and move; let go and the mouse is the UI's again.
        const ImGuiIO& io = ImGui::GetIO();
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            m_flying = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            m_flying = false;
        }
        if (hovered && !m_flying && io.MouseWheel != 0.0f)
        {
            m_flySpeed = std::clamp(m_flySpeed * std::pow(1.25f, io.MouseWheel), 0.25f, 100.0f);
        }
        if (!m_flying)
        {
            return;
        }
        Atom::Camera& camera = m_preview->Camera();
        constexpr float Sensitivity = 0.0035f; // radians per pixel
        camera.Rotate(io.MouseDelta.x * Sensitivity, -io.MouseDelta.y * Sensitivity);
        if (io.MouseWheel != 0.0f)
        {
            m_flySpeed = std::clamp(m_flySpeed * std::pow(1.25f, io.MouseWheel), 0.25f, 100.0f);
        }
        const auto held = [](ImGuiKey key) { return ImGui::IsKeyDown(key) ? 1.0f : 0.0f; };
        const glm::vec3 move = camera.GetForward() * (held(ImGuiKey_W) - held(ImGuiKey_S))
                             + camera.GetFlatRight() * (held(ImGuiKey_D) - held(ImGuiKey_A))
                             + glm::vec3{ 0.0f, 1.0f, 0.0f } * (held(ImGuiKey_E) - held(ImGuiKey_Q));
        if (glm::length(move) > 0.0f)
        {
            const float speed = m_flySpeed * (ImGui::IsKeyDown(ImGuiMod_Shift) ? 4.0f : 1.0f);
            camera.SetPosition(camera.GetPosition() + glm::normalize(move) * speed * deltaSeconds);
        }
    }

    void WorkspaceApp::DrawSelectionBounds(float x, float y, float width, float height)
    {
        // The selected entity drawn over the scene (M114's overlay, now in
        // the panel): its model's bounds (yellow), its collider as collision
        // stores it (cyan), or - with neither, often an interaction point on
        // something the level's own model draws - a cross at its position
        // with a line to where the player looks to use it (pink).
        Level* level = m_preview->GetLevel();
        const std::size_t selected = level ? m_inspector.Resolve(level->GetName(), level->GetData()) : EntityInspector::npos;
        if (selected == EntityInspector::npos)
        {
            return;
        }
        const EntityData& data = level->GetData().entities[selected];
        const Entity* live = FindLive(*level, data.name);
        if (!live)
        {
            return;
        }
        const float yaw = glm::radians(data.yawDegrees);
        const float c = std::abs(std::cos(yaw));
        const float s = std::abs(std::sin(yaw));
        const auto turned = [&](const glm::vec3& h) { // a turned box, as the axis-aligned box around it
            return glm::vec3{ h.x * c + h.z * s, h.y, h.x * s + h.z * c };
        };
        const auto offset = [&](const glm::vec3& o) {
            return glm::vec3{ o.x * std::cos(yaw) + o.z * std::sin(yaw), o.y, -o.x * std::sin(yaw) + o.z * std::cos(yaw) };
        };
        struct Box
        {
            glm::vec3 center;
            glm::vec3 half;
            ImU32 colour;
        };
        std::vector<Box> boxes;
        if (live->renderable && live->renderable->model)
        {
            const Atom::Model& model = *live->renderable->model;
            const float scale = live->renderable->scale;
            const glm::vec3 middle = (model.GetBoundsMin() + model.GetBoundsMax()) * 0.5f * scale;
            const glm::vec3 half = (model.GetBoundsMax() - model.GetBoundsMin()) * 0.5f * scale;
            boxes.push_back({ live->position + offset(middle), turned(half), IM_COL32(255, 210, 60, 255) });
        }
        if (data.collider)
        {
            boxes.push_back({ live->position + offset(data.collider->center), turned(data.collider->halfExtents),
                              IM_COL32(80, 220, 255, 255) });
        }

        const Atom::Camera& camera = m_preview->Camera();
        const glm::mat4 viewProjection =
            glm::perspective(camera.verticalFov, width / height, camera.nearPlane, camera.farPlane) * camera.GetViewMatrix();
        const auto project = [&](const glm::vec3& p, ImVec2& out) {
            const glm::vec4 clip = viewProjection * glm::vec4(p, 1.0f);
            if (clip.w <= camera.nearPlane)
            {
                return false; // behind the camera
            }
            out = { x + (clip.x / clip.w * 0.5f + 0.5f) * width, y + (0.5f - clip.y / clip.w * 0.5f) * height };
            return true;
        };
        ImDrawList* list = ImGui::GetWindowDrawList();
        list->PushClipRect({ x, y }, { x + width, y + height }, true);
        for (const Box& box : boxes)
        {
            std::array<ImVec2, 8> points{};
            std::array<bool, 8> visible{};
            for (int i = 0; i < 8; ++i)
            {
                const glm::vec3 corner = box.center + glm::vec3{ (i & 1) ? box.half.x : -box.half.x,
                                                                 (i & 2) ? box.half.y : -box.half.y,
                                                                 (i & 4) ? box.half.z : -box.half.z };
                visible[i] = project(corner, points[i]);
            }
            for (int a = 0; a < 8; ++a)
            {
                for (const int axis : { 1, 2, 4 })
                {
                    const int b = a | axis;
                    if (b != a && visible[a] && visible[b])
                    {
                        list->AddLine(points[a], points[b], box.colour, 2.0f);
                    }
                }
            }
        }
        ImVec2 label;
        if (boxes.empty())
        {
            const ImU32 colour = IM_COL32(255, 160, 220, 255);
            const auto line = [&](const glm::vec3& a, const glm::vec3& b) {
                ImVec2 pa, pb;
                if (project(a, pa) && project(b, pb))
                {
                    list->AddLine(pa, pb, colour, 2.0f);
                }
            };
            const glm::vec3 at = live->position;
            constexpr float Arm = 0.3f;
            line(at - glm::vec3{ Arm, 0.0f, 0.0f }, at + glm::vec3{ Arm, 0.0f, 0.0f });
            line(at - glm::vec3{ 0.0f, 0.0f, Arm }, at + glm::vec3{ 0.0f, 0.0f, Arm });
            glm::vec3 top = at + glm::vec3{ 0.0f, 0.5f, 0.0f };
            if (live->interactable)
            {
                top = at + live->interactable->focusOffset;
                ImVec2 focus;
                if (project(top, focus))
                {
                    list->AddCircle(focus, 6.0f, colour, 0, 2.0f);
                }
            }
            line(at, top);
            if (project(top + glm::vec3{ 0.0f, 0.15f, 0.0f }, label))
            {
                list->AddText(label, colour, (data.name + " (a point)").c_str());
            }
        }
        else if (project(boxes.front().center + glm::vec3{ 0.0f, boxes.front().half.y + 0.2f, 0.0f }, label))
        {
            list->AddText(label, boxes.front().colour, data.name.c_str());
        }
        list->PopClipRect();
    }

    // ---- Preview controls -------------------------------------------------

    void WorkspaceApp::DrawPreviewPanel()
    {
        // Temporary, as F10's were: nothing here is saved; "Copy as JSON"
        // gives what to paste into the level or preset file.
        if (!m_preview->GetLevel())
        {
            ImGui::TextDisabled("No level shown.");
            if (ImGui::CollapsingHeader("Performance"))
            {
                DrawPerformanceSection();
            }
            return;
        }
        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawEnvironmentSection();
        }
        if (ImGui::CollapsingHeader("Sky picture"))
        {
            DrawSkyPictureSection();
        }
        if (ImGui::CollapsingHeader("Level lighting"))
        {
            DrawLightingSection();
        }
        if (ImGui::CollapsingHeader("Render"))
        {
            DrawRenderSection();
        }
        if (ImGui::CollapsingHeader("Performance"))
        {
            DrawPerformanceSection();
        }
    }

    void WorkspaceApp::DrawEnvironmentSection()
    {
        // The presets (M49), blended over the transition; then what is
        // showing, editable live until the next switch.
        ImGui::PushID("environment");
        ImGui::SliderFloat("Transition", &m_environmentSeconds, 0.0f, 10.0f, "%.1f s");
        if (ImGui::RadioButton("level", m_preview->EnvironmentName().empty()))
        {
            m_preview->SetEnvironment("level", m_environmentSeconds);
        }
        for (const std::string& name : m_preview->Presets())
        {
            ImGui::SameLine();
            if (ImGui::GetContentRegionAvail().x < ImGui::CalcTextSize(name.c_str()).x + ImGui::GetFrameHeight() * 2.0f)
            {
                ImGui::NewLine();
            }
            if (ImGui::RadioButton(name.c_str(), m_preview->EnvironmentName() == name))
            {
                m_preview->SetEnvironment(name, m_environmentSeconds);
            }
        }
        EnvironmentController& environment = m_preview->Environment();
        if (environment.IsTransitioning())
        {
            ImGui::ProgressBar(environment.Progress(), { -1.0f, 0.0f }, "blending");
        }
        ImGui::SeparatorText("Showing now");
        EnvironmentState e = environment.Current();
        bool changed = false;
        changed |= ImGui::DragFloat3("Sun direction", &e.sunDirection.x, 0.01f, -1.0f, 1.0f);
        changed |= EditColor("Sun", e.sunColor);
        changed |= EditColor("Ambient sky", e.skyColor);
        changed |= EditColor("Ambient ground", e.groundColor);
        changed |= EditColor("Fog colour", e.fogColor);
        if (e.fogDensity)
        {
            changed |= ImGui::SliderFloat("Fog density", &*e.fogDensity, 0.0f, 0.15f, "%.3f");
        }
        if (e.sky)
        {
            changed |= EditColor("Zenith", e.sky->zenith);
            changed |= EditColor("Horizon", e.sky->horizon);
            changed |= ImGui::SliderFloat("Sun size", &e.sky->sunSize, 0.0f, 10.0f, "%.2f deg");
            changed |= ImGui::SliderFloat("Sun glow", &e.sky->sunGlow, 0.0f, 2.0f);
        }
        changed |= EditColor("Water shallow", e.water.shallow);
        changed |= EditColor("Water deep", e.water.deep);
        changed |= ImGui::SliderFloat("Sky reflection", &e.water.skyReflection, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Ripple", &e.water.ripple, 0.0f, 3.0f);
        changed |= ImGui::SliderFloat("Glint", &e.water.glint, 0.0f, 2.0f);
        changed |= ImGui::Checkbox("Water reflection", &e.water.reflection);
        changed |= ImGui::SliderFloat("Rain", &e.rain, 0.0f, 1.0f);
        changed |= ImGui::DragFloat3("Wind (m/s)", &e.wind.x, 0.02f, -6.0f, 6.0f);
        if (changed)
        {
            environment.Reset(e); // the preview applies the lighting every frame
        }
        if (ImGui::Button("Copy as JSON"))
        {
            const std::string name = m_preview->EnvironmentName().empty() ? "my_preset" : m_preview->EnvironmentName();
            std::string json = "{\n  \"$schema\": \"../../Framework/Schemas/environment.schema.json\",\n"
                               "  \"name\": \"" + name + "\",\n"
                               "  \"sunDirection\": " + Vec3Json(e.sunDirection) + ",\n"
                               "  \"sunColor\": " + Vec3Json(e.sunColor) + ",\n"
                               "  \"skyColor\": " + Vec3Json(e.skyColor) + ",\n"
                               "  \"groundColor\": " + Vec3Json(e.groundColor) + ",\n"
                               "  \"fogColor\": " + Vec3Json(e.fogColor) + ",\n";
            char number[160];
            if (e.fogDensity)
            {
                std::snprintf(number, sizeof(number), "  \"fogDensity\": %.3g,\n", *e.fogDensity);
                json += number;
            }
            if (e.sky)
            {
                std::snprintf(number, sizeof(number), "\"sunSize\": %.3g, \"sunGlow\": %.3g", e.sky->sunSize, e.sky->sunGlow);
                json += "  \"skyGradient\": { \"zenith\": " + Vec3Json(e.sky->zenith) + ", \"horizon\": "
                      + Vec3Json(e.sky->horizon) + ", " + number + " },\n";
            }
            std::snprintf(number, sizeof(number), "\"skyReflection\": %.3g, \"ripple\": %.3g, \"glint\": %.3g%s",
                          e.water.skyReflection, e.water.ripple, e.water.glint, e.water.reflection ? ", \"reflection\": true" : "");
            json += "  \"water\": { \"shallow\": " + Vec3Json(e.water.shallow) + ", \"deep\": " + Vec3Json(e.water.deep)
                  + ", " + number + " }\n}\n";
            SDL_SetClipboardText(json.c_str());
            SetStatus("Copied the environment as JSON (Content/Environments/<name>.json).");
        }
        ImGui::PopID();
    }

    void WorkspaceApp::DrawSkyPictureSection()
    {
        // Off lets a preset's gradient sky show (a picture always wins over
        // it); a picked PNG stands in for the level's own.
        ImGui::PushID("sky");
        ViewToggles& view = m_preview->View();
        const Level* level = m_preview->GetLevel();
        const std::string own = level->GetData().sky ? level->GetData().sky->panorama : std::string{};
        bool picture = view.skyPicture;
        if (ImGui::Checkbox("Sky picture", &picture))
        {
            ApplyViewSwitch("sky_picture", picture ? "on" : "off");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("off: the gradient sky, or none");
        const std::string levelLabel = own.empty() ? "level's (none)" : "level's (" + own + ")";
        const std::string current = view.skyImageName.empty() ? levelLabel : view.skyImageName;
        if (ImGui::BeginCombo("Picture", current.c_str()))
        {
            if (ImGui::Selectable(levelLabel.c_str(), view.skyImageName.empty()))
            {
                m_preview->LoadSkyImage("");
                m_skyError.clear();
            }
            for (const std::string& file : m_preview->SkyImages())
            {
                const std::string name = "Sky/" + file;
                if (ImGui::Selectable(name.c_str(), view.skyImageName == name))
                {
                    m_skyError = m_preview->LoadSkyImage(file) ? std::string{} : "can't load " + name;
                }
            }
            ImGui::EndCombo();
        }
        if (!view.skyImageName.empty())
        {
            ImGui::SliderFloat("Intensity", &view.skyImageIntensity, 0.0f, 2.0f);
        }
        if (!m_skyError.empty())
        {
            ImGui::TextColored({ 1.0f, 0.5f, 0.4f, 1.0f }, "%s", m_skyError.c_str());
        }
        const std::string panorama = view.skyImageName.empty() ? own : view.skyImageName;
        if (!panorama.empty() && ImGui::Button("Copy sky as JSON"))
        {
            const float intensity = view.skyImageName.empty() ? level->GetData().sky->intensity : view.skyImageIntensity;
            char json[256];
            std::snprintf(json, sizeof(json), "\"sky\": { \"panorama\": \"%s\", \"intensity\": %.3g }", panorama.c_str(), intensity);
            SDL_SetClipboardText(json);
            SetStatus("Copied the sky as JSON (a level file's \"sky\").");
        }
        ImGui::PopID();
    }

    void WorkspaceApp::DrawLightingSection()
    {
        // The level's own lighting, live; "Copy as JSON" gives the block to
        // paste into the level file. Nothing is saved by itself.
        ImGui::PushID("lighting");
        Level* level = m_preview->GetLevel();
        LevelLighting& l = level->EditLighting();
        bool changed = false;
        changed |= ImGui::DragFloat3("Sun direction", &l.sunDirection.x, 0.01f, -1.0f, 1.0f);
        changed |= EditColor("Sun", l.sunColor);
        changed |= EditColor("Sky", l.skyColor);
        changed |= EditColor("Ground", l.groundColor);
        changed |= EditColor("Fog colour", l.fogColor);
        bool hasFog = l.fogDensity.has_value();
        if (ImGui::Checkbox("Level fog", &hasFog))
        {
            l.fogDensity = hasFog ? std::optional<float>(0.03f) : std::nullopt;
            changed = true;
        }
        if (l.fogDensity)
        {
            ImGui::SameLine();
            changed |= ImGui::SliderFloat("##density", &*l.fogDensity, 0.0f, 0.15f, "%.3f");
        }
        bool hasSky = l.sky.has_value();
        if (ImGui::Checkbox("Sky gradient", &hasSky))
        {
            l.sky = hasSky ? std::optional<SkyGradient>(SkyGradient{}) : std::nullopt;
            changed = true;
        }
        if (l.sky)
        {
            changed |= EditColor("Zenith", l.sky->zenith);
            changed |= EditColor("Horizon", l.sky->horizon);
            changed |= ImGui::SliderFloat("Sun size", &l.sky->sunSize, 0.0f, 10.0f, "%.2f deg");
            changed |= ImGui::SliderFloat("Sun glow", &l.sky->sunGlow, 0.0f, 2.0f);
        }
        if (ImGui::TreeNode("Water"))
        {
            changed |= EditColor("Shallow", l.water.shallow);
            changed |= EditColor("Deep", l.water.deep);
            changed |= ImGui::SliderFloat("Sky reflection", &l.water.skyReflection, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Ripple", &l.water.ripple, 0.0f, 3.0f);
            changed |= ImGui::SliderFloat("Glint", &l.water.glint, 0.0f, 2.0f);
            changed |= ImGui::Checkbox("Reflection", &l.water.reflection);
            ImGui::TreePop();
        }
        changed |= ImGui::Checkbox("Sun shadows", &l.shadows);
        changed |= ImGui::SliderFloat("Baked light", &l.bakedLight, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Glow strength", &l.glowStrength, 0.0f, 2.0f);
        changed |= ImGui::SliderFloat("Glow threshold", &l.glowThreshold, 0.1f, 4.0f);
        if (changed)
        {
            m_preview->RefreshEnvironment(); // the preset in use, over the edited level
        }
        if (ImGui::Button("Copy as JSON"))
        {
            char glow[96];
            std::snprintf(glow, sizeof(glow), "{ \"strength\": %.3g, \"threshold\": %.3g }", l.glowStrength, l.glowThreshold);
            char baked[32];
            std::snprintf(baked, sizeof(baked), "%.3g", l.bakedLight);
            std::string extra;
            if (l.fogDensity)
            {
                char density[48];
                std::snprintf(density, sizeof(density), "  \"fogDensity\": %.3g,\n", *l.fogDensity);
                extra += density;
            }
            if (l.sky)
            {
                char sun[96];
                std::snprintf(sun, sizeof(sun), "\"sunSize\": %.3g, \"sunGlow\": %.3g", l.sky->sunSize, l.sky->sunGlow);
                extra += "  \"skyGradient\": { \"zenith\": " + Vec3Json(l.sky->zenith) + ", \"horizon\": "
                       + Vec3Json(l.sky->horizon) + ", " + sun + " },\n";
            }
            char water[160];
            std::snprintf(water, sizeof(water), "\"skyReflection\": %.3g, \"ripple\": %.3g, \"glint\": %.3g%s",
                          l.water.skyReflection, l.water.ripple, l.water.glint, l.water.reflection ? ", \"reflection\": true" : "");
            extra += "  \"water\": { \"shallow\": " + Vec3Json(l.water.shallow) + ", \"deep\": " + Vec3Json(l.water.deep)
                   + ", " + water + " },\n";
            const std::string json = std::string("\"lighting\": {\n") + "  \"sunDirection\": " + Vec3Json(l.sunDirection)
                                   + ",\n" + "  \"sunColor\": " + Vec3Json(l.sunColor) + ",\n" + "  \"skyColor\": "
                                   + Vec3Json(l.skyColor) + ",\n" + "  \"groundColor\": " + Vec3Json(l.groundColor) + ",\n"
                                   + "  \"fogColor\": " + Vec3Json(l.fogColor) + ",\n" + extra + "  \"shadows\": "
                                   + (l.shadows ? "true" : "false") + ",\n" + "  \"bakedLight\": " + baked + ",\n"
                                   + "  \"glow\": " + glow + "\n}";
            SDL_SetClipboardText(json.c_str());
            SetStatus("Copied the lighting as JSON (paste into " + level->GetName() + ".json).");
        }
        ImGui::PopID();
    }

    void WorkspaceApp::DrawRenderSection()
    {
        // The same switches as the scenarios' "set".
        ImGui::PushID("render");
        const Atom::RenderSettings& settings = GetRenderer().GetSettings();
        int msaa = settings.msaaSamples >= 4 ? 2 : settings.msaaSamples == 2 ? 1 : 0;
        if (ImGui::Combo("MSAA", &msaa, "1x\0" "2x\0" "4x\0"))
        {
            ApplyViewSwitch("msaa", msaa == 2 ? "4" : msaa == 1 ? "2" : "1");
        }
        float renderScale = settings.renderScale;
        if (ImGui::SliderFloat("Render scale", &renderScale, 0.25f, 1.0f, "%.2f"))
        {
            ApplyViewSwitch("scale", std::to_string(renderScale));
        }
        float fov = glm::degrees(m_preview->Camera().verticalFov);
        if (ImGui::SliderFloat("FOV", &fov, 30.0f, 110.0f, "%.0f deg"))
        {
            ApplyViewSwitch("fov", std::to_string(fov));
        }
        const ViewToggles& view = m_preview->View();
        int fog = static_cast<int>(view.fogPreset);
        const char* fogNames[std::size(FogPresets)];
        for (std::size_t i = 0; i < std::size(FogPresets); ++i)
        {
            fogNames[i] = FogPresets[i].name;
        }
        if (ImGui::Combo("Fog", &fog, fogNames, static_cast<int>(std::size(fogNames))))
        {
            ApplyViewSwitch("fog", fogNames[fog]);
        }
        int post = view.postMode;
        if (ImGui::Combo("Post", &post, PostNames, IM_ARRAYSIZE(PostNames)))
        {
            ApplyViewSwitch("post", PostNames[post]);
        }
        const auto toggle = [&](const char* label, const char* what, bool value) {
            if (ImGui::Checkbox(label, &value))
            {
                ApplyViewSwitch(what, value ? "on" : "off");
            }
        };
        toggle("Shadows", "shadows", view.shadows);
        toggle("Sun", "sun", view.sun);
        toggle("Glow", "glow", view.glow);
        toggle("Baked light", "baked", view.bakedLight);
        ImGui::PopID();
    }

    void WorkspaceApp::DrawPerformanceSection()
    {
        // The preview's frame: time, and what the renderer drew.
        ImGui::PushID("performance");
        float average = 0.0f;
        float worst = 0.0f;
        for (const float ms : m_frameHistory)
        {
            average += ms;
            worst = std::max(worst, ms);
        }
        average /= static_cast<float>(m_frameHistory.size());
        ImGui::Text("%.2f ms average (%.0f fps), worst %.2f ms", average, average > 0.0f ? 1000.0f / average : 0.0f, worst);
        const Atom::FrameTiming& t = GetAverageFrameTiming();
        ImGui::Text("CPU %.2f ms (update %.2f, render %.2f), waiting for GPU %.2f ms", t.CpuMs(), t.updateMs, t.renderMs,
                    t.waitMs);
        ImGui::PlotLines("##frames", m_frameHistory.data(), static_cast<int>(m_frameHistory.size()),
                         static_cast<int>(m_frameHistoryNext), "frame time (ms)", 0.0f, std::max(20.0f, worst), { -1.0f, 60.0f });
        const Atom::FrameStats& stats = GetRenderer().GetLastFrameStats();
        ImGui::Text("Scene %ux%u  MSAA %ux", stats.sceneWidth, stats.sceneHeight, stats.msaaSamples);
        ImGui::Text("Draws %u / %u   shadow %u   spot shadow %u", stats.drawn, stats.submitted, stats.shadowDrawn,
                    stats.spotShadowDrawn);
        ImGui::Text("Binds: pipelines %u, materials %u", stats.pipelineBinds, stats.materialBinds);
        ImGui::Text("Lit draws: spot %u, live %u (of %u lights)", stats.spotLitDraws, stats.liveLitDraws, stats.liveLights);
        bool overlay = GetPerfOverlay().IsOverlayVisible();
        if (GetPerfOverlay().IsInitialized() && ImGui::Checkbox("F1 overlay (in the window's corner)", &overlay))
        {
            GetPerfOverlay().SetOverlayVisible(overlay);
        }
        ImGui::PopID();
    }
}
