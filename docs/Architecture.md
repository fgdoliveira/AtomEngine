# AtomEngine — Architecture

How AtomEngine is put together, and the rules that keep it that way. The
[technical manual](AtomEngine-Tech-Manual.md) explains *concepts*; this page
explains *structure*. It is maintained (M58):
when a change alters a diagram or a rule here, the change updates this page.

## 1. Targets and dependency direction

```text
AtomGame (exe) ──→ AtomGameLib ──→ AtomEngine ──→ SDL3, GLM
AtomTests ───────→ AtomGameLib
```

```mermaid
graph TD
    Exe[AtomGame executable] --> Game[AtomGameLib]
    Tests[AtomTests] --> Game
    Game --> Engine[AtomEngine]
    Game -.->|private| JSON[nlohmann/json]
    Game -.->|private| ImGui
    Engine --> SDL[SDL3]
    Engine --> GLM
    Engine -.->|private| ImGuiE[Dear ImGui]
    Engine -.->|private| CGLTF[cgltf + stb]
    Exe -.->|build order| Shaders[AtomShaders / DXIL]
```

- **The one rule that matters most:** the engine never includes game code.
  `Engine/` (namespace `Atom`) is reusable; `Game/` (namespace `AtomGame`)
  is the concrete demo.
- **Static libraries** let the tests exercise exactly the game code the
  executable runs.
- **Usage requirements are honest** (M54): PUBLIC only for what a target's
  headers expose (SDL types, GLM maths and its settings); PRIVATE for what
  only its `.cpp` files use (ImGui, JSON, cgltf, stb, the version string).
- **Build options:** `ATOM_BUILD_GAME` (the executable and shaders, needs
  `dxc`), `ATOM_BUILD_TESTS`, `ATOM_BUILD_PRESENTATION_PROBE`. CI builds
  with the game off (§10).

## 2. Application, engine and game: the real layering

There is **no runtime `Engine` object**, and none should be added.

```mermaid
graph LR
    Main[main] -->|constructs| Demo[AtomGame::DemoApp]
    Demo -->|inherits| App[Atom::Application]
    App -->|owns| Window & Renderer & Audio & Input & DevTools & Time
    Demo -->|owns| Levels[LevelManager] & Systems[game systems] & Diag[GameDiagnostics]
    Levels -->|owns| Level
```

- **`Atom::Application`** is the engine's runtime: the platform loop, the
  owner of every process-lifetime subsystem (window, renderer, audio,
  input, developer tools, time) and the composition root that creates them
  in order. It is not a service locator: there is no global lookup.
- **`AtomGame::DemoApp`** is the concrete game: it derives from
  `Application`, receives its hooks (`OnInitialize`, `OnUpdate`,
  `OnShutdown`) and coordinates the game's systems in an explicit frame
  order. Measurement and scripted tests live beside it in
  `GameDiagnostics` (M57).
- Inheritance keeps the ownership order visible in one place. An `IGame`
  interface or a dependency-injection layer would add indirection without
  solving a present problem.

## 3. Lifecycles

**Start:** `OnConfigure` (settings resolved, adapter chosen - §8) → SDL
video → window → GPU renderer → audio (optional) → ImGui →
`DemoApp::OnInitialize` (asset root, sound library, persistent GPU assets
and fonts, data libraries, the `LevelManager` and the first level,
diagnostics).

**Frame:**

```mermaid
flowchart TD
    Events[Poll events] --> Tools[DevTools frame]
    Tools --> Diag[Diagnostics: frame time, fixed step, test script]
    Diag --> Context[Input context]
    Context --> Levels[Transitions + level update]
    Levels --> Mode[Mode: exploring / dialogue / machine / lab]
    Mode --> Prep[Camera, lighting, environment, audio]
    Prep --> Submit[Submit models, lights, particles, UI]
    Submit --> Render[Renderer: sort, cull, passes, present]
```

**Shutdown** is the start in reverse: `DemoApp::OnShutdown` releases levels,
game GPU objects and fonts; then the base shuts down ImGui, audio, the
renderer and its device, the window and SDL.

**Levels** own everything level-lifetime (world, collision, models, voices,
screens); persistent progress (flags, counters) lives above them in
`GameState`. A transition loads the next level *before* releasing the old
one, so shared models are reused.

## 4. Renderer

A concrete façade over SDL GPU: device owner, resource factory, frame
queue and pass orchestrator.

```mermaid
flowchart TD
    Q[Frame queue] --> RT[Render textures] --> SS[Sun shadow] --> SP[Spot shadow]
    SP --> Ref[Reflection, optional] --> Scene[HDR scene, MSAA]
    Scene --> Glow[Glow, optional] --> Post[Post / tonemap] --> UI[Game UI]
    UI --> ImGui[Developer overlay] --> Cap[Capture, optional] --> Present
```

