#include "WorkspaceApp.h"

#include "Core/Log.h"
#include "Project/ProjectCreator.h"

#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_internal.h> // DockBuilder, the viewport side bars

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace AtomWorkspace
{
    namespace
    {
        constexpr const char* DescriptorSuffix = ".atomproject.json";

        // The source tree this executable was built from - a folder up
        // from bin/<config>/AtomWorkspace/ holding Apps/ and the engine.
        // Empty when the workspace runs from somewhere else.
        std::string FindEngineRoot()
        {
            const char* base = SDL_GetBasePath();
            if (!base)
            {
                return {};
            }
            std::error_code error;
            fs::path folder = fs::path(base);
            for (int up = 0; up < 6 && !folder.empty(); ++up)
            {
                if (fs::is_directory(folder / "Apps", error) && fs::is_regular_file(folder / "Engine" / "Core" / "Application.h", error))
                {
                    return folder.string();
                }
                const fs::path parent = folder.parent_path();
                if (parent == folder)
                {
                    break;
                }
                folder = parent;
            }
            return {};
        }

        std::string ProjectNameOf(const std::string& descriptorPath)
        {
            std::string name = fs::path(descriptorPath).filename().string();
            if (name.size() > std::strlen(DescriptorSuffix) && name.ends_with(DescriptorSuffix))
            {
                name.resize(name.size() - std::strlen(DescriptorSuffix));
            }
            return name;
        }

        void DisabledHint(const char* hint)
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("%s", hint);
            }
        }

        constexpr const char* OperationsHint = "Validate, Build and Run arrive with the workspace's operations (M125).";
    }

    bool WorkspaceApp::OnInitialize()
    {
        SDL_Window* window = GetWindow().GetSDLWindow();
        SDL_SetWindowTitle(window, "Atom Workspace");
        SDL_MaximizeWindow(window);

        if (char* pref = SDL_GetPrefPath("AtomEngine", "Workspace"))
        {
            m_settingsDir = pref;
            SDL_free(pref);
        }
        m_engineRoot = FindEngineRoot();
        std::snprintf(m_newWorkspace, sizeof(m_newWorkspace), "%s", m_engineRoot.c_str());

        // ATOM_WORKSPACE_VIEWPORTS=0 keeps every panel inside the main
        // window, for a machine whose driver mishandles extra swapchains.
        const char* viewports = SDL_getenv("ATOM_WORKSPACE_VIEWPORTS");
        ImGuiHost::Options options;
        options.viewports = !(viewports && std::strcmp(viewports, "0") == 0);
        options.layoutFile = m_settingsDir.empty() ? std::string{} : m_settingsDir + "layout.ini";
        if (!m_imgui.Initialize(window, GetRenderer().GetDevice(), options))
        {
            return false;
        }
        GetRenderer().SetOverlayPass([this](SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* target) {
            m_imgui.Render(commandBuffer, target);
        });
        // No scene in the window itself: the renderer draws into a small
        // offscreen target and only clears the window, which the panels
        // cover. M124's Scene Viewport sizes that target to its panel.
        GetRenderer().SetSceneTarget(16, 16);

        if (!m_settingsDir.empty())
        {
            m_recents.Load(m_settingsDir + "recent.txt");
        }
        Rediscover();
        ATOM_LOG_INFO("Workspace") << "Atom Workspace ready (detachable panels: " << (options.viewports ? "on" : "off") << ").\n";
        SetStatus(m_engineRoot.empty() ? "Ready. No engine source tree found beside the workspace." : "Ready.");
        return true;
    }

    void WorkspaceApp::OnShutdown()
    {
        if (!m_settingsDir.empty())
        {
            m_recents.Save(m_settingsDir + "recent.txt");
        }
        GetRenderer().SetOverlayPass({});
        m_imgui.Shutdown(); // saves the layout
    }

    bool WorkspaceApp::OnEvent(const SDL_Event& event)
    {
        const bool taken = m_imgui.HandleEvent(event);
        // With panels in windows of their own, closing the main window
        // isn't the last window closing: quit on it explicitly.
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
            && event.window.windowID == SDL_GetWindowID(GetWindow().GetSDLWindow()))
        {
            RequestQuit(0);
        }
        return taken;
    }

    void WorkspaceApp::OnUpdate(float deltaSeconds)
    {
        TakeDialogResults();
        m_imgui.BeginFrame();

        DrawMenuBar();
        DrawToolbar();
        DrawStatusBar(deltaSeconds);

        const ImGuiID dockspace = ImGui::GetID("AtomWorkspaceDock");
        if (m_imgui.TakeLayoutReset())
        {
            BuildDefaultLayout(dockspace);
        }
        ImGui::DockSpaceOverViewport(dockspace, ImGui::GetMainViewport());

        DrawPanel(m_startPanel, dockspace, &WorkspaceApp::DrawStartPage);
        DrawPanel(m_projectPanel, dockspace, &WorkspaceApp::DrawProjectPanel);

        DrawCreateProjectPopup();
        DrawAboutPopup();
    }

    void WorkspaceApp::BuildDefaultLayout(unsigned dockspace)
    {
        // The default arrangement: the project on the left, the start page
        // (from M124, the scene) in the middle. Saved as the user moves it.
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::DockBuilderRemoveNode(dockspace);
        ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace, viewport->WorkSize);
        ImGuiID center = dockspace;
        const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.24f, nullptr, &center);
        ImGui::DockBuilderDockWindow(m_projectPanel.name, left);
        ImGui::DockBuilderDockWindow(m_startPanel.name, center);
        ImGui::DockBuilderFinish(dockspace);
        m_startPanel.open = true;
        m_projectPanel.open = true;
    }

    void WorkspaceApp::DrawPanel(Panel& panel, unsigned dockspace, void (WorkspaceApp::*contents)())
    {
        if (!panel.open)
        {
            return;
        }
        if (panel.redock)
        {
            if (const ImGuiDockNode* central = ImGui::DockBuilderGetCentralNode(dockspace))
            {
                ImGui::SetNextWindowDockID(central->ID, ImGuiCond_Always);
            }
            panel.redock = false;
        }
        bool keep = true;
        if (ImGui::Begin(panel.name, &keep))
        {
            (this->*contents)();
        }
        if (!keep)
        {
            // A floating panel's close button (its own OS window's too)
            // puts it back in the main window; a docked one hides.
            if (ImGui::IsWindowDocked())
            {
                panel.open = false;
            }
            else
            {
                panel.redock = true;
            }
        }
        ImGui::End();
    }

    void WorkspaceApp::DrawMenuBar()
    {
        if (!ImGui::BeginMainMenuBar())
        {
            return;
        }
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Create Project..."))
            {
                m_openCreatePopup = true;
            }
            if (ImGui::MenuItem("Open Project..."))
            {
                BrowseForProject();
            }
            if (ImGui::BeginMenu("Recent Projects", !m_recents.Entries().empty()))
            {
                std::string chosen;
                for (const std::string& entry : m_recents.Entries())
                {
                    if (ImGui::MenuItem(ProjectNameOf(entry).c_str()))
                    {
                        chosen = entry;
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", entry.c_str());
                    }
                }
                ImGui::EndMenu();
                if (!chosen.empty())
                {
                    OpenProject(chosen);
                }
            }
            if (ImGui::MenuItem("Close Project", nullptr, false, m_project.has_value()))
            {
                CloseProject();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
            {
                RequestQuit(0);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Project"))
        {
            if (ImGui::MenuItem("Rediscover Projects", nullptr, false, !m_engineRoot.empty()))
            {
                Rediscover();
            }
            if (ImGui::MenuItem("Show in File Explorer", nullptr, false, m_project.has_value()))
            {
                const std::string url = "file:///" + fs::path(m_project->directory).generic_string();
                SDL_OpenURL(url.c_str());
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Build"))
        {
            ImGui::MenuItem("Validate", nullptr, false, false);
            ImGui::MenuItem("Build", nullptr, false, false);
            ImGui::MenuItem("Run", nullptr, false, false);
            ImGui::MenuItem("Stop", nullptr, false, false);
            ImGui::Separator();
            ImGui::TextDisabled("%s", OperationsHint);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem(m_startPanel.name, nullptr, &m_startPanel.open);
            ImGui::MenuItem(m_projectPanel.name, nullptr, &m_projectPanel.open);
            ImGui::Separator();
            bool overlay = GetPerfOverlay().IsOverlayVisible();
            if (ImGui::MenuItem("Performance Overlay", "F1", &overlay, GetPerfOverlay().IsInitialized()))
            {
                GetPerfOverlay().SetOverlayVisible(overlay);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout"))
            {
                m_imgui.ResetLayout();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About Atom Workspace"))
            {
                m_openAboutPopup = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    void WorkspaceApp::DrawToolbar()
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float height = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f;
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::BeginViewportSideBar("##Toolbar", ImGui::GetMainViewport(), ImGuiDir_Up, height, flags))
        {
            // Project: the open one, switched among the recent ones.
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Project");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
            const std::string current = m_project ? m_project->descriptor.name : std::string("(none)");
            std::string chosen;
            if (ImGui::BeginCombo("##project", current.c_str()))
            {
                for (const std::string& entry : m_recents.Entries())
                {
                    const bool selected = m_project && m_project->descriptorPath == entry;
                    if (ImGui::Selectable(ProjectNameOf(entry).c_str(), selected))
                    {
                        chosen = entry;
                    }
                }
                if (ImGui::Selectable("Open Project..."))
                {
                    BrowseForProject();
                }
                ImGui::EndCombo();
            }
            if (!chosen.empty())
            {
                OpenProject(chosen);
            }

            // Level: one of the project's, for the preview (M124).
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Level");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
            ImGui::BeginDisabled(!m_project || m_project->levels.empty());
            const char* level = (m_project && m_project->selectedLevel >= 0)
                                    ? m_project->levels[static_cast<std::size_t>(m_project->selectedLevel)].c_str()
                                    : "(none)";
            if (ImGui::BeginCombo("##level", level))
            {
                for (std::size_t i = 0; i < m_project->levels.size(); ++i)
                {
                    const bool selected = static_cast<int>(i) == m_project->selectedLevel;
                    if (ImGui::Selectable(m_project->levels[i].c_str(), selected))
                    {
                        m_project->selectedLevel = static_cast<int>(i);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, style.ItemSpacing.x * 4.0f);
            ImGui::BeginDisabled(true);
            for (const char* operation : { "Validate", "Build", "Run", "Stop" })
            {
                ImGui::Button(operation);
                DisabledHint(OperationsHint);
                ImGui::SameLine();
            }
            ImGui::EndDisabled();
            ImGui::NewLine();
        }
        ImGui::End();
    }

    void WorkspaceApp::DrawStatusBar(float deltaSeconds)
    {
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
        if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, ImGui::GetFrameHeight(), flags))
        {
            if (ImGui::BeginMenuBar())
            {
                if (m_statusError)
                {
                    ImGui::TextColored({ 1.0f, 0.45f, 0.4f, 1.0f }, "%s", m_status.c_str());
                }
                else
                {
                    ImGui::TextUnformatted(m_status.c_str());
                }
                char frame[32];
                std::snprintf(frame, sizeof(frame), "%.1f ms", deltaSeconds * 1000.0f);
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(frame).x - ImGui::GetStyle().WindowPadding.x * 2.0f);
                ImGui::TextDisabled("%s", frame);
                ImGui::EndMenuBar();
            }
        }
        ImGui::End();
    }

    void WorkspaceApp::DrawStartPage()
    {
        ImGui::TextUnformatted("Atom Workspace");
        ImGui::TextDisabled("AtomEngine %s", ATOM_VERSION);
        ImGui::Spacing();
        if (ImGui::Button("Create Project..."))
        {
            m_openCreatePopup = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Project..."))
        {
            BrowseForProject();
        }
        ImGui::Spacing();
        ImGui::SeparatorText("Recent projects");
        if (m_recents.Entries().empty())
        {
            ImGui::TextDisabled("None yet - projects you open appear here.");
        }
        std::string chosen;
        for (const std::string& entry : m_recents.Entries())
        {
            std::error_code error;
            const bool exists = fs::is_regular_file(entry, error);
            ImGui::BeginDisabled(!exists);
            if (ImGui::Selectable(ProjectNameOf(entry).c_str()))
            {
                chosen = entry;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("%s%s", entry.c_str(), exists ? "" : "  (missing)");
        }
        if (!m_engineRoot.empty())
        {
            ImGui::Spacing();
            ImGui::SeparatorText("Projects in this engine");
            for (const AtomProject::DiscoveredProject& project : m_examples.projects)
            {
                if (ImGui::Selectable(project.descriptor.name.c_str()))
                {
                    chosen = project.descriptorPath;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", fs::relative(project.directory, m_engineRoot).generic_string().c_str());
            }
            for (const std::string& error : m_examples.errors)
            {
                ImGui::TextColored({ 1.0f, 0.45f, 0.4f, 1.0f }, "%s", error.c_str());
            }
        }
        if (!chosen.empty())
        {
            OpenProject(chosen);
        }
    }

    void WorkspaceApp::DrawProjectPanel()
    {
        if (!m_project)
        {
            ImGui::TextDisabled("No project open.");
            return;
        }
        const OpenedProject& project = *m_project;
        const AtomProject::ProjectDescriptor& d = project.descriptor;
        std::error_code error;
        const bool hasMain = fs::is_regular_file(fs::path(project.directory) / d.source / "Main.cpp", error);
        if (ImGui::BeginTable("##descriptor", 2))
        {
            // The value column takes the panel's remaining width: wrapped
            // text in a fit-to-content column has no width to wrap to.
            ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
            const auto row = [](const char* key, const std::string& value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", key);
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", value.c_str());
            };
            row("Name", d.name);
            row("Engine", d.engine);
            row("Folder", project.directory);
            row("Source", d.source);
            row("Assets", d.assets);
            row("Executable", d.executable.empty() ? std::string("(none)") : d.executable);
            row("Startup level", d.startupLevel.empty() ? std::string("(none)") : d.startupLevel);
            row("Levels", std::to_string(project.levels.size()));
            ImGui::EndTable();
        }
        if (!hasMain)
        {
            ImGui::Spacing();
            ImGui::TextWrapped("No %s/Main.cpp yet: the project configures, but builds no executable until it has one.",
                               d.source.c_str());
        }
    }

    void WorkspaceApp::DrawCreateProjectPopup()
    {
        if (m_openCreatePopup)
        {
            ImGui::OpenPopup("Create Project");
            m_openCreatePopup = false;
            m_createError.clear();
        }
        ImGui::SetNextWindowSize({ ImGui::GetFontSize() * 34.0f, 0.0f }, ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Create Project", nullptr, ImGuiWindowFlags_NoSavedSettings))
        {
            return;
        }
        ImGui::TextWrapped("A project is a folder, Apps/<Name>/, with a descriptor, the standard folders and its "
                           "CMake file. No gameplay: it builds an executable once you add Source/Main.cpp.");
        ImGui::Spacing();
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::InputText("Name", m_newName, sizeof(m_newName));
        const std::string nameError = AtomProject::CheckProjectName(m_newName);
        if (m_newName[0] != '\0' && !nameError.empty())
        {
            ImGui::TextColored({ 1.0f, 0.45f, 0.4f, 1.0f }, "%s", nameError.c_str());
        }
        ImGui::InputText("Workspace", m_newWorkspace, sizeof(m_newWorkspace));
        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
        {
            BrowseForWorkspace();
        }
        ImGui::TextDisabled("The folder holding Apps/ (the engine's own, or another).");
        if (!m_createError.empty())
        {
            ImGui::TextColored({ 1.0f, 0.45f, 0.4f, 1.0f }, "%s", m_createError.c_str());
        }
        ImGui::Spacing();
        ImGui::BeginDisabled(m_newName[0] == '\0' || !nameError.empty() || m_newWorkspace[0] == '\0');
        if (ImGui::Button("Create"))
        {
            const AtomProject::CreateResult result = AtomProject::CreateProject({ m_newWorkspace, m_newName, ATOM_VERSION });
            if (result.ok)
            {
                ATOM_LOG_INFO("Workspace") << "Created project " << m_newName << ".\n";
                OpenProject(result.descriptorPath);
                SetStatus(std::string("Created ") + m_newName + ". Configure CMake again to add it to the build.");
                if (!m_engineRoot.empty())
                {
                    Rediscover();
                }
                m_newName[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            else
            {
                m_createError = result.error;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void WorkspaceApp::DrawAboutPopup()
    {
        if (m_openAboutPopup)
        {
            ImGui::OpenPopup("About Atom Workspace");
            m_openAboutPopup = false;
        }
        if (ImGui::BeginPopupModal("About Atom Workspace", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::Text("Atom Workspace - AtomEngine %s", ATOM_VERSION);
            ImGui::TextDisabled("Dear ImGui %s (docking), detachable panels %s", IMGUI_VERSION,
                                m_imgui.ViewportsEnabled() ? "on" : "off");
            ImGui::TextDisabled("GPU: %s", GetRenderer().GetAdapterName().c_str());
            ImGui::Spacing();
            if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    bool WorkspaceApp::OpenProject(const std::string& descriptorPath)
    {
        if (!descriptorPath.ends_with(DescriptorSuffix))
        {
            SetStatus(descriptorPath + ": not a project descriptor (<Name>" + DescriptorSuffix + ")", true);
            return false;
        }
        const AtomProject::DescriptorLoad load = AtomProject::LoadDescriptor(descriptorPath);
        if (!load.descriptor)
        {
            SetStatus(load.error, true);
            std::error_code error;
            if (!fs::exists(descriptorPath, error))
            {
                m_recents.Remove(descriptorPath);
            }
            return false;
        }
        OpenedProject project;
        project.descriptor = *load.descriptor;
        project.descriptorPath = fs::absolute(descriptorPath).lexically_normal().string();
        project.directory = fs::path(project.descriptorPath).parent_path().string();
        std::error_code error;
        for (const fs::directory_entry& entry :
             fs::directory_iterator(fs::path(project.directory) / project.descriptor.assets / "Levels", error))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                project.levels.push_back(entry.path().stem().string());
            }
        }
        std::sort(project.levels.begin(), project.levels.end());
        const auto startup = std::find(project.levels.begin(), project.levels.end(), project.descriptor.startupLevel);
        project.selectedLevel = startup != project.levels.end() ? static_cast<int>(startup - project.levels.begin())
                                : project.levels.empty()        ? -1
                                                                : 0;
        m_recents.Add(project.descriptorPath);
        if (!m_settingsDir.empty())
        {
            m_recents.Save(m_settingsDir + "recent.txt");
        }
        SetStatus("Opened " + project.descriptor.name + ".");
        ATOM_LOG_INFO("Workspace") << "Opened project " << project.descriptor.name << ".\n";
        SDL_SetWindowTitle(GetWindow().GetSDLWindow(), ("Atom Workspace - " + project.descriptor.name).c_str());
        m_project = std::move(project);
        return true;
    }

    void WorkspaceApp::CloseProject()
    {
        m_project.reset();
        SDL_SetWindowTitle(GetWindow().GetSDLWindow(), "Atom Workspace");
        SetStatus("Ready.");
    }

    void WorkspaceApp::Rediscover()
    {
        if (!m_engineRoot.empty())
        {
            m_examples = AtomProject::FindProjects((fs::path(m_engineRoot) / "Apps").string());
        }
    }

    void WorkspaceApp::BrowseForProject()
    {
        static const SDL_DialogFileFilter filters[] = { { "AtomEngine project (*.atomproject.json)", "json" } };
        static std::string location;
        location = m_engineRoot.empty() ? std::string{} : (fs::path(m_engineRoot) / "Apps").string();
        SDL_ShowOpenFileDialog(
            [](void* user, const char* const* files, int /*filter*/) {
                if (files && files[0])
                {
                    auto* self = static_cast<WorkspaceApp*>(user);
                    const std::lock_guard lock(self->m_dialogMutex);
                    self->m_pickedDescriptor = files[0];
                }
            },
            this, GetWindow().GetSDLWindow(), filters, 1, location.empty() ? nullptr : location.c_str(), false);
    }

    void WorkspaceApp::BrowseForWorkspace()
    {
        static std::string location;
        location = m_newWorkspace;
        SDL_ShowOpenFolderDialog(
            [](void* user, const char* const* folders, int /*filter*/) {
                if (folders && folders[0])
                {
                    auto* self = static_cast<WorkspaceApp*>(user);
                    const std::lock_guard lock(self->m_dialogMutex);
                    self->m_pickedWorkspace = folders[0];
                }
            },
            this, GetWindow().GetSDLWindow(), location.empty() ? nullptr : location.c_str(), false);
    }

    void WorkspaceApp::TakeDialogResults()
    {
        std::optional<std::string> descriptor;
        {
            const std::lock_guard lock(m_dialogMutex);
            descriptor.swap(m_pickedDescriptor);
            if (m_pickedWorkspace)
            {
                std::snprintf(m_newWorkspace, sizeof(m_newWorkspace), "%s", m_pickedWorkspace->c_str());
                m_pickedWorkspace.reset();
            }
        }
        if (descriptor)
        {
            OpenProject(*descriptor);
        }
    }

    void WorkspaceApp::SetStatus(std::string text, bool error)
    {
        m_status = std::move(text);
        m_statusError = error;
    }
}
