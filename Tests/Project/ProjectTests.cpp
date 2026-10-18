#include "Project/ProjectCreator.h"
#include "Project/ProjectDescriptor.h"
#include "Project/ProjectDiscovery.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace AtomProject;
namespace fs = std::filesystem;

namespace
{
    // A fresh workspace under the temp folder, with a space in its path
    // (as "Engine Programmer" has), removed at the end of each test.
    struct TempWorkspace
    {
        fs::path root = fs::temp_directory_path() / "atom project tests" / "my workspace";
        TempWorkspace()
        {
            fs::remove_all(root.parent_path());
            fs::create_directories(root);
        }
        ~TempWorkspace()
        {
            std::error_code ignored;
            fs::remove_all(root.parent_path(), ignored);
        }
    };

    std::string Read(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        return text.str();
    }
}

TEST_CASE("A project name is a letter, then letters, digits or '_', and never a Windows device")
{
    CHECK(CheckProjectName("MyGame").empty());
    CHECK(CheckProjectName("my_game_2").empty());
    CHECK_FALSE(CheckProjectName("").empty());
    CHECK_FALSE(CheckProjectName("2Fast").empty());     // a digit first
    CHECK_FALSE(CheckProjectName("_hidden").empty());   // '_' first
    CHECK_FALSE(CheckProjectName("My Game").empty());   // a space
    CHECK_FALSE(CheckProjectName("my-game").empty());   // a dash
    CHECK_FALSE(CheckProjectName("..").empty());
    CHECK_FALSE(CheckProjectName("a/b").empty());
    CHECK_FALSE(CheckProjectName(std::string(65, 'a')).empty());
    for (const char* reserved : { "CON", "con", "Nul", "COM1", "lpt9", "aux", "PRN" })
    {
        INFO(reserved);
        CHECK(CheckProjectName(reserved).find("reserved") != std::string::npos);
    }
    CHECK(CheckProjectName("Console").empty()); // only the exact device names
}

TEST_CASE("A descriptor round-trips, keeps no machine paths, and refuses paths leaving the project")
{
    ProjectDescriptor d;
    d.name = "MyGame";
    d.engine = "0.0.19";
    d.executable = "MyGame";
    d.startupLevel = "first";
    const DescriptorLoad load = ParseDescriptor(WriteDescriptor(d));
    INFO(load.error);
    REQUIRE(load.descriptor);
    CHECK(load.descriptor->name == "MyGame");
    CHECK(load.descriptor->source == "Source");
    CHECK(load.descriptor->assets == "Assets");
    CHECK(load.descriptor->startupLevel == "first");

    CHECK(ParseDescriptor(R"({ "schema": 1, "name": "A", "assets": "C:/Users/x/Assets" })").error.find("/assets") != std::string::npos);
    CHECK(ParseDescriptor(R"({ "schema": 1, "name": "A", "source": "../elsewhere" })").error.find("/source") != std::string::npos);
    CHECK(ParseDescriptor(R"({ "schema": 1, "name": "A", "assets": "/abs" })").error.find("/assets") != std::string::npos);
    CHECK(ParseDescriptor(R"({ "schema": 2, "name": "A" })").error.find("schema") != std::string::npos);
    CHECK(ParseDescriptor(R"({ "schema": 1, "name": "9" })").error.find("/name") != std::string::npos);
    CHECK_FALSE(ParseDescriptor("{ not json").descriptor);
}

