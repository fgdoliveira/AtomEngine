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

    CHECK(ParseDescriptor(R"({ "schema": 1, "name": "A", "assets": "D:/Games/Assets" })").error.find("/assets") != std::string::npos);
    CHECK(ParseDescriptor(R"({ "schema": 1, "name": "A", "source": "../elsewhere" })").error.find("/source") != std::string::npos);
    CHECK(ParseDescriptor(R"({ "schema": 1, "name": "A", "assets": "/abs" })").error.find("/assets") != std::string::npos);
    CHECK(ParseDescriptor(R"({ "schema": 2, "name": "A" })").error.find("schema") != std::string::npos);
    CHECK(ParseDescriptor(R"({ "schema": 1, "name": "9" })").error.find("/name") != std::string::npos);
    CHECK_FALSE(ParseDescriptor("{ not json").descriptor);
}

TEST_CASE("Creating a project makes its folders, descriptor and its own build - and no code")
{
    TempWorkspace workspace;
    const CreateResult result = CreateProject({ workspace.root.string(), "MyGame", "0.0.20" });
    INFO(result.error);
    REQUIRE(result.ok);
    const fs::path project = workspace.root / "MyGame"; // v0.0.20: anywhere, no Apps/
    CHECK(fs::path(result.projectDir) == project);
    for (const std::string& folder : ProjectFolders())
    {
        INFO(folder);
        CHECK(fs::is_directory(project / folder));
    }
    CHECK_FALSE(fs::exists(project / "Source" / "Main.cpp")); // no gameplay, no Main.cpp
    const DescriptorLoad descriptor = LoadDescriptor(result.descriptorPath);
    REQUIRE(descriptor.descriptor);
    CHECK(descriptor.descriptor->engine == "0.0.20");
    // Its own top-level build: the engine below it, the executable from the descriptor.
    const std::string cmake = Read(project / "CMakeLists.txt");
    CHECK(cmake.find("project(MyGame LANGUAGES CXX)") != std::string::npos);
    CHECK(cmake.find("add_subdirectory(\"${ATOM_ENGINE_DIR}\"") != std::string::npos);
    CHECK(cmake.find("atom_add_project(") != std::string::npos);
    CHECK(Read(project / "CMakePresets.json").find("\"name\": \"atom-base\"") != std::string::npos);
    const std::string ignored = Read(project / ".gitignore");
    CHECK(ignored.find("build/") != std::string::npos);
    CHECK(ignored.find("CMakeUserPresets.json") != std::string::npos);
    CHECK(fs::exists(project / "README.md"));
    // No engine named: no user presets; nothing machine-specific anywhere.
    CHECK_FALSE(fs::exists(project / "CMakeUserPresets.json"));
    for (const char* file : { "MyGame.atomproject.json", "CMakeLists.txt", "CMakePresets.json", "README.md" })
    {
        INFO(file);
        CHECK(Read(project / file).find(workspace.root.string()) == std::string::npos);
    }
    // No staging folder left behind.
    CHECK_FALSE(fs::exists(workspace.root / ".MyGame.creating"));
}

TEST_CASE("The engine's folder goes only in CMakeUserPresets.json, with forward slashes")
{
    TempWorkspace workspace;
    const fs::path engine = workspace.root / "the engine";
    const CreateResult result = CreateProject({ workspace.root.string(), "MyGame", "0.0.20", engine.string() });
    INFO(result.error);
    REQUIRE(result.ok);
    const std::string user = Read(fs::path(result.projectDir) / "CMakeUserPresets.json");
    CHECK(user.find("\"inherits\": \"atom-base\"") != std::string::npos);
    CHECK(user.find("\"ATOM_ENGINE_DIR\": \"" + engine.generic_string() + "\"") != std::string::npos);
    CHECK(user.find('\\') == std::string::npos); // no backslash to escape in JSON
    CHECK(Read(fs::path(result.projectDir) / "CMakePresets.json").find(engine.generic_string()) == std::string::npos);
    // A quote in a path stays valid JSON.
    CHECK(UserPresets("D:/odd\"name").find("D:/odd\\\"name") != std::string::npos);
}

TEST_CASE("Projects are found by their descriptors, one or two folders down; broken ones are reported")
{
    TempWorkspace workspace;
    REQUIRE(CreateProject({ workspace.root.string(), "Zeta", "0.0.20" }).ok);
    REQUIRE(CreateProject({ workspace.root.string(), "Alpha", "0.0.20" }).ok);
    // A grouped example, two folders down, and a broken descriptor.
    fs::create_directories(workspace.root / "Games" / "Old");
    std::ofstream(workspace.root / "Games" / "Old" / "Old.atomproject.json") << R"({ "schema": 1, "name": "Old", "source": "." })";
    fs::create_directories(workspace.root / "Broken");
    std::ofstream(workspace.root / "Broken" / "Broken.atomproject.json") << "{ nope";

    const Discovery found = FindProjects(workspace.root.string());
    REQUIRE(found.projects.size() == 3);
    CHECK(found.projects[0].descriptor.name == "Alpha"); // sorted
    CHECK(found.projects[1].descriptor.name == "Old");
    CHECK(found.projects[2].descriptor.name == "Zeta");
    CHECK(fs::path(found.projects[0].AssetsDir()) == (workspace.root / "Alpha" / "Assets").lexically_normal());
    REQUIRE(found.errors.size() == 1);
    CHECK(found.errors.front().find("Broken.atomproject.json") != std::string::npos);

    CHECK(FindProjects(workspace.root.string(), 1).projects.size() == 2); // one folder down only
}

TEST_CASE("Creating never overwrites, and refuses bad names and locations")
{
    TempWorkspace workspace;
    REQUIRE(CreateProject({ workspace.root.string(), "MyGame", "0.0.20" }).ok);
    std::ofstream(workspace.root / "MyGame" / "Source" / "Main.cpp") << "// mine";

    const CreateResult again = CreateProject({ workspace.root.string(), "MyGame", "0.0.20" });
    CHECK_FALSE(again.ok);
    CHECK(again.error.find("never overwritten") != std::string::npos);
    CHECK(Read(workspace.root / "MyGame" / "Source" / "Main.cpp") == "// mine"); // untouched

    const CreateResult otherCase = CreateProject({ workspace.root.string(), "mygame", "0.0.20" });
    CHECK_FALSE(otherCase.ok); // the same folder on Windows (a case-insensitive disk says so first)
    CHECK(otherCase.error.find("already exists") != std::string::npos);

    CHECK_FALSE(CreateProject({ workspace.root.string(), "CON", "0.0.20" }).ok);
    CHECK_FALSE(CreateProject({ workspace.root.string(), "../Escape", "0.0.20" }).ok);
    CHECK_FALSE(fs::exists(workspace.root.parent_path() / "Escape"));
    CHECK_FALSE(CreateProject({ (workspace.root / "missing").string(), "Fine", "0.0.20" }).ok);
}
