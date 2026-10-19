// The workspace's operations (v0.0.19, M125): Validate (AtomLevelCheck),
// Build (cmake --build) and Run (the game), each a separate process - a
// crash or a hang there never takes the workspace with it - and the
// Console that shows their output as it comes.
#include "WorkspaceApp.h"

#include "Core/Log.h"
#include "Project/ProjectCreator.h"

#include <SDL3/SDL.h>

#include <imgui.h>

#include <fstream>

namespace fs = std::filesystem;

namespace AtomWorkspace
{
    namespace
    {
        constexpr std::size_t MaxConsoleLines = 20000;

        const char* NameOf(int operation)
        {
            constexpr const char* names[] = { "Validate", "Build", "Run" };
            return names[operation];
        }

        ImVec4 ColourOf(LineKind kind)
        {
            switch (kind)
            {
            case LineKind::Note: return { 0.93f, 0.62f, 0.32f, 1.0f };
            case LineKind::Success: return { 0.55f, 0.85f, 0.55f, 1.0f };
            case LineKind::Warning: return { 1.0f, 0.8f, 0.3f, 1.0f };
            case LineKind::Error: return { 1.0f, 0.45f, 0.4f, 1.0f };
            default: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
            }
        }

        std::string Executable(const fs::path& path)
        {
#ifdef _WIN32
            return path.string() + ".exe";
#else
            return path.string();
#endif
        }
    }

    void WorkspaceApp::FindBuildTree()
    {
        // The workspace runs from <build>/bin/<config>/AtomWorkspace/: its
        // build tree builds the projects, with the cmake that configured it.
        const char* base = SDL_GetBasePath();
        if (!base)
        {
            return;
        }
        const fs::path folder = fs::path(base).parent_path(); // the trailing separator off
        const fs::path build = folder.parent_path().parent_path().parent_path();
        std::ifstream cache(build / "CMakeCache.txt");
        std::string line;
        while (std::getline(cache, line))
        {
            constexpr std::string_view key = "CMAKE_COMMAND:INTERNAL=";
            if (line.starts_with(key))
            {
                m_cmake = line.substr(key.size());
                break;
            }
        }
        if (!m_cmake.empty())
        {
            m_buildDir = build.string();
            m_buildConfig = folder.parent_path().filename().string();
        }
    }

    bool WorkspaceApp::Standalone() const
    {
        // v0.0.20: a project with its own presets owns its build (M129);
        // the engine's bundled examples have none and build in the
        // workspace's own tree.
        std::error_code error;
        return m_project && fs::is_regular_file(fs::path(m_project->directory) / "CMakePresets.json", error);
    }

    std::string WorkspaceApp::ProjectBuildDir() const
    {
        if (!m_project)
        {
            return {};
        }
        return Standalone() ? (fs::path(m_project->directory) / "build").string() : m_buildDir;
    }

    std::string WorkspaceApp::GameExecutable() const
    {
        const std::string build = ProjectBuildDir();
        if (build.empty() || m_buildConfig.empty() || m_project->descriptor.executable.empty())
        {
            return {};
        }
        const std::string& target = m_project->descriptor.executable;
        return Executable(fs::path(build) / "bin" / m_buildConfig / target / target);
    }

    std::string WorkspaceApp::WhyNot(Operation operation) const
    {
        if (!m_project)
        {
            return "Open a project first.";
        }
        if (m_runner.IsRunning())
        {
            return std::string(NameOf(static_cast<int>(m_operation))) + " is running - Stop it first.";
        }
        if (m_buildDir.empty())
        {
            // Its tools (AtomLevelCheck), its cmake and its configuration.
            return "The workspace isn't running from a build tree (<build>/bin/<config>/AtomWorkspace/).";
        }
        std::error_code error;
        const AtomProject::ProjectDescriptor& d = m_project->descriptor;
        switch (operation)
        {
        case Operation::Validate:
            if (!fs::is_regular_file(Executable(fs::path(m_buildDir) / "bin" / m_buildConfig / "AtomLevelCheck"), error))
            {
                return "AtomLevelCheck isn't built (" + m_buildConfig + ").";
            }
            return {};
        case Operation::Build:
        case Operation::Run:
            if (!fs::is_regular_file(fs::path(m_project->directory) / d.source / "Main.cpp", error))
            {
                return "No " + d.source + "/Main.cpp yet: the project builds no executable until it has one.";
            }
            if (d.executable.empty())
            {
                return "The descriptor names no executable.";
            }
            if (operation == Operation::Build && Standalone() && m_engineRoot.empty()
                && !fs::is_regular_file(fs::path(m_project->directory) / "CMakeUserPresets.json", error))
            {
                return "No engine to build against: this workspace runs outside an engine's folder and the project "
                       "has no CMakeUserPresets.json.";
            }
            if (operation == Operation::Run && !fs::is_regular_file(GameExecutable(), error))
            {
                return d.executable + " isn't built yet (" + m_buildConfig + ") - Build it first.";
            }
            return {};
        }
        return {};
    }