TEST_CASE("Creating a project makes its folders, descriptor and build file - and no code")
{
    TempWorkspace workspace;
    const CreateResult result = CreateProject({ workspace.root.string(), "MyGame", "0.0.19" });
    INFO(result.error);
    REQUIRE(result.ok);
    const fs::path project = workspace.root / "Apps" / "MyGame";
    CHECK(fs::path(result.projectDir) == project);
    for (const std::string& folder : ProjectFolders())
    {
        INFO(folder);
        CHECK(fs::is_directory(project / folder));
    }
    CHECK_FALSE(fs::exists(project / "Source" / "Main.cpp")); // no gameplay, no Main.cpp
    const DescriptorLoad descriptor = LoadDescriptor(result.descriptorPath);
    REQUIRE(descriptor.descriptor);
    CHECK(descriptor.descriptor->engine == "0.0.19");
    const std::string cmake = Read(project / "CMakeLists.txt");
    CHECK(cmake.find("if(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/Source/Main.cpp\")") != std::string::npos);
    CHECK(cmake.find("add_executable(MyGame") != std::string::npos);
    CHECK(fs::exists(project / "README.md"));
    // Nothing machine-specific in what's written.
    CHECK(Read(result.descriptorPath).find(workspace.root.string()) == std::string::npos);
    // No staging folder left behind.
    CHECK_FALSE(fs::exists(workspace.root / "Apps" / ".MyGame.creating"));
}

TEST_CASE("Projects are found by their descriptors, one or two folders down; broken ones are reported")
{
    TempWorkspace workspace;
    REQUIRE(CreateProject({ workspace.root.string(), "Zeta", "0.0.19" }).ok);
    REQUIRE(CreateProject({ workspace.root.string(), "Alpha", "0.0.19" }).ok);
    // A grouped example, two folders down, and a broken descriptor.
    fs::create_directories(workspace.root / "Apps" / "Games" / "Old");
    std::ofstream(workspace.root / "Apps" / "Games" / "Old" / "Old.atomproject.json")
        << R"({ "schema": 1, "name": "Old", "source": "." })";
    fs::create_directories(workspace.root / "Apps" / "Broken");
    std::ofstream(workspace.root / "Apps" / "Broken" / "Broken.atomproject.json") << "{ nope";

    const Discovery found = FindProjects((workspace.root / "Apps").string());
    REQUIRE(found.projects.size() == 3);
    CHECK(found.projects[0].descriptor.name == "Alpha"); // sorted
    CHECK(found.projects[1].descriptor.name == "Old");
    CHECK(found.projects[2].descriptor.name == "Zeta");
    CHECK(fs::path(found.projects[0].AssetsDir()) == (workspace.root / "Apps" / "Alpha" / "Assets").lexically_normal());
    REQUIRE(found.errors.size() == 1);
    CHECK(found.errors.front().find("Broken.atomproject.json") != std::string::npos);

    CHECK(FindProjects((workspace.root / "Apps").string(), 1).projects.size() == 2); // new projects only
}

TEST_CASE("Creating never overwrites, and refuses bad names and workspaces")
{
    TempWorkspace workspace;
    REQUIRE(CreateProject({ workspace.root.string(), "MyGame", "0.0.19" }).ok);
    std::ofstream(workspace.root / "Apps" / "MyGame" / "Source" / "Main.cpp") << "// mine";

    const CreateResult again = CreateProject({ workspace.root.string(), "MyGame", "0.0.19" });
    CHECK_FALSE(again.ok);
    CHECK(again.error.find("never overwritten") != std::string::npos);
    CHECK(Read(workspace.root / "Apps" / "MyGame" / "Source" / "Main.cpp") == "// mine"); // untouched

    const CreateResult otherCase = CreateProject({ workspace.root.string(), "mygame", "0.0.19" });
    CHECK_FALSE(otherCase.ok); // the same folder on Windows (a case-insensitive disk says so first)
    CHECK(otherCase.error.find("already exists") != std::string::npos);

    CHECK_FALSE(CreateProject({ workspace.root.string(), "CON", "0.0.19" }).ok);
    CHECK_FALSE(CreateProject({ workspace.root.string(), "../Escape", "0.0.19" }).ok);
    CHECK_FALSE(fs::exists(workspace.root / "Escape"));
    CHECK_FALSE(CreateProject({ (workspace.root / "missing").string(), "Fine", "0.0.19" }).ok);
}