- **Immediate submission:** each frame the game pushes references (meshes,
  materials, chunks, lights, particles); `Render()` sorts, culls, draws and
  clears the queue. The renderer never walks the game's world, and keeps no
  second copy of it. Protect this boundary.
- **Pipelines** are created lazily per variant (samples, culling, alpha to
  coverage, skinning, rain - M52).
- **Known pressure:** `Renderer.cpp` holds every pass. The plan is to move
  one pass's state and code into a private helper *when that pass next
  changes* (audit RENDER-001) - not a render graph, not an RHI.

## 5. GPU resource lifetime

`Mesh` and `Texture` free their SDL resources through a raw, non-owning
device pointer; `RenderTexture` unregisters through a raw `Renderer*`.

**Rule: every GPU wrapper must be destroyed before the renderer shuts
down.** It is checked, not just documented (M53): wrappers count themselves
in `GpuResources`, and `Renderer::Shutdown` reports anything still alive
(and any registered render texture) before destroying the device - and
asserts in Debug. Scenarios fail on that report.

Ownership stays unique (`unique_ptr` by the semantic owner; `shared_ptr`
only for models shared through `ModelCache`).

## 6. World

There is no general engine scene. A `Level` owns a `GameWorld`: a slot map
of entities - a name and transform plus optional capabilities (renderable,
interactable, animated, animator) - addressed by generation-checked
handles. Bulk static geometry is model parts and chunks, not entities.
Keep it that way; it is not an ECS and doesn't need to be (ADR-003).

## 7. Assets: source and product

| Kind | Where | Made by |
|---|---|---|
| **Source** | `Tools/Blender/*.py`, hand-written JSON (levels, dialogue, presets, machines, data) | people |
| **Intermediate** | `build/bake_cache/` | the Blender build (not committed) |
| **Product** | `Assets/**/*.glb`, `*.png`, `*.markers.json` (committed), DXIL (`build/…/shaders`) | Blender, headless and deterministic; `dxc` |
| **Runtime payload** | the folder list in `Game/CMakeLists.txt` (all of `Assets/` but `Schemas/`) | copied next to the executable (M56) |
| **CPU / GPU representation** | parsed level, model, material data; meshes and textures | the loaders, synchronously |

Loading is synchronous and fused (`Model::Load` parses, decodes and
uploads); every level load logs its time and where it went (M57). Split CPU
from GPU loading only when a measured hitch or a tool needs it.

## 8. Hardware and settings

Two decisions are kept apart: **which adapter** (a stability choice, made
once before the device exists) and **how much to draw** (a quality tier,
applied live). Mixing them was the trap: "the fast GPU" is not "high
quality", and on a hybrid laptop it can be the unstable one.

```mermaid
flowchart LR
    CLI[command line] --> R[ResolveSettings]
    Env[ATOM_* env] --> R
    Saved[settings.json] -.->|not for tests, benches, --no-settings| R
    Def[defaults] --> R
    R -->|gpu preference| Cfg[Application::OnConfigure: StartupConfig]
    Cfg --> Dev[Renderer: create device + swapchain]
    Dev -->|high-performance failed| Retry[retry low-power, record why]
    R -->|quality mode| Tier[TierFor: Auto uses calibration]
    Tier --> Apply[DemoApp::ApplyQuality: scale, MSAA, shadows, particles, reflection]
```

- **Precedence:** command line > `ATOM_*` > saved file > defaults
  (`low-power`, `High`). All of it is pure (`Game/Settings/`) and
  unit-tested; a bad file yields the defaults.
- **Before the device:** `Application::OnConfigure()` lets the game fill a
  `StartupConfig` before the renderer exists - the only point where the
  adapter can be chosen. Changing it later means a restart; there is no
  in-process device rebuild.
