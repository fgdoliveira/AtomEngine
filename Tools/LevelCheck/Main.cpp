// AtomLevelCheck (v0.0.17, M112): checks an app's level files without
// starting the game - what the app's loader would refuse (errors) and what
// it would accept but probably wasn't meant (warnings), with the file,
// the entity and the JSON Pointer of each. The checks are the framework's
// (Framework/Level/LevelCheck.h); this is their command line.
//
//   AtomLevelCheck --project Apps/MyGame/MyGame.atomproject.json   (v0.0.19)
//   AtomLevelCheck --app demo                     every level of an app
//   AtomLevelCheck --app showcase showcase        one level
//   AtomLevelCheck --all                          every app
//   AtomLevelCheck --assets <dir> [--content <dir>] [level...]
//   options: --errors-only, --no-clips (don't open models: no clip or readability check)
//
// Exit code: 0 no errors (warnings allowed), 1 errors, 2 usage.
#include "Level/LevelCheck.h"
#include "Project/ProjectDiscovery.h"

#include <cctype>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace AtomFramework;

    // v0.0.19: the projects are found, not listed - every
    // <Name>.atomproject.json under Apps/ (AtomProject::FindProjects). The
    // short names the tests and docs use stay as aliases.
    struct App
    {
        std::string owner;   // "demo", "hello", ... (the alias), or the name in lower case
        std::string assets;
        std::string descriptor;
    };

    std::string OwnerOf(const std::string& name)
    {
        if (name == "HelloAtom") return "hello";
        if (name == "AtomArena") return "arena";
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower;
    }

    std::vector<App> FindApps(const fs::path& root)
    {
        std::vector<App> apps;
        const AtomProject::Discovery found = AtomProject::FindProjects((root / "Apps").string());
        for (const std::string& error : found.errors)
        {
            std::cerr << "AtomLevelCheck: " << error << '\n';
        }
        for (const AtomProject::DiscoveredProject& project : found.projects)
        {
            apps.push_back({ OwnerOf(project.descriptor.name), project.AssetsDir(), project.descriptorPath });
        }
        return apps;
    }

    int Usage()
    {
        std::cerr << "usage: AtomLevelCheck --project <Name.atomproject.json> [level...]\n"
                     "       AtomLevelCheck --app <name> [level...]   (a project under Apps/: showcase, demo, hello, arena, ...)\n"
                     "       AtomLevelCheck --all                      (every project under Apps/)\n"
                     "       AtomLevelCheck --assets <dir> [--content <dir>] [level...]\n"
                     "options: --root <repository>, --errors-only, --no-clips\n";
        return 2;
    }

    // Checks the levels (all of them when none is named) and prints the
    // diagnostics and a line per level. Returns the number of errors.
    std::size_t Run(const std::string& label, const LevelCheckOptions& options, const std::vector<std::string>& levels,
                    bool errorsOnly)
    {
        std::vector<std::string> names = levels;
        if (names.empty())
        {
            const fs::path folder = fs::path(options.assets.Roots().front()) / "Levels";
            if (fs::is_directory(folder))
            {
                for (const auto& entry : fs::directory_iterator(folder))
                {
                    const std::string file = entry.path().filename().string();
                    if (entry.path().extension() == ".json" && !file.ends_with(".markers.json"))
                    {
                        names.push_back(entry.path().stem().string());
                    }
                }
            }
            std::sort(names.begin(), names.end());
        }
        std::size_t errors = 0;
        for (const std::string& name : names)
        {
            const std::vector<LevelDiagnostic> found = CheckLevelFile(name, options);
            std::size_t warnings = 0;
            for (const LevelDiagnostic& diagnostic : found)
            {
                const bool error = diagnostic.severity == LevelDiagnostic::Severity::Error;
                errors += error ? 1 : 0;
                warnings += error ? 0 : 1;
                if (error || !errorsOnly)
                {
                    std::cout << FormatDiagnostic(diagnostic) << '\n';
                }
            }
            const std::size_t levelErrors = CountErrors(found);
            std::cout << (levelErrors ? "FAIL " : "ok   ") << label << '/' << name << ": " << levelErrors << " error(s), "
                      << warnings << " warning(s)\n";
        }
        if (names.empty())
        {
            std::cout << "ok   " << label << ": no level files\n";
        }
        return errors;
    }

    std::vector<OtherAppAssets> OtherApps(const std::vector<App>& apps, const std::string& except)
    {
        std::vector<OtherAppAssets> others;
        for (const App& app : apps)
        {
            if (app.owner != except)
            {
                others.push_back({ app.owner, app.assets });
            }
        }
        return others;
    }
}

int main(int argc, char** argv)
{
    fs::path root = ATOM_SOURCE_DIR;
    std::string app, assets, content, project;
    std::vector<std::string> levels;
    bool all = false;
    bool errorsOnly = false;
    bool clips = true;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--app" && hasValue) app = argv[++i];
        else if (arg == "--project" && hasValue) project = argv[++i];
        else if (arg == "--assets" && hasValue) assets = argv[++i];
        else if (arg == "--content" && hasValue) content = argv[++i];
        else if (arg == "--root" && hasValue) root = argv[++i];
        else if (arg == "--all") all = true;
        else if (arg == "--errors-only") errorsOnly = true;
        else if (arg == "--no-clips") clips = false;
        else if (arg.rfind("--", 0) == 0) return Usage();
        else levels.push_back(arg);
    }
    if (content.empty())
    {
        content = (root / "Content").string();
    }

    std::size_t errors = 0;
    if (!project.empty())
    {
        // A project anywhere: its descriptor says where its assets are; the
        // projects under Apps/ (if any) are the others it may not borrow from.
        const AtomProject::DescriptorLoad load = AtomProject::LoadDescriptor(project);
        if (!load.descriptor)
        {
            std::cerr << "AtomLevelCheck: " << load.error << '\n';
            return 2;
        }
        const std::string owner = OwnerOf(load.descriptor->name);
        LevelCheckOptions options;
        options.assets = AssetRoots({ (fs::path(project).parent_path() / load.descriptor->assets).string(), content });
        options.otherApps = OtherApps(FindApps(root), owner);
        options.checkClips = clips;
        errors += Run(owner, options, levels, errorsOnly);
    }
    else if (all || !app.empty())
    {
        const std::vector<App> apps = FindApps(root);
        bool known = false;
        for (const App& candidate : apps)
        {
            if (!all && app != candidate.owner)
            {
                continue;
            }
            known = true;
            LevelCheckOptions options;
            options.assets = AssetRoots({ candidate.assets, content });
            options.otherApps = OtherApps(apps, candidate.owner);
            options.checkClips = clips;
            errors += Run(candidate.owner, options, all ? std::vector<std::string>{} : levels, errorsOnly);
        }
        if (!known)
        {
            std::cerr << "AtomLevelCheck: no project \"" << app << "\" under " << (root / "Apps").string() << '\n';
            return Usage();
        }
    }
    else if (!assets.empty())
    {
        LevelCheckOptions options;
        options.assets = AssetRoots({ assets, content });
        options.checkClips = clips;
        errors += Run(fs::path(assets).parent_path().filename().string(), options, levels, errorsOnly);
    }
    else
    {
        return Usage();
    }
    std::cout << (errors ? "FAILED: " : "PASS: ") << errors << " error(s)\n";
    return errors ? 1 : 0;
}
