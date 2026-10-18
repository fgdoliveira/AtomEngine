#pragma once

#include "ConsoleLines.h"
#include "Core/Application.h"
#include "ProcessRunner.h"
#include "Debug/EntityInspector.h"
#include "ImGuiHost.h"
#include "Preview/ScenePreview.h"
#include "Project/ProjectDiscovery.h"
#include "RecentProjects.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
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
    public:
        // From the command line: a project to open, and a level of it to show.
        struct Startup
        {
            std::string project; // a <Name>.atomproject.json
            std::string level;
            // --test <folder>: the self-test (M126) - drives the workspace
            // through its tools and quits with 0 when all passed. Projects
            // it creates go under <folder>; the user's layout and recent
            // projects are neither read nor written.
            std::string testFolder;
        };
        void SetStartup(Startup startup) { m_startup = std::move(startup); }

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
            bool focus = false;     // brought to the front next frame
            bool noPadding = false; // the scene fills its panel edge to edge
            bool drawn = false;     // its contents were drawn this frame (visible, not a hidden tab)
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
        // M124: the project's panels (WorkspacePanels.cpp).
        void DrawAssetsPanel();
        void DrawAssetFolder(const std::filesystem::path& folder);
        void DrawHierarchyPanel();
        void DrawInspectorPanel();
        void DrawScenePanel();
        void DrawPreviewPanel();
        void DrawEnvironmentSection();
        void DrawSkyPictureSection();
        void DrawLightingSection();
        void DrawRenderSection();
        void DrawPerformanceSection();
        void DrawSelectionBounds(float x, float y, float width, float height);
        void UpdateFlyCamera(bool hovered, float deltaSeconds);
        void FrameSelection();
        void SelectLevel(int index);
        void ApplyViewSwitch(const std::string& what, const std::string& value);
        // M125: Validate, Build, Run as separate processes (WorkspaceOperations.cpp).
        enum class Operation
        {
            Validate,
            Build,
            Run,
        };
        void FindBuildTree();
        // "" when `operation` can start now; otherwise why not, for its tooltip.
        std::string WhyNot(Operation operation) const;
        void StartOperation(Operation operation);
        void StopOperation();
        void PumpOperation();
        void AddConsoleLine(std::string text);
        void DrawConsolePanel();
        std::string GameExecutable() const;

        // M126: the self-test (WorkspaceSelfTest.cpp), one step a frame or more.
        void RunSelfTest();
        bool Check(bool passed, const std::string& what);
        bool TestMode() const { return !m_startup.testFolder.empty(); }

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

        Startup m_startup;
        ImGuiHost m_imgui;
        std::string m_settingsDir;      // the user's pref path: layout, recents
        std::string m_engineRoot;       // the source tree this runs from, if any ("" in a package)
        RecentProjects m_recents;
        AtomProject::Discovery m_examples; // projects under <engine>/Apps
        std::optional<OpenedProject> m_project;

        Panel m_startPanel{ "Start" };
        Panel m_projectPanel{ "Project" };
        Panel m_assetsPanel{ "Assets" };
        Panel m_hierarchyPanel{ "Hierarchy" };
        Panel m_inspectorPanel{ "Inspector" };
        Panel m_scenePanel{ "Scene", true, false, false, true };
        Panel m_previewPanel{ "Preview" };
        Panel m_consolePanel{ "Console" };

        // The operations (M125): the build tree this workspace runs from
        // (<build>/bin/<config>/AtomWorkspace/), and the one child process.
        std::string m_buildDir;
        std::string m_buildConfig;
        std::string m_cmake;           // from the build's CMakeCache.txt
        ProcessRunner m_runner;
        Operation m_operation = Operation::Validate;
        struct ConsoleLine
        {
            std::string text;
            LineKind kind = LineKind::Plain;
            std::string file; // the file it names, opened with a double-click
        };
        std::vector<ConsoleLine> m_console;
        std::string m_firstProblem;    // the run's first error line with a file, for the status bar
        std::array<char, 64> m_consoleFilter{};
        bool m_consoleScroll = false;  // follow the output (until scrolled up)
        std::optional<int> m_lastExitCode; // the last operation's, once it ended

        // The self-test's progress.
        int m_testStep = 0;
        int m_testFrames = 0;   // frames spent in the current step
        int m_testFailures = 0;

        // The level shown (M124): no game code, the project's data only.
        std::unique_ptr<AtomFramework::ScenePreview> m_preview;
        AtomFramework::EntityInspector m_inspector; // the selection, by name
        std::array<char, 64> m_entityFilter{};
        bool m_showBounds = true;
        bool m_flying = false;          // right mouse held in the Scene panel
        float m_flySpeed = 4.0f;        // m/s; the wheel changes it
        float m_environmentSeconds = 3.0f;
        std::string m_skyError;
        std::array<float, 240> m_frameHistory{}; // ms, a ring
        std::size_t m_frameHistoryNext = 0;

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