- **Tiers cap, they don't force:** a tier only turns existing switches off
  (the reflection stays a level's choice under High). No renderer feature
  exists for one tier only.
- **Failure:** at creation, high-performance falls back to low-power
  before any resource exists. Mid-run, a lost swapchain calls
  `OnRenderFailure`; the game saves a low-power fallback, and the device is
  *abandoned*, not destroyed - releasing it on that path corrupted SDL's
  heap - and the process exits with code 3.
- **Calibration** is a measured run over fixed views; its decision
  (`DecideCalibration`) is pure. Records carry the adapter and resolution
  and are dropped when those change.
- **Frames in flight: 3.** In SDL's D3D12 backend this also sets the
  swapchain's buffer count. SDL's default 2 is double buffering, and with
  vsync it missed every other refresh: 72 fps on a 144 Hz panel. 3 holds
  the refresh rate and keeps a clock-dropping iGPU busy. A deeper queue
  can add a frame of input delay, but each frame is then half as long.
  `ATOM_FRAMES_IN_FLIGHT` overrides; calibration measures at the player's
  setting.

## 9. Tools

- **Runtime:** Dear ImGui panels (F10) and the F1 overlay - an overlay, not
  an editor framework, and kept out of captures.
- **Scripted:** `.atomtest` scenarios, paired benches, captures - compiled
  in, switched on by the environment.
- **Offline:** Blender scripts, documentation captures, `ab.ps1`.
- **External:** PIX, Intel GPA, RenderDoc for per-pass GPU timing (SDL GPU
  has no GPU timers).

## 10. Verification layers

| Layer | Runs where | Catches |
|---|---|---|
| Unit and authoring tests (`ctest -LE scenario`) | locally and in **CI** on every push | logic, parsing, content errors; a clean clone that doesn't build |
| In-game scenarios (`ctest -L scenario`) | locally (needs a GPU) | gameplay, rendering, transitions, leaks of voices and GPU resources |
| Paired benches, `ab.ps1` | locally, plugged in | performance changes - never a CI gate |

## 11. Architecture decision records

### ADR-001 — SDL3 GPU is the renderer abstraction
**Decision:** use SDL GPU types directly; no internal RHI. **Why:** SDL GPU
already owns backend selection; a layer on top would map one to one.
**Revisit when** a needed API or platform can't be served through SDL GPU.

### ADR-002 — Static engine/game composition
**Decision:** ordinary CMake targets and static links; no plugins, Gems or
dynamic modules. **Revisit when** features must be distributed or loaded
without relinking.

### ADR-003 — Keep the slot-map world
**Decision:** `GameWorld` with optional capabilities; add small indices only
where profiling asks. **Revisit when** entity or query counts make iteration
measurably expensive.

### ADR-004 — Committed GLB/PNG are runtime products
**Decision:** commit deterministic products, so a clone runs without
Blender; the bake cache and GPU-tuning bakes are never shipped truth.
**Revisit when** product size or content merge conflicts become a measured
problem.

### ADR-005 — New libraries need a trigger
**Decision:** no migration by default. Each adoption needs a written unmet
capability, a baseline, a prototype and a rollback path. **Revisit** per the
table below, never as a bundle.

### ADR-006 — Low-power by default; the GPU choice is not a quality setting
**Context:** every release up to 0.0.9 hardcoded the low-power adapter -
on the development hybrid laptop (Iris Xe + RTX 4060) moving the window
between displays wired to different GPUs lost the swapchain. Measured in
0.0.10: there, Direct3D 12 refuses a swapchain on the RTX for the built-in
panel (`DXGI_ERROR_DEVICE_REMOVED`, also in a raw D3D12 probe), while the
RTX presents to an external monitor wired to it. **Decision:** keep
`low-power` the default; offer `high-performance` as a preference that
falls back to low-power at creation and, after a mid-run loss, on the next
launch. Quality is a separate tier, defaulting to `High` (0.0.9's look).
Calibration is opt-in. **Rejected:** scoring GPUs by name, a hardware
database, continuous adaptive quality, changing the power plan, rebuilding
the device in-process, a launcher. **Revisit when** a device rebuild is
needed for something else (then a live adapter switch costs little), or
when the default adapter proves wrong on hardware other than hybrid
laptops.

## 12. When to add what

| Architecture or library | Not yet, because | Reconsider when |
|---|---|---|
| ECS | slot-map capabilities solve identity and composition | entity or query scale is measured as painful after small indices |
| Job system | the main thread meets current needs | profiles show independent workloads that standard facilities can't run |
| Custom RHI | SDL GPU is the abstraction (ADR-001) | a required API or platform is out of SDL GPU's reach |
| Render graph | the passes are few, fixed and readable | configurable topology, or repeated resource-hazard defects |
| Plugin / module system | one statically composed product | features must ship or load independently |
| Asset database | paths, schemas and committed products work | dependency tracking, moves or platform variants become a measured bottleneck |
| Async loading / CPU-GPU model split | level loads take ~0.2 s and have no budget | a measured load misses a stated budget, or a headless tool needs models |
| fastgltf | cgltf loads every shipped feature | parse time, diagnostics or an unsupported glTF feature fails a requirement |
| meshoptimizer / gltfpack | no size, load or vertex bottleneck is proven | representative assets miss a measured budget |
| Jolt Physics | 3D needs are static queries; 2D pachinko physics is its own | dynamic 3D bodies, constraints, ragdolls or vehicles enter scope |
| miniaudio | the SDL mixer covers synthesis, WAV, 3D, reverb, capture | streaming, codecs or complex routing exceed it |
| Scripting | data and C++ iteration suffice | non-programmers need runtime behaviour iteration |
| Reflection | parsers and schemas are explicit | many types need shared serialization or editor inspection |
| Custom allocator | no allocation problem measured | profiling shows contention or fragmentation |

**Rejected now:** an ECS framework, plugins, an RHI, a render graph, a
reflection system, dependency injection or a service locator, an event bus,
a job system or custom allocators, scripting, an asset daemon, an editor
framework around ImGui, splitting every engine folder into a library, and
library migrations without their trigger.
