# Working on AtomEngine

- **Validate what changed, not the full matrix.** Per milestone:
  `pwsh Tools/Dev/check.ps1 -Level changed` - it maps the branch's changed
  files to owners (`Tools/Dev/changed.psd1`: engine, framework, content,
  each app), builds only their test targets and apps in Debug, and runs
  their CPU tests, the named scenarios and, for an engine or framework
  change, every app's smoke scenario; `-DryRun` shows the plan. Inner
  loop: `-Level quick`, or `-Level feature -Scenario <names>`. `-Level full`
  (Debug + Release, every scenario) **once per version, before the release
  commit** - not at every milestone. Debug for asserts and lifetime checks,
  Release for anything timed; `ab.ps1` only when a milestone touches
  performance. Docs-only changes: nothing to build. Never rebuild assets
  unless Blender scripts or content products changed - then export only
  the unit you changed (`pwsh Tools/Blender/export.ps1 -Only <unit>`,
  `-What markers` for markers; it runs `AtomLevelCheck`). After editing a
  level file, `AtomLevelCheck --app <app>`. A `.blend` is cleaned before
  it is committed (`Tools/Blender/clean_scene.ps1`). Add a rule to
  `changed.psd1` when you add a scenario, a test target or a folder.
- **No local CI-configuration build** - except its configure step after
  touching CMake files or `.github/workflows/ci.yml`: CI configures with
  `ATOM_BUILD_GAME=OFF` (no apps, no shaders), which `check.ps1` never
  does. `cmake -B build-ci -S . -DATOM_BUILD_GAME=OFF -DATOM_BUILD_TESTS=ON`
  (seconds), then delete `build-ci`.
- **Edit source with direct edits.** Never modify C++, HLSL or JSON
  containing escape sequences (`\n`, `\"`) through inline Python or shell
  heredocs: the extra quoting layers break them. Use an edit/patch
  operation, and look at the diff.
- **Never commit on master.** Check `git branch --show-current` first.
- Performance claims need paired measurements (`bench`, `Tools/Perf/ab.ps1`),
  plugged in, on the laptop's own screen.
