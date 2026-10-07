# Working on AtomEngine

- **Validate what changed, not the full matrix.** Per milestone:
  `pwsh Tools/Dev/check.ps1 -Level changed` - it maps the branch's changed
  files to scenarios (`Tools/Dev/changed.psd1`), builds Debug once and runs
  the unit tests plus those scenarios; `-DryRun` shows the plan. Inner
  loop: `-Level quick`, or `-Level feature -Scenario <names>`. `-Level full`
  (Debug + Release, every scenario) **once per version, before the release
  commit** - not at every milestone. No local CI-configuration build: CI
  runs it on the pull request. Debug for asserts and lifetime checks,
  Release for anything timed; `ab.ps1` only when a milestone touches
  performance. Docs-only changes: nothing to build. Never rebuild assets
  unless Blender scripts or content products changed. Add a rule to
  `changed.psd1` when you add a scenario or a folder.
- **Edit source with direct edits.** Never modify C++, HLSL or JSON
  containing escape sequences (`\n`, `\"`) through inline Python or shell
  heredocs: the extra quoting layers break them. Use an edit/patch
  operation, and look at the diff.
- **Never commit on master.** Check `git branch --show-current` first.
- Performance claims need paired measurements (`bench`, `Tools/Perf/ab.ps1`),
  plugged in, on the laptop's own screen.
