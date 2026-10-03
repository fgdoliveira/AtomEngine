# Working on AtomEngine

- **Validate with the ladder, not the full matrix.** After a localized
  change, build one configuration incrementally and run the relevant tests:
  `pwsh Tools/Dev/check.ps1 -Level quick|feature -Scenario <names>`
  (see README, Development workflow). Debug for asserts and lifetime
  checks, Release for anything timed. `-Level full` (Debug + Release, every
  scenario) once, before a milestone or release commit. Docs-only changes:
  nothing to build. Never rebuild assets unless Blender scripts or content
  products changed.
- **Edit source with direct edits.** Never modify C++, HLSL or JSON
  containing escape sequences (`\n`, `\"`) through inline Python or shell
  heredocs: the extra quoting layers break them. Use an edit/patch
  operation, and look at the diff.
- **Never commit on master.** Check `git branch --show-current` first.
- Performance claims need paired measurements (`bench`, `Tools/Perf/ab.ps1`),
  plugged in, on the laptop's own screen.