    bool WorkspaceApp::StartStep(Step step)
    {
        std::string commandLine;
        for (const std::string& arg : step.args)
        {
            commandLine += (arg.find(' ') != std::string::npos ? "\"" + arg + "\"" : arg) + ' ';
        }
        AddConsoleLine("> " + std::string(NameOf(static_cast<int>(m_operation))) + ": " + commandLine);
        if (!m_runner.Start(std::move(step.args), step.workingDirectory))
        {
            AddConsoleLine("< " + m_runner.Error());
            SetStatus(m_runner.Error(), true);
            m_pendingSteps.clear();
            m_lastExitCode = -1;
            return false;
        }
        return true;
    }

    void WorkspaceApp::StartOperation(Operation operation)
    {
        if (const std::string why = WhyNot(operation); !why.empty())
        {
            SetStatus(why, true);
            return;
        }
        // An operation is one process, or a few run one after another, each
        // only if the one before succeeded (a standalone project's first
        // Build configures, then builds).
        std::vector<Step> steps;
        const std::string project = m_project->directory;
        switch (operation)
        {
        case Operation::Validate:
            steps.push_back({ { Executable(fs::path(m_buildDir) / "bin" / m_buildConfig / "AtomLevelCheck"), "--project",
                                m_project->descriptorPath } });
            break;
        case Operation::Build:
        {
            const std::string build = ProjectBuildDir();
            std::error_code error;
            if (Standalone() && !fs::is_regular_file(fs::path(build) / "CMakeCache.txt", error))
            {
                // Its own build, configured once: through its presets, this
                // engine named in CMakeUserPresets.json (written here when a
                // cloned project has none - it is never committed).
                const fs::path userPresets = fs::path(project) / "CMakeUserPresets.json";
                if (!fs::is_regular_file(userPresets, error))
                {
                    std::ofstream(userPresets, std::ios::binary) << AtomProject::UserPresets(m_engineRoot);
                    AddConsoleLine("> wrote CMakeUserPresets.json: this engine, for this machine only");
                }
                steps.push_back({ { m_cmake, "--preset", "atom" }, project });
            }
            // The examples build in the workspace's own tree (the engine's).
            steps.push_back({ { m_cmake, "--build", build, "--config", m_buildConfig, "--target",
                                m_project->descriptor.executable } });
            break;
        }
        case Operation::Run:
        {
            const std::string game = GameExecutable();
            steps.push_back({ { game }, fs::path(game).parent_path().string() });
            break;
        }
        }
        m_console.clear();
        m_firstProblem.clear();
        m_lastExitCode.reset();
        m_operation = operation;
        m_pendingSteps.assign(steps.begin() + 1, steps.end());
        if (!StartStep(std::move(steps.front())))
        {
            return;
        }
        SetStatus(std::string(NameOf(static_cast<int>(operation))) + " running...");
        m_consolePanel.open = true;
        m_consolePanel.focus = true;
        m_consoleScroll = true;
    }

    void WorkspaceApp::StopOperation()
    {
        if (m_runner.IsRunning())
        {
            m_runner.Stop();
            SetStatus(std::string("Stopping ") + NameOf(static_cast<int>(m_operation)) + "...");
        }
    }

