# Getting started: your first AtomEngine app

This walks through **`Samples/HelloAtom`**, the smallest complete app on
AtomEngine: a window, a level loaded from data, first-person walking, the
engine's numbers, the developer tools and the scenario harness, in about
150 lines (`Samples/HelloAtom/Main.cpp`). Everything the Showcase and the
games do is built on the same pieces.

## 1. Build and run it

Requirements are the engine's (see the README, *Building*): Windows 10/11,
Visual Studio 2022 or later with the C++ tools, CMake 3.26+, and `dxc`.

```powershell
git clone --recursive https://github.com/fgdoliveira/AtomEngine.git
cd AtomEngine
cmake -B build -S .
cmake --build build --config Release --target HelloAtom
build\bin\Release\HelloAtom.exe
```

WASD and the mouse walk, Shift jogs. **F1** shows the engine's numbers
(frame time, draws, the GPU), **F2-F8** switch what's drawn (render scale,
baked light, MSAA, fog, shadows, the post look), **F10** opens the
developer tools, **Esc** quits.

## 2. The three layers

```
Engine/      Atom::            the window, GPU renderer, audio, input, physics, UI drawing
Framework/   AtomFramework::   levels from JSON, the entity world, the player, environments,
                               the UI kit and menus, settings, diagnostics, the scenario harness
your app     (Samples/HelloAtom, Showcase/, Games/*)   what makes it yours
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
The build copies `Samples/HelloAtom/Assets` to `Assets/HelloAtom/` beside
the executable, and the pieces it uses from `Content/` (the first-render
stage, the kit) to `Assets/`.

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

**The level file.** `Samples/HelloAtom/Assets/Levels/hello.json` is
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
- **F10**: `DevPanels` draws the shared panels (Frame, Render, Lighting,
  Environment, Level) for what you hand it in a `DevContext`; your own
  panels go beside them with `DevPanels::Add`.
- **Scenarios**: `BasicTestHooks` answers the scenario harness for an app
  that is a level, a player and a camera. Set `ATOM_TEST_SCRIPT` to a
  script and the app runs it and exits with its result:

  ```powershell
  $env:ATOM_TEST_SCRIPT = "Samples\HelloAtom\Scenarios\hello_atom.atomtest"
  build\bin\Release\HelloAtom.exe
  ```

  `ctest -L scenario` runs it with the others (`Scenario.hello_atom`).
- **A UI kit, menus and settings** (`Framework/UI`): `UiKit` for captions,
  hint bars and panels in one theme, sharp at any window size;
  `MenuScreen` and `SettingsScreen` for a title or pause menu and the
  player's settings, saved by `SettingsStore`. The Showcase, the demo and
  DRIFT all use them.

## 5. Where to go next

- **Add an entity** to `hello.json`: any piece in `Content/Kit/`. Give it
  `"animation": { "clip": ... }` if the model has clips, or a `"collider"`
  so the player can't walk through it.
- **Change the light**: edit `lighting`, or switch environments (see how
  the Showcase uses `EnvironmentPresets` and `EnvironmentController`).
  F10 > Lighting edits it live and *Copy as JSON* gives you the block.
- **Read a bigger app**: `Showcase/` adds a time-of-day clock, a
  character with a state machine, a live synth, the lens and a benchmark;
  `Games/Drift/` is a game that uses the engine without the level system.
- **Understand the engine**: `docs/Architecture.md` for the layers and
  decisions; `docs/AtomEngine-Tech-Manual.md` for how each system works.
