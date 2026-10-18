#pragma once

#include "Core/Application.h"
#include "ImGuiHost.h"
#include "Project/ProjectDiscovery.h"
#include "RecentProjects.h"

#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace AtomWorkspace
{
    // Atom Workspace (v0.0.19): the engine's development program - a window
    // of docked panels, any of which can leave it as a window of its own.
    // It opens projects (<Name>.atomproject.json), creates them (the same
    // AtomProject service as AtomProjectTool), and from M124 previews
    // their levels. It's an Application like the games: the same window,
    // device and renderer, with ImGui drawn in the renderer's overlay pass.
    class WorkspaceApp final : public Atom::Application
    {
    protected:
        bool OnInitialize() override;
        void OnUpdate(float deltaSeconds) override;
        void OnShutdown() override;
        bool OnEvent(const SDL_Event& event) override;

    private:
        // A dockable panel. Closing one that floats in its own window
        // docks it back instead; closing a docked one hides it (View).
        struct Panel
        {
            const char* name;
            bool open = true;
            bool redock = false;
        };

        struct OpenedProject
        {
            AtomProject::ProjectDescriptor descriptor;
            std::string descriptorPath;
            std::string directory;
            std::vector<std::string> levels; // <assets>/Levels/*.json, by name
            int selectedLevel = -1;
        };

        void DrawMenuBar();
        void DrawToolbar();
        void DrawStatusBar(float deltaSeconds);
        void DrawStartPage();
        void DrawProjectPanel();
        void DrawCreateProjectPopup();
        void DrawAboutPopup();
        void BuildDefaultLayout(unsigned dockspace);
        void DrawPanel(Panel& panel, unsigned dockspace, void (WorkspaceApp::*contents)());

        bool OpenProject(const std::string& descriptorPath);
        void CloseProject();
        void Rediscover();
        void BrowseForProject();
        void BrowseForWorkspace();
        void TakeDialogResults();
        void SetStatus(std::string text, bool error = false);

        ImGuiHost m_imgui;
        std::string m_settingsDir;      // the user's pref path: layout, recents
        std::string m_engineRoot;       // the source tree this runs from, if any ("" in a package)
        RecentProjects m_recents;
        AtomProject::Discovery m_examples; // projects under <engine>/Apps
        std::optional<OpenedProject> m_project;

        Panel m_startPanel{ "Start" };
        Panel m_projectPanel{ "Project" };

        std::string m_status;
        bool m_statusError = false;

        // Create Project
        bool m_openCreatePopup = false;
        bool m_openAboutPopup = false;
        char m_newName[65] = "";
        char m_newWorkspace[1024] = "";
        std::string m_createError;

        // The OS file dialogs answer on a thread of their own.
        std::mutex m_dialogMutex;
        std::optional<std::string> m_pickedDescriptor;
        std::optional<std::string> m_pickedWorkspace;
    };
}
