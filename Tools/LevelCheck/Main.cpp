// AtomLevelCheck (v0.0.17, M112): checks an app's level files without
// starting the game - what the app's loader would refuse (errors) and what
// it would accept but probably wasn't meant (warnings), with the file,
// the entity and the JSON Pointer of each. The checks are the framework's
// (Framework/Level/LevelCheck.h); this is their command line.
//
//   AtomLevelCheck --app demo                     every level of an app
//   AtomLevelCheck --app showcase showcase        one level
//   AtomLevelCheck --all                          every app
//   AtomLevelCheck --assets <dir> [--content <dir>] [level...]
//   options: --errors-only, --no-clips (don't open models: no clip or readability check)
//
// Exit code: 0 no errors (warnings allowed), 1 errors, 2 usage.
#include "Level/LevelCheck.h"

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

    struct App
    {
        const char* owner;
        const char* assets; // relative to the repository
    };

    // The apps whose levels are checked (Apps/, v0.0.16). DRIFT has no
    // level files.
    constexpr App Apps[] = {
        { "showcase", "Apps/Showcase/Assets" },
        { "demo", "Apps/Games/Demo/Assets" },
        { "hello", "Apps/Samples/HelloAtom/Assets" },
        { "arena", "Apps/Benchmarks/AtomArena/Assets" },
    };

    int Usage()
    {
        std::cerr << "usage: AtomLevelCheck --app <showcase|demo|hello|arena> [level...]\n"
                     "       AtomLevelCheck --all\n"
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

    std::vector<OtherAppAssets> OtherApps(const fs::path& root, const char* except)
    {
        std::vector<OtherAppAssets> others;
        for (const App& app : Apps)
        {
            if (std::strcmp(app.owner, except) != 0)
            {
                others.push_back({ app.owner, (root / app.assets).string() });
            }
        }
        return others;
    }
}

int main(int argc, char** argv)
{
    fs::path root = ATOM_SOURCE_DIR;
    std::string app, assets, content;
    std::vector<std::string> levels;
    bool all = false;
    bool errorsOnly = false;
    bool clips = true;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--app" && hasValue) app = argv[++i];
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
    if (all || !app.empty())
    {
        bool known = false;
        for (const App& candidate : Apps)
        {
            if (!all && app != candidate.owner)
            {
                continue;
            }
            known = true;
            LevelCheckOptions options;
            options.assets = AssetRoots({ (root / candidate.assets).string(), content });
            options.otherApps = OtherApps(root, candidate.owner);
            options.checkClips = clips;
            errors += Run(candidate.owner, options, all ? std::vector<std::string>{} : levels, errorsOnly);
        }
        if (!known)
        {
            std::cerr << "AtomLevelCheck: no app \"" << app << "\"\n";
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
