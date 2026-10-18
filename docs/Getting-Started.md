# Getting started: your first AtomEngine app

This walks through **`Apps/Samples/HelloAtom`**, the smallest complete app on
AtomEngine: a window, a level loaded from data, first-person walking, the
engine's numbers and the scenario harness, in about 150 lines
(`Apps/Samples/HelloAtom/Main.cpp`). Everything the Showcase and the
games do is built on the same pieces. Section 7 makes a project of your
own in **Atom Workspace**, the engine's development program.

## 1. Build and run it

Requirements are the engine's (see the README, *Building*): Windows 10/11,
Visual Studio 2022 or later with the C++ tools, CMake 3.26+, and `dxc`.

```powershell
git clone --recursive https://github.com/fgdoliveira/AtomEngine.git
cd AtomEngine
cmake -B build -S .
cmake --build build --config Release --target HelloAtom
build\bin\Release\HelloAtom\HelloAtom.exe
```

Each app runs from a folder of its own (`build\bin\<config>\<app>\`):
the executable, `SDL3.dll`, the engine's shaders and the app's assets.

WASD and the mouse walk, Shift jogs. **F1** shows the engine's numbers
(frame time, draws, the GPU), **F2-F8** switch what's drawn (render scale,
baked light, MSAA, fog, shadows, the post look), **Esc** quits. The
developer tools aren't in the app: they're in the workspace (section 7).

## 2. The three layers

```
Engine/      Atom::            the window, GPU renderer, audio, input, physics, UI drawing
Framework/   AtomFramework::   levels from JSON, the entity world, the player, environments,
                               the UI kit and menus, settings, diagnostics, the scenario harness
your app     (Apps/: Showcase, Games/*, Samples/*, Benchmarks/*)  what makes it yours
```

An app links `AtomEngine` and `AtomFramework`. The engine knows nothing of
levels or players; the framework knows nothing of any one app. What only
your app has (named sounds, live screens, dialogue) comes in through small
interfaces such as `SoundLibrary` and `ScreenFactory`.

## 3. The app, step by step

**An `Application`.** Derive from `Atom::Application` and override three
hooks: `OnInitialize` (once, with the window and GPU ready), `OnUpdate`
(every frame) and `OnShutdown`. `main` makes one and calls `Run()`.

```cpp
class HelloApp : public Atom::Application
{
protected:
    bool OnInitialize() override;          // return false to stop
    void OnUpdate(float deltaSeconds) override;
    void OnShutdown() override;
};

int main(int, char**) { HelloApp app; return app.Run(); }
```

**Where assets are.** `AssetRoots` looks a relative path ("Kit/torii.glb")
up in several folders in order: the app's own first, then the shared ones.
The build copies `Apps/Samples/HelloAtom/Assets` to `Assets/HelloAtom/` beside
the executable, and the pieces it uses from `Content/` (the first-render
stage, the kit) to `Assets/` - each named in its `CMakeLists.txt`
(`atom_app_payload`). Its folder holds nothing else, so a piece it forgets
to name is missing at once, not found in another app's copy.

```cpp
m_assets = AssetRoots({ m_root + "Assets/HelloAtom/", m_root + "Assets/" });
```

**A level from data.** A `LevelManager` loads `Levels/<name>.json`: the
ground model and its collision, spawn points, lighting, and a list of
entities (a model, a position, optionally an animation, a collider, an
animator). `onLoaded` is where the app puts its player at the spawn.

```cpp
m_levels = std::make_unique<LevelManager>(
    Level::Services{ GetRenderer(), GetAudio(), m_sounds, m_assets, m_models, ScreenFactory{} });
m_levels->onLoaded = [this](Level&, const SpawnPoint& spawn) {
    m_player.Teleport(spawn.position, m_camera);
    ApplyLighting();
};
m_levels->Load("hello", "");
```

**The level file.** `Apps/Samples/HelloAtom/Assets/Levels/hello.json` is
validated by `Framework/Schemas/level.schema.json` (editors with JSON
Schema support complete and check it as you type). Adding something to
the world is adding an entry:

```json
{ "name": "torii", "position": [0.0, 0.0, -6.0], "model": "Kit/torii.glb" }
```

**Every frame.** Read input into a `MoveIntent`, let the `PlayerController`
move against the level's collision (it also turns the camera with the
mouse), advance the level, then hand the renderer the camera and the
level's draws. The renderer does the rest (shadows, fog, sky, post) when
the frame ends.

```cpp
m_player.Update(input, intent, m_camera, &level->GetCollision(), deltaSeconds);
renderer.SetCamera(m_camera.GetViewMatrix(), m_camera.verticalFov, m_camera.nearPlane, m_camera.farPlane);
level->Update(deltaSeconds, m_player.GetFeetPosition());
level->Submit(renderer, m_player.GetFeetPosition());
```

**Light.** `SceneLightingFor` turns the level's lighting block (or an
environment preset over it: `Content/Environments/`) plus the view keys'
toggles into the renderer's lighting.

## 4. What you get for free

- **F1** is the engine's: it appears in every app with no code.
- **The workspace's tools, with no code:** your levels are data, so Atom
  Workspace previews, inspects and lights them without running your app
  (section 7). An app carries no tool code (v0.0.19).
- **Scenarios**: `BasicTestHooks` answers the scenario harness for an app
  that is a level, a player and a camera. Tell it where each level puts
  the camera - `m_hooks->ExpectSpawn(spawn)` in `onLoaded` (create the
  hooks before the first `Load`) - and every scenario checks that the
  first frame of a level is drawn from there; an app that never says
  fails that check. Set `ATOM_TEST_SCRIPT` to a script and the app runs
  it and exits with its result:

  ```powershell
  $env:ATOM_TEST_SCRIPT = "Samples\HelloAtom\Scenarios\hello_atom.atomtest"
  build\bin\Release\HelloAtom\HelloAtom.exe
  ```

  `ctest -L scenario` runs it with the others (`Scenario.hello.hello_atom`).
  Your app registers its own scripts in its `CMakeLists.txt`:
  `atom_add_scenarios(<exe> OWNER <name> DIR <folder> SMOKE <short one>)`
  (`cmake/AtomTesting.cmake`), and its CPU tests with `atom_add_tests`.
- **A UI kit, menus and settings** (`Framework/UI`): `UiKit` for captions,
  hint bars and panels in one theme, sharp at any window size;
  `MenuScreen` and `SettingsScreen` for a title or pause menu and the
  player's settings, saved by `SettingsStore`. The Showcase, the demo and
  DRIFT all use them.

## 5. Edit a level (v0.0.17)

**In the level file.** Add an entity to `hello.json`, then check it before
running anything:

```json
{ "name": "lamp", "position": [1.5, 0.0, 3.0], "model": "Kit/toro.glb" }
```

```powershell
cmake --build build --config Debug --target AtomLevelCheck HelloAtom
build\bin\Debug\AtomLevelCheck.exe --app hello      # a typo'd model path, a clip that isn't there... named here
build\bin\Debug\HelloAtom\HelloAtom.exe
```

In the workspace (`AtomWorkspace --project
Apps/Samples/HelloAtom/HelloAtom.atomproject.json --level hello`), the
Hierarchy lists the entities. Click `lamp` to see its data in the
Inspector and a box drawn around it in the Scene; F brings the camera to
it. Validate runs the same check as above.

**In Blender.** HelloAtom's second level, `hello_room`, was modelled by
hand. Its source is `Apps/Samples/HelloAtom/Source/hello_room.blend`.

```powershell
pwsh Tools/Blender/open.ps1 Apps/Samples/HelloAtom/Source/hello_room.blend
#   move a wall or the cube's marker (sidebar N > AtomEngine), save, close
pwsh Tools/Blender/clean_scene.ps1 Apps/Samples/HelloAtom/Source/hello_room.blend
pwsh Tools/Blender/export.ps1 -Scene Apps/Samples/HelloAtom/Source/hello_room.blend -Level hello_room
cmake --build build --config Debug --target HelloAtom
$env:ATOM_START_LEVEL = "hello_room"; build\bin\Debug\HelloAtom\HelloAtom.exe
```

Blender is only needed to make content; HelloAtom and any game run
without it. The full guide (who owns which file, the procedural build,
markers, external models) is [Authoring.md](Authoring.md).

## 6. Where to go next

- **Add an entity** to `hello.json`: any piece in `Content/Kit/`. Give it
  `"animation": { "clip": ... }` if the model has clips, or a `"collider"`
  so the player can't walk through it.
- **Change the light**: edit `lighting`, or switch environments (see how
  the Showcase uses `EnvironmentPresets` and `EnvironmentController`).
  The workspace's Preview > Level lighting edits it live and *Copy as
  JSON* gives you the block.
- **Read a bigger app**: `Apps/Showcase/` adds a time-of-day clock, a
  character with a state machine, a live synth, the lens and a benchmark;
  `Apps/Games/Drift/` is a game that uses the engine without the level system.
- **Understand the engine**: `docs/Architecture.md` for the layers and
  decisions; `docs/AtomEngine-Tech-Manual.md` for how each system works.

## 7. Your own project, in Atom Workspace (v0.0.19)

```powershell
cmake --build build --config Debug --target AtomWorkspace
build\bin\Debug\AtomWorkspace\AtomWorkspace.exe
```

**Create it.** File > Create Project (or the Start page's button), a name
- a letter first, then letters, digits or `_` - and the workspace: the
folder holding `Apps/`, the engine's own by default. It makes
`Apps/<Name>/`:

```
<Name>.atomproject.json   what the project is: name, engine version, source and assets folders,
                          its executable, a startup level - all relative, never a machine's path
CMakeLists.txt            builds an executable only once Source/Main.cpp exists
Source/ Authoring/ Assets/{Levels,Models,Textures,Audio}/ Config/ Tests/ Scenarios/
README.md
```

Nothing is overwritten: an existing folder is an error, and the project is
written in `Apps/.<Name>.creating` and renamed into place only when
complete. The same service runs from the command line:
`AtomProjectTool create --workspace <folder> --name <Name>`.

**Make it run.** The project has no code yet: Run is disabled and says
so. Copy HelloAtom's `Main.cpp` into `Source/`, change its asset folder
(`"Assets/HelloAtom/"`) to `"Assets/<Name>/"`, and give it a level in
`Assets/Levels/` (copy `hello.json`; set `"startupLevel"` in the
descriptor to show it first in the workspace). The descriptor already
names the executable after the project. `hello.json` uses shared pieces
from `Content/`: add them to `atom_app_payload` in the project's
`CMakeLists.txt`, as HelloAtom's does (`"${CMAKE_SOURCE_DIR}/Content/Kit"
"Assets/Kit"`, and `First`). Build: the build notices the new
project (the engine's CMake finds every `Apps/*/*.atomproject.json`) and
compiles it; Run starts it, its output in the Console. Validate checks its
levels; a problem's `Source:` line opens the file with a double-click.

**Look at it.** Pick a level in the toolbar: the Scene shows it without
running your code. The right mouse button and WASD fly; the Preview panel
tries environments, skies and render switches - nothing is saved, Copy as
JSON gives the block for your file.