    void WorkspaceApp::PumpOperation()
    {
        // Each frame: what the process wrote since, then - once - how it ended.
        for (std::string& line : m_runner.TakeLines())
        {
            AddConsoleLine(std::move(line));
        }
        const std::optional<int> exitCode = m_runner.TakeExitCode();
        if (!exitCode)
        {
            return;
        }
        // The next step of the chain, only after a success.
        if (*exitCode == 0 && !m_runner.WasStopped() && !m_pendingSteps.empty())
        {
            Step next = std::move(m_pendingSteps.front());
            m_pendingSteps.erase(m_pendingSteps.begin());
            StartStep(std::move(next));
            return;
        }
        m_pendingSteps.clear();
        m_lastExitCode = exitCode;
        const char* name = NameOf(static_cast<int>(m_operation));
        char summary[160];
        if (m_runner.WasStopped())
        {
            std::snprintf(summary, sizeof(summary), "< %s stopped after %.1f s", name, m_runner.Seconds());
            SetStatus(summary + 2);
        }
        else
        {
            std::snprintf(summary, sizeof(summary), "< %s %s (exit code %d) in %.1f s", name,
                          *exitCode == 0 ? "succeeded" : "failed", *exitCode, m_runner.Seconds());
            // A failure says where first: the file of its first problem.
            SetStatus(*exitCode == 0 || m_firstProblem.empty() ? std::string(summary + 2)
                                                                : std::string(summary + 2) + " - " + m_firstProblem,
                      *exitCode != 0);
        }
        AddConsoleLine(summary);
        ATOM_LOG_INFO("Workspace") << (summary + 2) << '\n';
    }

    void WorkspaceApp::AddConsoleLine(std::string text)
    {
        ConsoleLine line;
        line.kind = ClassifyLine(text);
        if (m_project && line.kind != LineKind::Note)
        {
            std::vector<std::string> roots{ (fs::path(m_project->directory) / m_project->descriptor.assets).string(),
                                            m_project->directory };
            if (!m_engineRoot.empty())
            {
                roots.push_back((fs::path(m_engineRoot) / "Content").string());
                roots.push_back(m_engineRoot);
            }
            line.file = FileInLine(text, roots);
        }
        if (m_firstProblem.empty() && !line.file.empty() && line.kind != LineKind::Success)
        {
            m_firstProblem = fs::path(line.file).filename().string();
        }
        line.text = std::move(text);
        if (m_console.size() >= MaxConsoleLines)
        {
            m_console.erase(m_console.begin(), m_console.begin() + MaxConsoleLines / 10);
        }
        m_console.push_back(std::move(line));
    }

    void WorkspaceApp::DrawConsolePanel()
    {
        if (ImGui::SmallButton("Clear"))
        {
            m_console.clear();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_runner.IsRunning());
        if (ImGui::SmallButton("Stop"))
        {
            StopOperation();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
        ImGui::InputTextWithHint("##filter", "filter lines", m_consoleFilter.data(), m_consoleFilter.size());
        if (m_runner.IsRunning())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s... %.0f s", NameOf(static_cast<int>(m_operation)), m_runner.Seconds());
        }
        ImGui::Separator();

        if (!ImGui::BeginChild("##output", { 0.0f, 0.0f }, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::EndChild();
            return;
        }
        const std::string_view filter = m_consoleFilter.data();
        std::vector<int> shown;
        shown.reserve(m_console.size());
        for (int i = 0; i < static_cast<int>(m_console.size()); ++i)
        {
            if (filter.empty() || AtomFramework::EntityInspector::Matches(m_console[static_cast<std::size_t>(i)].text, filter))
            {
                shown.push_back(i);
            }
        }
        // Only the visible lines are drawn: a build writes thousands.
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(shown.size()));
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const ConsoleLine& line = m_console[static_cast<std::size_t>(shown[static_cast<std::size_t>(row)])];
                ImGui::PushID(row);
                ImGui::PushStyleColor(ImGuiCol_Text, ColourOf(line.kind));
                if (line.file.empty())
                {
                    ImGui::TextUnformatted(line.text.c_str());
                }
                else
                {
                    ImGui::Selectable(line.text.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Double-click to open %s", line.file.c_str());
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            const std::string url = "file:///" + fs::path(line.file).generic_string();
                            SDL_OpenURL(url.c_str());
                        }
                    }
                }
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
        }
        // Follow the output while at the bottom; scrolling up stops it.
        if (m_consoleScroll || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        m_consoleScroll = false;
        ImGui::EndChild();
    }
}
