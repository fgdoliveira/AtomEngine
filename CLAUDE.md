# Working on AtomEngine

- **Validate what changed, not the full matrix.** Per milestone:
  `pwsh Tools/Dev/check.ps1 -Level changed` - it maps the branch's changed
  files to scenarios (`Tools/Dev/changed.psd1`), builds Debug once and runs
  the unit tests plus those scenarios; `-DryRun` shows the plan. Inner
  loop: `-Level quick`, or `-Level feature -Scenario <names>`. `-Level full`
  (Debug + Release, every scenario) **once per version, before the release
  commit** - not at every milestone. Debug for asserts and lifetime checks,
  Release for anything timed; `ab.ps1` only when a milestone touches
  performance. Docs-only changes: nothing to build. Never rebuild assets
  unless Blender scripts or content products changed. Add a rule to
  `changed.psd1` when you add a scenario or a folder.
- **Run the CI job a change can break, locally, with act**
  (https://nektosact.com/usage/index.html) - after touching CMake files,
  build options or `.github/workflows/ci.yml`. CI configures differently
  (`ATOM_BUILD_GAME=OFF`: no apps, no shaders), which `check.ps1` never
  does. Linux job (`privacy`):
  `act -j <job id> -P ubuntu-latest=ghcr.io/catthehacker/ubuntu:pwsh-latest`.
  Windows jobs (`unit-tests`, `package`) don't run in a container: add
  `-P windows-latest=-self-hosted` (runs them on this machine).
- **Edit source with direct edits.** Never modify C++, HLSL or JSON
  containing escape sequences (`\n`, `\"`) through inline Python or shell
  heredocs: the extra quoting layers break them. Use an edit/patch
  operation, and look at the diff.
- **Never commit on master.** Check `git branch --show-current` first.
- Performance claims need paired measurements (`bench`, `Tools/Perf/ab.ps1`),
  plugged in, on the laptop's own screen.
