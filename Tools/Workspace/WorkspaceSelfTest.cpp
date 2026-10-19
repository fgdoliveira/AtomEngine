// The workspace's self-test (v0.0.19, M126): what the games' F10
// scenarios checked (demo.devtools, hello_inspect, the inspect and sky
// lines of hello_room and showcase_lens), now where the tools live.
//
//   AtomWorkspace --test <folder>     (Scenario.workspace.workspace_self_test)
//
// It drives the real workspace - the same functions the menus and panels
// call, a step a frame (or as many as a step waits) - and quits with 0
// when every check passed. A failed check is logged and the test goes on,
// so one run reports everything that broke.
#include "WorkspaceApp.h"

#include "Core/Log.h"
#include "Level/Level.h"
#include "Project/ProjectCreator.h"

namespace fs = std::filesystem;

namespace AtomWorkspace
{
    bool WorkspaceApp::Check(bool passed, const std::string& what)
    {
        if (passed)
        {
            ATOM_LOG_INFO("Workspace test") << "ok   " << what << '\n';
        }
        else
        {
            ++m_testFailures;
            ATOM_LOG_ERROR("Workspace test") << "FAIL " << what << '\n';
        }
        return passed;
    }

    void WorkspaceApp::RunSelfTest()
    {
        ++m_testFrames;
        const auto next = [this] {
            ++m_testStep;
            m_testFrames = 0;
        };
        const auto selectLevel = [this](const std::string& name) {
            const auto found = std::find(m_project->levels.begin(), m_project->levels.end(), name);
            if (found != m_project->levels.end())
            {
                SelectLevel(static_cast<int>(found - m_project->levels.begin()));
            }
        };
        const fs::path apps = fs::path(m_engineRoot) / "Apps";

        switch (m_testStep)
        {
        case 0:
            if (!Check(!m_engineRoot.empty(), "the engine's source tree is found beside the workspace"))
            {
                m_testStep = 100; // nothing else can run
                return;
            }
            Check(OpenProject((apps / "Games" / "Demo" / "Demo.atomproject.json").string()) && m_project
                      && m_project->descriptor.name == "Demo",
                  "open the Demo");
            next();
            break;
        case 1:
            if (m_project)
            {
                selectLevel("street");
            }
            Check(m_preview->GetLevel() && m_preview->GetLevel()->GetName() == "street",
                  "preview the street without the game");
            next();
            break;
        case 2:
            // A few frames for the Scene panel to size the offscreen target.
            if (m_testFrames < 10)
            {
                return;
            }
            Check(GetRenderer().GetSceneTargetWidth() > 16 && GetRenderer().GetSceneTargetHeight() > 16,
                  "the Scene panel shows the offscreen frame at its size");
            next();
            break;
        case 3:
        {
            AtomFramework::Level* level = m_preview->GetLevel();
            if (level)
            {
                m_inspector.Select(level->GetName(), "shrine_keeper");
            }
            const std::size_t index = level ? m_inspector.Resolve(level->GetName(), level->GetData()) : AtomFramework::EntityInspector::npos;
            Check(index != AtomFramework::EntityInspector::npos
                      && !AtomFramework::EntityInspector::Describe(level->GetData(), index, nullptr).empty(),
                  "select an entity and inspect it (shrine_keeper)");
            const glm::vec3 before = m_preview->Camera().GetPosition();
            FrameSelection();
            Check(glm::length(m_preview->Camera().GetPosition() - before) > 0.01f, "frame the selection");
            next();
            break;
        }
        case 4:
            ApplyViewSwitch("sky_picture", "off");
            Check(!m_preview->View().skyPicture, "turn the sky picture off");
            ApplyViewSwitch("sky_picture", "on");
            Check(m_preview->View().skyPicture, "and on again");
            next();
            break;
        case 5:
        {
            const std::vector<std::string> presets = m_preview->Presets();
            if (Check(!presets.empty(), "the environment presets are offered"))
            {
                Check(m_preview->SetEnvironment(presets.front(), 0.0f) && m_preview->EnvironmentName() == presets.front(),
                      "switch to the environment " + presets.front());
                Check(m_preview->SetEnvironment("level", 0.0f) && m_preview->EnvironmentName().empty(),
                      "back to the level's own light");
            }
            next();
            break;
        }
        case 6:
            Check(m_preview->Reload().empty() && m_preview->GetLevel(), "reload the level");
            selectLevel("shore");
            Check(m_preview->GetLevel() && m_preview->GetLevel()->GetName() == "shore", "switch to another level (shore)");
            next();
            break;
        case 7:
        {
            // A project made the workspace's way, then opened: it has no
            // Main.cpp, so Run is disabled and says why.
            std::error_code ignored;
            fs::remove_all(fs::path(m_startup.testFolder) / "WorkspaceTest", ignored);
            fs::create_directories(m_startup.testFolder, ignored);
            const AtomProject::CreateResult created =
                AtomProject::CreateProject({ m_startup.testFolder, "WorkspaceTest", ATOM_VERSION, m_engineRoot });
            if (Check(created.ok, "create a project (" + (created.ok ? std::string("WorkspaceTest") : created.error) + ")"))
            {
                Check(OpenProject(created.descriptorPath) && m_project->descriptor.name == "WorkspaceTest", "open the new project");
                Check(WhyNot(Operation::Run).find("Main.cpp") != std::string::npos, "Run is disabled without Source/Main.cpp");
                Check(!m_preview->GetLevel(), "a project without levels shows none");
            }
            next();
            break;
        }
        case 8:
            // An operation: AtomLevelCheck on HelloAtom, as a process.
            Check(OpenProject((apps / "Samples" / "HelloAtom" / "HelloAtom.atomproject.json").string()), "open HelloAtom");
            if (Check(WhyNot(Operation::Validate).empty(), "Validate is available"))
            {
                StartOperation(Operation::Validate);
                next();
            }
            else
            {
                m_testStep = 100;
            }
            break;
        case 9:
            if (!m_lastExitCode && m_testFrames < 60 * 60)
            {
                return; // the UI keeps drawing while it runs
            }
            Check(m_lastExitCode && *m_lastExitCode == 0, "Validate HelloAtom in a separate process (exit code 0)");
            Check(!m_console.empty() && m_console.back().kind == LineKind::Note, "its output and exit status reach the Console");
            m_testStep = 100;
            break;
        default:
            ATOM_LOG_INFO("Workspace test") << (m_testFailures ? "FAILED: " : "PASS: ") << m_testFailures << " failure(s)\n";
            RequestQuit(m_testFailures ? 1 : 0);
            m_testStep = 101;
            break;
        }
    }
}
