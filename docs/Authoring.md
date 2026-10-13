# Authoring content

How a level is made, which file says what, and who may edit it. For the
concepts behind each step, see the [technical manual](AtomEngine-Tech-Manual.md);
for where the pieces live, see [Architecture](Architecture.md).

## The contract: one authority per fact

Every fact about a level has exactly one file that decides it. Anything
else holding the same fact is generated from that file, and says so.

| Fact | Authority (edit this) | Generated from it (never edit) |
|---|---|---|
| Procedural geometry, collision, lighting bakes | the Python builders in `Tools/Blender/` (`atom_kit.py`, `atom_levels.py`, `atom_street.py`, ...) | `*.glb`, `*_col.glb`, `*_lm.png` in an app's `Assets/` or `Content/` |
| Hand-made geometry (v0.0.17) | a `.blend` scene in the app's `Source/` folder | the same outputs, through the same export |
| Where spawns and entities stand | a Blender **marker** (`spawn:<name>`, `entity:<name>`) *or* the level file's `position` / `yaw` | `<level>.markers.json` (has `"_generated"`) |
| What an entity *is* and *does* (model, collider, interaction, animation, mover) | the level file, `<app>/Assets/Levels/<level>.json` | nothing |
| Lighting, fog, sky, audio, sequences of a level | the level file | nothing |
| Environment presets | `Content/Environments/*.json` | nothing |
| A pachinko machine | `Tools/Machines/*_layout.py` | `Assets/Machines/*.json` |
| External models (third-party GLB) | the file as received, with its licence beside it | nothing |
| The rules every data file follows | `Framework/Schemas/*.schema.json`, an app's `Assets/Schemas/` - and, above them, the C++ parser (`Framework/Level/LevelData.cpp`) | nothing |

Where they live:

| Kind | Where | Committed |
|---|---|---|
| Source assets (Python builders, `.blend`, level JSON, third-party GLB) | `Tools/Blender/`, `<app>/Source/`, `<app>/Assets/Levels/`, `<app>/Assets/ThirdParty/` | yes |
| Generated runtime assets (GLB, lightmaps, markers) | `<app>/Assets/<folder>/`, `Content/<folder>/` | yes: CPU-baked, byte-identical on rebuild |
| Bake cache and previews | `build/bake_cache/` (fingerprints); `--gpu` lightmaps | no (GPU bakes must be redone on the CPU before a commit; a test refuses them) |
| Each app's runtime copy | `build/bin/<config>/<app>/Assets/` | no |
| Distribution payload | `Dist/<Game>/` | no |

```mermaid
graph LR
    Py[Python builders<br/>Tools/Blender] --> Export
    Blend[Hand-made .blend<br/>app Source/] --> Export
    Export[shared export<br/>+ AtomLevelCheck] --> Out[GLB + lightmaps<br/>+ markers.json]
    Json[level JSON<br/>hand-written] --> Runtime
    Out --> Runtime[AtomFramework<br/>ParseLevel, Level]
    Runtime --> Engine[AtomEngine<br/>renderer, physics, audio]
    Runtime --> F10[F10 entity report]
```

## Placement: marker or level file

A spawn or an entity can be placed in two ways:

- **By a marker**: an empty in Blender named `spawn:<name>` or
  `entity:<name>`, or carrying the `atom_marker_kind` / `atom_marker_name`
  properties that the builders' `add_marker` sets. The export writes it to
  `<level>.markers.json`, in game coordinates (x, z, -y).
- **In the level file**: a `position` (and `yaw`) written on the spawn or
  the entity.

When both exist, **the level file wins**. That has been the rule since
M20, and it stays: a value written by hand is a decision. Since v0.0.17
the level also *says so*: each overridden marker is a warning naming the
JSON Pointer of the value that wins and the marker it hides. Moving the
marker in Blender would change nothing until that value is removed.

| Level file | Marker | Result | Reported |
|---|---|---|---|
| `position` | none | the file's | - |
| none | `entity:<name>` | the marker's | - |
| `position` | `entity:<name>` | the file's | warning at `/entities/<i>/position` |
| `yaw` only | `entity:<name>` | the marker's position, the file's yaw | warning at `/entities/<i>/yaw` |
| none | none, but the level has markers | - | **error**: no position and no marker |
| none | none, and no markers file | the origin | warning if the entity has a model or collider |
| - | `entity:<name>` with no such entity | - | **error** in `<level>.markers.json` |

Errors stop the level from loading, and the content tests fail. Warnings
let it load: they explain behaviour that is supported but easy to miss.
Every loaded entity and spawn keeps where its position came from
(`PlacementSource`: file, marker, or file with the marker ignored).

## Checking a level: `AtomLevelCheck`

One command checks an app's levels the way the app would load them, plus
what only shows across files, without starting the game (no window, no
GPU):

```powershell
build\bin\Debug\AtomLevelCheck.exe --app demo              # every level of an app
build\bin\Debug\AtomLevelCheck.exe --app showcase showcase # one level
build\bin\Debug\AtomLevelCheck.exe --all                   # every app
```

Options: `--errors-only`, and `--no-clips` (don't open models to check
clips). For a folder outside the repository, use `--assets <dir> [--content <dir>]`.
The exit code is 0 when there are no errors (warnings are allowed), 1 on
errors and 2 on bad usage. Each problem names where to look:

```text
ERROR [broken]
  Entity: lamp
  Source: Levels/broken.json
  Property: /entities/0/model
  Problem: model "Kit/missing_lamp.glb" was not found
  Fix: check the path (relative to Assets/), or build it: Tools/Blender/export.ps1

WARNING [pachinko_hall]
  Entity: prize_counter
  Source: Levels/pachinko_hall.json
  Property: /entities/3/position
  Problem: the position written here overrides the entity:prize_counter marker; remove it to let the marker place it
```

What it checks:

| Check | Severity |
|---|---|
| everything the parser refuses: syntax with line and column, types, unknown actions, missing fields, a marker naming no entity, a sequence step naming no entity | error |
| the level's model, collision, lightmap, sky, chunk files, impostor descriptors, entity models, pachinko machines exist | error |
| a file found only in **another app's** assets | error: an app reads only its own `Assets/` and the shared `Content/` |
| an animation clip that its model doesn't have (entity animation, animator states, `playAnimation`) - the model is opened | error, listing the clips it has |
| two entities with one name | error |
| a collider with a zero or negative size | error |
| `changeLevel` to a level the app doesn't have, or to a spawn that level doesn't have | error |
| the level file overriding a marker | warning |
| something drawn or solid left at the origin | warning |
| a position more than 10 km from the origin | warning |

The checks live in the framework (`Framework/Level/LevelCheck.h`), and
the parser stays the authority on what a level file may say. CTest runs
the tool for every app (`LevelCheck.<owner>`, labels `cpu` and `content`),
CI included, and the apps' content tests call the same code.
