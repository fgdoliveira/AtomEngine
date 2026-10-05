# Changelog

## Unreleased

### Changed
- **`GPUDevice`: device and presentation out of the renderer.** Creating
  the GPU device (which picks the adapter) and claiming the window (which
  creates the swapchain) were one step reporting one result, and the
  adapter was logged only when both worked. On the development laptop the
  RTX 4060 device *is* created and only its swapchain for the built-in
  panel is refused, yet every message said the high-performance GPU was
  "unavailable". `GPUDevice` now owns the device, the window claim, the
  fallback, and the swapchain's composition, present mode and frames in
  flight; the renderer owns one and stays the frame coordinator. No
  backend interface: SDL is that abstraction (ADR-001).
- **Honest start-up diagnostics:** the adapter is logged as soon as the
  device exists, before the window claim. A failed claim says which
  adapter it failed on, with SDL's error verbatim (on this laptop:
  "Could not create swapchain! ... (0x00000000)", a success code that
  hides the real failure). `--diagnostics` adds `gpu.fallback.adapter`,
  `.stage` (`device` or `presentation`) and `.error`, and SDL's versions
  now print before the renderer starts. New `Scenario.gpu_fallback`;
  unit tests for the present-mode choice and the fallback wording.
- **SDL 3.4.18** (from 3.4.16). It doesn't change the hybrid-laptop
  result: the RTX still can't present to the built-in panel.

### Added
- **`SwapchainMatrix`** (with `ATOM_BUILD_PRESENTATION_PROBE`): every
  swapchain kind (flip and bitblt models, buffer counts, formats) in
  D3D11 and D3D12, on every adapter, with no SDL. On the development
  laptop every RTX 4060 line fails and every Iris Xe line works.

### Investigated
- **Why the RTX can't present to the laptop panel:** not the engine, SDL,
  the swapchain kind, the API, NVIDIA's opt-ins or the Intel driver (all
  ruled out). It's this muxless laptop's hybrid-display stack: a D3D11
  game reaches the RTX through NVIDIA's per-game path, which D3D12
  doesn't have. Recorded as an addendum to ADR-006; the README's
  hybrid-laptop notes say what a D3D12 program can and can't use.

## 0.0.10 — Hardware portability

Predictable on other Windows machines, without giving up what's known to
work here. Until now the low-power adapter was hardcoded and every
setting lived in F-keys and environment variables. Now the GPU choice
(a stability matter) and the graphics quality (a look-versus-speed matter)
are separate, saved, overridable from the command line, and visible.
Defaults are 0.0.9's: low-power, native resolution, 4× MSAA.

### Added
- **A settings model (M59):** `GameSettings` - schema version, GPU
  preference, quality mode, calibration record, pending fallback - read
  from JSON that never fails (bad input gives the defaults), and a pure
  `ResolveSettings`: command line > `ATOM_*` > saved > defaults. Quality
  presets High / Balanced / Low cap render scale, MSAA, shadows, particles
  and the reflection; anything else is Custom.
- **A command line (M60):** `--gpu`, `--quality`, `--calibrate`,
  `--diagnostics <file>`, `--no-settings`, `--reset-settings`, parsed
  before the device exists (`Application::OnConfigure`, `StartupConfig`).
  Scenarios, benchmarks and `--no-settings` never read saved settings.
- **Saved settings and an F10 Settings panel (M61):**
  `%APPDATA%\AtomEngine\AtomGame\settings.json`; the panel shows the
  adapter really in use, the preference ("restart required"), the quality
  and tier, calibration, reset. Scenario `quality_tiers` checks each
  tier's effect; harness `expect_quality`, `expect_reflection`,
  `move_window`.
- **Diagnostics and a doctor (M62):** `--diagnostics <file>` writes SDL
  versions, adapter, backend, preference and why, present modes, MSAA,
  scene format, display, power and the effective settings.
  `Tools/Dev/doctor.ps1` checks the prerequisites and never changes the
  system; `common.ps1` shares the tool lookup with `check.ps1`. Every PERF
  log starts with a `PERF context` line (adapter, power, tier).
- **A high-performance option that falls back (M63):** if the
  high-performance device can't be created, low-power is used at once and
  the reason shown. A swapchain lost mid-run saves a low-power fallback for
  the next launch and exits cleanly with code 3.
  `ATOM_SIMULATE_SWAPCHAIN_LOSS` and `ATOM_WINDOW_POSITION` reproduce it.
- **Calibration, opt-in (M64):** `--calibrate`, or *Calibrate now / next
  launch* in F10: two heavy views, every tier twice (H B L L B H), the
  highest tier whose worst p95 is ≤ 13.3 ms, saved with its adapter and
  resolution and used by `auto`. Refused on battery; no result when the
  display caps the frame rate.

### Changed
- **Three frames in flight** (SDL's default is two; `ATOM_FRAMES_IN_FLIGHT=1..3`
  overrides, reported in diagnostics and the PERF context). In SDL's D3D12
  backend this also sets the swapchain's buffer count, and two is double
  buffering: with vsync, every release until now missed every other
  refresh - **72 fps on the 144 Hz panel**, 13.9 ms per frame against
  6.95 ms with three (paired, 3 rounds). Three also keeps the Iris Xe from
  idling and lowering its clock (3-second medians had swung 3.5↔9.7 ms).
  A deeper queue can add a frame of input delay, but at twice the frame
  rate each frame is half as long. Calibration measures at the player's
  setting.
- `check.ps1` splits `-Scenario a,b` and fails when a name matches no
  test; `ab.ps1` warns on battery.

### Fixed
- **A crash on swapchain loss:** releasing the window or destroying the
  device after a lost swapchain corrupted the heap (0xC0000374) inside SDL.
  The device is now abandoned on that path, and the process exits.
- **The F10 Settings panel** opened under the Environment panel.
- **No GPU can present:** instead of a bare SDL error, the game explains
  that Windows' per-app graphics setting may force a GPU that can't reach
  the screen, and to choose *Let Windows decide*.

### Measured
Plugged in, Iris Xe, `Tools/Perf/ab.ps1` against v0.0.9, 8 rounds,
uncapped:
- **at the defaults**, 0.0.10 is faster: the night street −0.51 ms (range
  −1.11..−0.31), the lakeshore −0.55 ms (−0.67..−0.50);
- **with `ATOM_FRAMES_IN_FLIGHT=2`** (v0.0.9's setting) it's identical:
  −0.006 ms (−0.20..+0.16) and −0.002 ms (−0.13..+0.12).

So the speed-up is the third frame in flight, and the settings work costs
nothing. With vsync (the default present mode) the difference is larger:
72 → 144 fps on the 144 Hz panel. Older baselines measured with two frames
don't compare with 0.0.10's. Calibration chose High every time, at three
frames in flight and, twice, at two. All scenarios pass at the default in Debug and
Release. At Low and Balanced, the only failures are the checks that a
tier turns off on purpose: rain particles at Low, the reflection at both,
and `quality_tiers`' check that the default is High.

### Known
- On the development laptop (Iris Xe + RTX 4060), the RTX cannot present
  to the built-in panel (`DXGI_ERROR_DEVICE_REMOVED`, also in a raw
  D3D12 probe), so `--gpu high-performance` falls back to the Iris there;
  it works on an external monitor wired to the RTX. ADR-006.
- Input-to-screen latency isn't measured: the frames-in-flight choice
  rests on frame times, and on the reasoning that a frame twice as long
  costs more than one more queued frame.

## 0.0.9 — Hardening

An architecture audit of 0.0.8 (2026-10-03, kept private) found no correctness
failure and no case for a rewrite, and named the places growth was
straining: a GPU lifetime rule kept only in comments, CMake dependencies
broader than their use, no CI, an implicit runtime payload, and a game
coordinator holding too much. This release addresses them in small
commits. No new feature; the engine is the same, and sturdier.

### Changed
- **GPU lifetimes are checked (M53):** meshes and textures count
  themselves per device; the renderer reports anything still alive before
  destroying its device, and asserts in Debug. Every scenario now fails if
  a GPU resource outlives the renderer; all shut down clean.
- **Honest CMake (M54):** ImGui and JSON are PRIVATE to the code that uses
  them, the version string to each target that prints it; SDL and GLM stay
  PUBLIC because the engine's headers expose them. `ATOM_BUILD_GAME`
  (default ON) gates the executable, shaders and scenarios; off, the
  libraries and unit tests build with no shader compiler.
- **Continuous integration (M55):** GitHub Actions builds and runs the unit
  and authoring tests on a fresh Windows machine on every push and pull
  request. Scenarios and performance stay local (they need a GPU).
- **An explicit runtime payload (M56):** the folders copied next to the
  game are a list in `Game/CMakeLists.txt`, found by logging every file the
  game opens (`ATOM_ASSET_LOG`) across all scenarios and levels: 86 files
  from 20 folders; `Schemas/` stays out. CMake 3.26 or later.
- **Diagnostics out of `DemoApp` (M57):** `GameDiagnostics` owns the
  frame-time log, the scripted test runner and the fixed step, with tests
  pinning the PERF line byte for byte. Every level load now logs its time
  and where it went (parse, decode, GPU upload, build).
- **Architecture documentation (M58):** `docs/Architecture.md` - the real
  layering (`Application` is the runtime and composition root, `DemoApp`
  the game; there is no `Engine` object), lifecycles, the renderer's
  passes, the GPU lifetime rule, source and product assets, decision
  records, and when each future library or framework would be justified.

- **A validation ladder:** `Tools/Dev/check.ps1 -Level docs|quick|feature|full`
  checks a change with the cheapest step that can catch its mistakes (from
  nothing for docs, to one incremental build and the unit tests, to the
  scenarios a feature touches, to the full Debug + Release matrix before a
  milestone). README "Development workflow"; `CLAUDE.md` for agents.

### Fixed
- **`ab.ps1` could hang forever:** it waited on the game with no
  wall-clock limit, and the script's own timeout counts game time, which
  stops when frames do. Each run now has a limit; a hung game is killed
  and retried once, a second hang stops the comparison keeping the rounds
  measured. It also left its environment variables set in your shell
  (a later manual launch started in the benchmark level and quit); they
  are now restored however it ends.
- **A benchmark that measured nothing:** `bench` alternates AB BA, so after
  an even number of rounds it ended on its first value. After
  `bench weather off on` the water was left off, and the following
  `bench reflection` compared no reflection with no reflection (~0 ms, what
  the audit recorded). Benches now always end on their second value; the
  reflection measures +0.13..+0.16 ms again.

### Measured
No runtime cost, as intended: against v0.0.8 with interleaved builds
(`Tools/Perf/ab.ps1`, 8 rounds, plugged in, Iris Xe), the street +0.027 ms
(range −0.07..+0.67) and the lakeshore +0.024 ms (−0.10..+0.25), both
within what the method resolves. Level loads now print their timing - the
night street, for instance, 173 ms, most of it GPU upload.

## 0.0.8 — Water and weather

Early-2000s fantasy-game looks on modern hardware: a lake that reads
through colour, movement and highlights rather than optics, and weather
as authored state rather than meteorology. A rainy lake isn't the sunny
lake with rain on top - the sky, fog, sun, ambient light and the water
itself shift together. Built in a lab level of its own, outside the
demo's story.

### Added
- **A day sky (M47):** a procedural gradient from horizon to zenith with
  a sun disc that blooms and a halo (`lighting.skyGradient`); the fog
  takes the horizon's colour unless told otherwise. The blendable part of
  a level's light becomes `EnvironmentState`, which `LevelLighting`
  extends. The fog setting gains "level" (the default): the level's own
  density (`lighting.fogDensity`), none for the existing levels.
- **The lakeshore lab and stylized water (M48):** `ATOM_START_LEVEL=lakeshore`
  - a round lake in a meadow, a sandy shelf, reeds, rocks, trees, hills
  and a jetty, lit live (sky occlusion baked only, so presets can move the
  sun). Water (`atom_water` materials) has its own pipeline and shader: a
  tint from shallow to deep (the depth baked into the mesh's UV), two
  drifting noise layers for ripples, Fresnel toward the sky along the
  rippled reflection, the sun's glint, shore foam, alpha fading out at the
  waterline. Its look is data (`lighting.water`).
- **Environment presets (M49):** `Assets/Environments/*.json` - clear day,
  overcast, rain, fog, sunset, night - with the keys of a level's
  lighting, each optional (what a preset leaves out is the level's).
  Levels offer presets and start in one (`"environment"`);
  `EnvironmentController` switches at once or blends, eased and
  deterministic. An F10 Environment panel (switch, blend time, live
  edits, Copy as JSON), hot reload, and the harness's `environment` /
  `expect_environment`. The water gains a glint control.
- **Rain and wind (M50):** rain streaks in a box that follows the player
  (particles stretched along the fall, slanted by the wind), rings on the
  water, upward surfaces darkened and sheened by the wet, a rain sound
  that plays only while it rains. The environment's wind drives the sway,
  the leaves and the rain. Harness `expect_particles`.
- **Measured, and a planar reflection kept (M51):** `set water|rain|weather|reflection`
  to benchmark, `timeout` for long scripts, and
  `Tools/Perf/water_and_weather.atomtest`. The reflection experiment - the
  opaque near scene mirrored at half resolution, flipped left-right so the
  mirror's winding comes back, clipped at the water by an oblique near
  plane - looked worth its cost and stays as a water option
  (`lighting.water.reflection`), on for the lakeshore.

### Fixed
- **Solid bands across every grass card:** the grass texture generator
  painted nearly opaque rows across the card whenever a leaning blade left
  its left edge (a negative slice end in numpy). Street, shrine, windmill
  field, the kit's tuft and the lakeshore's reeds were all affected.

### Measured
Release, uncapped, Iris Xe, 1280×720, plugged in (Balanced power plan),
the laptop's own screen; medians of 240-frame blocks (`ATOM_PERF_LOG`),
p95 in brackets, single sessions.

The lakeshore from the beach, per preset: clear day 3.53 ms (4.47),
overcast 3.53 (4.34), rain 3.90 (4.89), fog 3.53 (4.34), sunset 3.32
(4.48), night 3.58 (4.62).

What its features cost (in-process paired benches, two runs, beach /
jetty's end): water +0.34 / +0.68 ms, rain +0.23 / +0.18 ms, both +0.63 /
+0.91 ms, the reflection +0.21 / +0.14 ms.

The demo levels, against v0.0.7 (`Tools/Perf/ab.ps1`, interleaved
builds, 8 rounds): the first pass found the street 0.26 ms slower. The
cause was the rain's code in the scene shader: skipped by a branch in
every dry frame, it still cost the street 0.23 ms (removing it alone gave
the time back), the same lesson as v0.0.6's weights view. It now lives in
a shader variant (`BasicRain.frag`) the renderer uses only while it rains.
After that: street −0.035 ms (range −0.28..+0.21), windmill field
−0.007 ms (−0.05..+0.01), night street +0.15 ms (−0.28..+0.87, as noisy as
it was in 0.0.7): within what the method resolves (~0.15 ms).

## 0.0.7 — Into the dark

The two dead ends written as darkness - the machiya's corridor and the
windmill field's shed - are joined by a passage you cross with a
flashlight: dynamic lighting as gameplay. One spot light with its own
shadow map over a mostly baked world, dust and a beam in the air, marks
only the light reveals. Alongside: developer tools, and a way of measuring
performance that a laptop's drift can't fool.

### Added
- **Dear ImGui as a developer tool (M41):** F10 shows panels drawn last,
  outside every screenshot and capture: frame (time graph, draws, binds,
  layers, lit draws), render (the harness's `set` switches), lighting (the
  level's light live, Copy as JSON for the level file), spot light, level
  (entities, flags, counters). Harness `set devtools`.
- **Spot lights and material response (M42):** cone (inner/outer), a
  windowed inverse-square falloff ending at the range, Lambert and a
  Blinn-Phong highlight whose exponent comes from glTF roughness (read at
  last) and strength from `atom_specular`; the same maths on the CPU
  (`SpotMath`) for tests.
- **The spot shadow map (M43):** 1024², perspective from the lamp, drawn
  by the sun's depth pipelines culled to the spot's frustum; 3×3 PCF; the
  normal offset grows with distance.
- **The flashlight (M44):** picked up from the machiya's entry step, F
  toggles it in any level, held low and right and following the view a
  moment late. Decals revealed only by the spot (`atom_reveal`),
  interactables found only in the beam (`requiresLight`), entities gone
  for good once a flag is set (`goneWithFlag`); the dark corridor opens
  once you have it.
- **The passage (M45):** a cellar (candle, battery lantern, jars), a
  timber-shored tunnel that forks, a ladder chamber with an old exit lamp;
  baked dark but not black from those lights alone. Chalk at the fork and
  the trapdoor's bolt are found in the beam; the shed becomes the way down
  once unbolted (`lockedPrompt`). Dust motes seen only in the beam, a faked
  visible beam (three planes along the axis, lit by the spot's reach).
- **Light culling and data (M46):** live point lights culled per draw
  (sphere against box, a bitmask), the spot skipped for draws outside its
  frustum; the flashlight's settings in `Assets/Data/flashlight.json`
  (schema, hot reload, Copy as JSON writes exactly that file).
- **Measuring performance (M46):** `ATOM_PERF_LOG` (per-block median, p95,
  mean; CSV), the in-process `bench` harness command (AB BA alternation,
  median paired difference), `Tools/Perf/ab.ps1` for builds, the procedure
  in the README; `ATOM_PRESENT=immediate`.

### Fixed
- **Particles were drawn with the halo pipeline since 0.0.4**, and a halo
  pipeline was created, and leaked, every frame: the particle pipeline
  cache stored every pipeline as the particles'. Leaves, ash and fog banks
  now blend as designed.
- The developer tools crashed when the Lighting panel was collapsed
  (`End()` must follow every `Begin()`); a scenario collapses every panel.
- Kit pieces are appended to the bake layout, never inserted (inserting
  moved and re-baked the pieces after it).

### Measured
Release, uncapped, Iris Xe, 1280×720, plugged in, the laptop's own screen;
medians of 240-frame blocks (`ATOM_PERF_LOG`), p95 in brackets:
- the passage 1.9 ms (4.2) with the flashlight off, 2.3 ms (4.5) on;
- the machiya interior 2.0 ms (3.6), the character lab 2.2 ms (3.4);
- the pachinko hall 3.0 ms (9.1), the night street 3.9 ms (10.4), the
  street 4.2 ms (8.9).

In-process benches on the night street (two runs): particles +0.28 ms,
consistent; the flashlight +0.23 to +0.77 ms (it lights little of the
street, and the view moves); its shadow pass indistinguishable from zero.
`ab.ps1` of a build against itself gives up to ~0.15 ms, the floor for
comparing builds here. 0.0.7 is the baseline for comparing later versions;
0.0.6 has no `ATOM_PERF_LOG`, and its tag stays as released.

## 0.0.6 — The character lab

The engine animates characters. A rigged model is skinned on the GPU, its
clips are blended and driven by a state machine written as data, and a new
level, the character lab, shows it all in a 2000s model-viewer studio: an
orbit camera, clip controls and debug views, then Tab to take the
character for a walk in third person. The direction moves from horror to
the engine itself.

### Added
- **Skeletal skinning (M35):**
  - glTF skins load: joints, inverse bind matrices, `JOINTS_0` and
    `WEIGHTS_0` (weights renormalised); skins over 64 joints are refused.
  - Each joint's palette matrix (joint world × inverse bind) is computed on
    the CPU once per model per frame; skinned variants of the scene, decal
    and shadow pipelines blend up to four of them per vertex (linear blend
    skinning), from a second vertex stream.
  - Poses are explicit (`Model::Submit` with a pose), the base for
    blending. Skinned meshes' boxes cover every clip, so culling holds.
  - Entities get a uniform `scale`.
- **The character lab and its viewer (M36):**
  - `character_lab` (built by `Tools/Blender/atom_lab.py`): a grid floor,
    a cyclorama fading into same-coloured fog, a checkered turntable.
  - A level with a `lab` section opens in the viewer: an orbit camera
    (arrows, mouse, wheel), clips on 1–4, speed, pause and frame step.
  - Debug views: the skeleton drawn over the body (K), the skin weights
    with a colour per joint blended by weight (W), the bind pose (B).
  - Harness: `clip`, `expect_clip`; `set skeleton|weights|bind|pause`.
- **Pose blending and the animation state machine (M37):**
  - Poses blend per joint (translation and scale lerped, rotation slerped
    the short way); any number of weighted clips.
  - `animator` on entities, as data: states (a clip, or two clips blended
    by a parameter), transitions on conditions such as `"speed > 0.2"`
    with crossfade times, one-shots that return by themselves.
  - Walk and run blended in phase: both cycles at the same fraction of a
    stride, the blended stride in between, so the feet agree.
  - Viewer: clip switches crossfade; 5 = walk/run blend on a slider (Z/X);
    6 = the state machine driven by a 14 s demo script.
  - Harness: `set_param`, `expect_state`; `set mode|blend`.
- **Drive mode (M38):**
  - Tab hands the character to the keys: WASD relative to the camera,
    Shift runs, Space jumps; it turns smoothly to face its way.
  - It walks on the player's own body code (`PlayerController::Move`:
    walls, steps, gravity), now with a jump. The animator reads what the
    body does, not the keys; the jump clip plays in place (`inPlace` pins
    the hips) while physics makes the arc.
  - A spring-arm camera: pulled in at once in front of walls (a raycast),
    easing back out.
  - Animation events per clip (`events`): foot-down times, measured from
    the clips, play the footsteps.
  - The lab gets steps, a ramp, crates and a wall (colliders may now be
    rotated); a `character_lab` scenario drives all of it.
- **Documentation captures of the lab (M39):** `Tools/Docs/character_lab.atomtest`
  and `capture_character_lab.ps1`: stills and GIFs of skinning, the
  skeleton, weights, every clip, a crossfade, the blend, the state
  machine, driving and the spring arm, into `out/img/character_lab/`.
- Added new Third Party credits assets on `Assets/ThirdParty/README.md`.

### Fixed
- A per-pixel branch added for the weights view cost about 10 % of the
  frame on Iris Xe in lightmapped scenes (found measuring this release
  against 0.0.5); it is now a blend, and the cost is gone.

### Measured
Release, uncapped (IMMEDIATE present mode), Iris Xe, 1280x720, averaged
over 6000+ frames: the character lab ≈ 2.35 ms in the viewer, ≈ 2.4 ms
with the skeleton, weights and state machine on, ≈ 2.2 ms driving; the
night street ≈ 3.1 ms, first_render ≈ 1.7 ms. Measured side by side with a
0.0.5 build, the other levels are unchanged (the interior 2.07 against
2.06 ms; the pachinko hall 2.85 against 2.99).

## 0.0.5 — The pachinko game

One machine in the night city's pachinko hall is playable: sit down, the
camera moves in and the game fills the window. Balls fly up the launch lane
and fall through the nails of a 2D physics world; the start pocket spins a
seeded lottery; a hit opens the gate for fever rounds; tokens buy balls, and
balls buy a prize. The engine gained input contexts, a mode controller,
2D physics, playfields and rules as data, and counters in the game state.

### Added
- **Input contexts and the machine mode (M29):**
  - Gameplay reads named actions (`launch`, `leave`, `move_forward`…);
    each mode (exploring, dialogue, machine) maps keys to them, so Space
    confirms in a dialogue and fires at the machine. The harness holds and
    presses actions the same way (`hold_action`, `press_action`).
  - One machine in the pachinko hall is playable (`playMachine` action):
    sitting down eases the camera to its screen, then the game fills the
    window at the largest whole-number scale of 320x240, with borders and
    crisp pixels (`UIRenderer::DrawImage`, nearest sampling for pixel art);
    Q leaves the same way back. That machine has its own screen material,
    which the game takes over from the attract loop (`Level::TakeOverScreen`).
  - The mouse wheel is read (for the launch strength, M31).
- **2D physics (M30):** `World2D` for the pachinko field: balls (dynamic
  circles) against nails (static circles) and segments (walls, rails),
  and against each other as equal masses. Fixed 1/480 s substeps with a
  speed cap, so a ball never moves more than a third of its radius per
  substep (no tunnelling); restitution, friction, and a rest threshold so
  balls settle on rails without jitter; a uniform-grid broad phase; impact
  events for sounds; deterministic to the bit. A debug view draws a world
  into a canvas. Unit tests: resting, bounce energy, tunnelling at top
  speed, bit-identical reruns, momentum.
- **The playable machine (M31):**
  - Playfields as data: `Assets/Machines/night_fever.json` (+ schema):
    walls, curved rails, nail rows, the launcher, pockets (start, side,
    attacker, out, foul) and the attacker's gate; validated on load (all
    shapes on the board, no touching nails, a start pocket and an out
    hole). Laid out by `Tools/Machines/night_fever_layout.py`.
  - `PachinkoGame`: hold Space to fire about 1.7 balls a second up the
    launch lane; the knob (Up/Down, mouse wheel) sets their speed, with a
    little seeded jitter. Weak shots fall back and return to the tray;
    pockets pay balls into it; 250 balls to start with for now.
  - Tuned by playing it headless: a pitched roof on the reel frame (a flat
    top held balls), road nails a little wider apart than a ball (closer,
    they cradled balls and fed every one to the start pocket), a clear
    band around them; about 5-12 % of balls reach the start pocket.
  - Drawn in the 320x240 canvas: board, rails, nails, pockets, balls, the
    tray count and the knob; 7-segment digits shared with the attract loop.
  - Rail friction lowered: a ball riding a rail touches it every substep.
- **Rules, lottery and fever (M32):**
  - `PachinkoRules`, a pure state machine: balls into the start pocket
    hold up to 4 spins; each spin's outcome is drawn from a seeded
    generator when it starts (1 in 99 hits); the reels roll and stop left
    to right, hanging on a reach (two matching); a hit starts a fever of 8
    rounds, each opening the attacker gate for 9 balls or 25 seconds.
    Odds and timings are in the machine file (`rules`).
  - Drawn: the reels in the centre window (pulsing on a reach, flashing in
    a fever), held-spin lamps and the round counter in the right panel,
    the attacker lit while open.
  - Sounds, synthesised: ball clicks (the loudest few impacts per tick),
    the start chime, the payout rattle, reel stops, the reach and the
    fever fanfare; the hall's ambience ducks while you play.
  - The hall's screens now run the real game playing itself (`screens`
    with a `machine`), instead of the simple attract loop.
  - Tests: every rule state in order, round time limits, reaches, held
    spins, hit rate within tolerance over 100 000 draws, a whole session
    replayed exactly from its seed, a forced fever that opens the gate.
- **Tokens, balls and the prize counter (M33):**
  - `GameState` counters next to the flags (`tokens`, `balls`): they read
    0 when missing, never go negative, and survive level changes.
  - Actions `addCounter` (optionally once, with a line for next time) and
    `exchange` (spend a counter for a flag, or say what's missing).
  - The hall's attendant gives 50 tokens on your first night; at the
    machine B (or Enter) buys 50 balls for 10 tokens; balls stay in your
    tray between sessions; 300 balls buy the ofuda from the prize shelf.
  - Payouts retuned: outside a fever the machine pays back about what it
    takes (side pockets 7, start 3); fevers (12 per attacker ball) are
    where you win.
  - Harness `expect_counter`, `set_counter`; a `pachinko_session`
    scenario plays the whole loop.

### Measured
Release, uncapped (IMMEDIATE present mode), Iris Xe, 1280x720, averaged
over 4500+ frames: the pachinko hall ≈ 2.8 ms walking around (0.0.4: 2.6,
now its three screens simulate the real game instead of the attract loop),
≈ 3.5 ms seated at the machine and playing (the 2D game fullscreen, the
hall still drawn underneath); the night street ≈ 3.3 ms (0.0.4: 3.5,
within noise). Other levels are unchanged.

### Added before the game (documentation, #7)
- **Documentation captures:** the `first_render` level (a grid plane and
  spinning cubes, the scene the engine first drew) and a script that
  renders screenshots and frame sequences of the first concepts into
  `out/img/first_render/`, plus a GIF maker (`Tools/Docs/`). The renderer
  can save a frame as PNG; the harness gains `screenshot`, `capture`,
  `pan` and `set`, with a fixed time step for evenly spaced frames.

### Fixed
- Blender markers keep their real name when another level has a marker of
  the same name (object names are global in Blender): the night street's
  `start` spawn would otherwise have been dropped. A bad marker now fails
  the asset build instead of disappearing.
- The bake cache ignores line-ending conversions of the baker's source
  (a checkout could make every lightmap re-bake).
- Levels can skip the vertex bake (`BAKE_MODES` "none").

## 0.0.4 — The night city

A night bus from the rural street leads to one street of a neon city built
to imply a whole one: distance layers and impostors behind it, emissive
light and glow, a wet road, live lights, per-cell ambience, and a pachinko
hall whose machines run live screens. The playable pachinko game is next
(v0.0.5).

### Added
- **Representation foundations (M22):**
  - Level **chunks** (`chunks`: name, model, optional collision, layer
    near/mid/far, `castsShadow`): each drawn and culled as a whole with one
    box test; chunks that cast no shadow stay out of the shadow pass. The
    street is split into a base and three chunks along the road.
  - **Cells** (`cells`: ground-plane bounds and neighbours): near chunks of
    cells that are neither the player's nor a neighbour aren't drawn.
  - A uniform **collision grid** (4 m cells) under raycasts, floor search
    and wall sliding; results identical to the full scan (tested on every
    shipped level), about 5x faster on the street.
  - **Draw sorting** by pipeline and material (the street's 238 draws now
    bind 2 pipelines and ~80 materials); a shared **model cache** across
    levels and entities (reloads files changed on disk).
  - **F1:** per-layer chunks, draws, triangles and shadow draws; binds;
    models loaded and shared.
- **Night rendering (M23):**
  - **Emissive masks:** a separate glTF emissive texture decides exactly
    which pixels glow (neon tubes, lamp glass); without one the base colour
    glows as before.
  - **Glow:** a bright pass at 1/4 size (4-tap downsample, soft knee), a
    separable Gaussian blur (two passes) and an additive composite before
    the tonemap, with a slight shimmer. About 0.09 ms on the Iris Xe.
  - **Halos:** additive billboards around lights, from the level file
    (`halos`: position, size, colour, intensity, flicker); fog dims them.
  - **Night sky:** an equirectangular panorama (`sky`), generated by the
    asset build, drawn behind everything on the far plane.
  - **Fog amount per material** (glTF extras `atom_fog`): lights cut
    through fog.
  - Kit: a street lamp and a neon sign; a `night_test` level
    (`ATOM_START_LEVEL=night_test`) and scenario; `goto_level` in the
    harness.
- **Middle and far layers (M24):**
  - **Mid buildings:** low-poly shells with a facade atlas (four styles)
    and lit windows from an emissive mask; no collision, shadows or bake.
  - **Impostors:** a building rendered by Blender (Cycles, CPU, fixed
    seed: byte-identical) from 8 directions into an alpha-dilated atlas
    with a descriptor; the engine draws a camera-facing card showing the
    nearest view, with 7.5 degrees of hysteresis (`impostors` in levels).
  - **Skyline:** three rings of alpha-tested silhouette cards (180, 260,
    380 m) with lit windows and reduced fog, in front of the panorama.
  - All on `night_test`; the F1 layer stats show mid and far chunks
    culled per chunk and never in the shadow pass.
- **The night street, level E (M25):**
  - Four cells (bus stop, main street, alley, pachinko front), each a
    chunk with its own night lightmap baked from lamps, shop windows,
    the pachinko front, neon spill and a faint moon; the layout (alley
    end wall, side-opening plaza, railway overhead) hides undrawn cells.
  - Wet road: sign and window reflection decals, puddles, and a shader
    ripple and sheen on wet materials (`atom_wet` in glTF extras).
  - Live lights: up to 4 runtime point lights (`lights`); a failing amber
    sign whose light, halo and glow stutter together, and a train on the
    elevated line (`mover` on entities) carrying its light, halos and a
    sound that pans as it passes.
  - Cell ambience: `audio.zones` crossfade as you walk; new synthesised
    sounds: traffic, neon buzz, voices, street bells, pachinko leak, train.
  - Interactables tie back to the rural street (the letter's phone
    number, the hokora's ofuda). Reached for now with
    `ATOM_START_LEVEL=night_street`; the bus comes in M26.
  - Harness `expect_zone`; `night_street` scenario; unit tests for movers,
    flicker, crossfade, cells and the new level keys.
- **The bus stop and the ride (M26):**
  - Action sequences: named step lists in level files (`sequences`:
    wait, message, setFlag, show/hide, playSound following an entity,
    playAnimation, moveEntity easing to a stop, changeLevel last),
    started by a `sequence` action. The player is frozen while one runs
    and one can't start twice.
  - A rural bus stop at the east end of the street; waiting there, the
    bus comes out of the fog with headlights (halos and a live light) and
    its engine, the doors fold open with a hiss, and the fade takes you
    to the city stop. The city timetable runs the ride back.
  - Kit: `bus_stop`, and `bus` with a `doors_open` clip; entities can
    start `hidden`. Sounds: `bus_engine`, `door_hiss`.
  - Harness `wait_for_sequence`; the roundtrip scenario now rides street
    -> city -> street; unit tests for the runner and sequence parsing.
- **The pachinko hall, level F, and render-to-texture (M27):**
  - `RenderTexture`: a colour target drawn before the scene each frame
    and usable as any material's texture, sampled with nearest filtering;
    its canvas is the UI's 2D batcher at a fixed 320x240. F1 stats count
    render textures drawn and scene draws sampling one.
  - The attract loop: balls fall through pins into a start pocket that
    spins the reels and rolls a 7-segment score, all rectangles, on a
    fixed-timestep clock (60 Hz, capped catch-up) and a seeded generator:
    the same picture at any frame rate. Levels map it onto materials
    (`screens`); two seeds alternate along the rows.
  - Level F: rows of machines under fluorescent panels, a prize counter,
    a 1024 lightmap baked from the panels with the machines' glow bleeding
    colour. Entered through the city's pachinko doors (fade), left the
    same way.
  - Audio: a room reverb on the master mix (Schroeder combs + allpass),
    set per level (`audio.reverb`); the hall's own loud bed inside, the
    low-passed leak outside.
  - Harness `expect_screens`; the night street scenario now goes into the
    hall and out; unit tests for the fixed step, the attract loop
    (deterministic, in bounds, scores), the reverb and the new keys.
  - The playable game (input, ball physics, fullscreen) is v0.0.5.
- **Asset build: bake cache and GPU baking:**
  - Lightmaps whose inputs are unchanged (mesh and lightmap UVs, the
    meshes around it, materials and their images, lights, settings,
    Blender version, the baker's code) are kept: fingerprints live in
    `build/bake_cache/`. A build that doesn't touch lit levels takes about
    2 minutes instead of 16; `--no-cache` bakes everything.
  - `--gpu` bakes on the NVIDIA GPU (OptiX, else CUDA): all six lightmaps
    in under 3 minutes, for tuning light. Not byte-identical to the CPU,
    so those files are marked and a test refuses them in `Assets/`.

### Measured
Release, uncapped (IMMEDIATE present mode), Iris Xe, 1280×720, at each
level's default spawn, averaged over 4000+ frames:

| Level | Frame | Draws (near / mid / far) | Shadow draws |
|---|---|---|---|
| street | ≈ 3.9 ms (0.0.3: 3.5) | 238 / 0 / 0 | 114 |
| shrine grounds | ≈ 3.4 ms (2.9) | 104 / 0 / 0 | 137 |
| machiya interior | ≈ 2.0 ms (1.5) | 11 / 0 / 0 | 0 |
| windmill field | ≈ 2.5 ms (2.2) | 88 / 0 / 0 | 109 |
| night street | ≈ 3.5 ms (new) | 52 / 4 / 4 | 0 |
| pachinko hall | ≈ 2.6 ms (new) | 13 / 0 / 0 | 0 |

Per layer on the night street: without the mid and far layers (mid
shells, skyline cards and seven impostors) the frame is ≈ 2.8 ms, so they
cost ≈ 0.65 ms for 8 draws; they never enter the shadow pass. The older
levels are 0.3–0.5 ms slower than in 0.0.3; the likeliest cause is the
glow pass, which now runs in every level (default strength 0.35), not
measured separately. A 144 Hz frame (6.9 ms) still has room everywhere.

## 0.0.3 — Baked light, alpha materials, animation and authoring

The look moves toward early-2000s baked lighting; foliage, cloth, grime and
moving parts fill the scenes; a fourth level; and content iterates without
restarting.

### Added
- **Baked lighting, vertex colours (M15):** Blender (Cycles) bakes light into
  every vertex (glTF `COLOR_0`): sky visibility and bounce outdoors, ambient
  occlusion indoors. Large faces are split into a grid (1 m, 0.5 m indoors)
  so the bake has vertices to live on. The shader blends the flat
  hemisphere ambient toward the baked light per mesh; the sun stays dynamic.
  Per-level `lighting.bakedLight`, **F3** to compare. The bake is
  deterministic (rebuilds stay byte-identical) and a bake that is all black
  or all white fails the asset build.
- **Lightmaps (M16):** the machiya interior is lit by a baked 512² lightmap
  (second UV set, `TEXCOORD_1`): Cycles bakes direct and bounced light from
  bake-only lights at the shoji and the entrance, written as a
  deterministic sRGB PNG. Levels name it with `lightmap` (texture,
  intensity); a missing lightmap fails the load. It replaces the ambient
  term and follows F3. The build lints lightmap UVs (inside 0..1, no
  overlapping faces); the sampler clamps and stops at two mip levels so
  charts don't bleed.
- **Alpha-tested materials (M17):** glTF `alphaMode` MASK with a cutoff and
  `doubleSided`; pixels below the cutoff are discarded in the scene and in
  the shadow pass, so leaves cast leaf-shaped shadows. Double-sided cards
  flip their normal on the back face and let half the sun through (leaves
  lit from behind). Alpha-to-coverage is used when the scene target has an
  alpha channel (RGBA16F), a plain alpha test otherwise (R11G11B10).
- **Content:** leaf, grass, torn-noren and chain-link textures (alpha
  dilated so filtering never pulls in black); bushes, grass tufts, a
  broadleaf tree and a chain-link fence section; a noren at every machiya
  door. Placed along the street and in the shrine grounds.
- **Bake of cards:** a second vertex-bake pass for alpha-tested cards, so
  vertices on transparent texels still receive light; masked materials
  whose alpha never crosses the cutoff fail the asset build.
- **Decals (M18):** glTF `alphaMode` BLEND materials are decals: drawn after
  all other geometry with alpha blending, no depth writes and a depth bias
  toward the camera; they cast no shadows. Water stains and grime on the
  machiya, the shrine wall and the interior, a faded shop sign, ofuda on
  the shrine gate, crossing diamonds on the road; the road lines became
  decals too. Decals float 2 mm over their surface.
- **Lint:** faces of different pieces closer than 5 mm, parallel and
  overlapping, are now an error unless one is a decal (they z-fight at a
  distance); it moved the stone lantern's paper windows out to 6 mm.
- **Animation (M19):** rigid glTF node animation - clips of translation,
  rotation (slerp) and scale keys, linear, step or cubic spline - sampled
  per frame; only animated nodes are posed, static parts keep their baked
  transforms. Entities gain an `animation` (clip, loop, autoplay, speed, a
  sound fired N times per loop) and a `playAnimation` action; a finished
  one-shot stays finished. Harness: `expect_animating`,
  `wait_for_animation`.
- **Vertex sway (M19):** grass, leaves and noren move in the wind in the
  vertex shader (and the shadow pass), weighted per vertex through the
  baked colour's alpha; the wind follows the level's gusts, still indoors.
- **Level D, the windmill field (M19):** past the west end of the street
  ([E] at the field path): a windmill whose sails turn and creak on every
  quarter, a hanging sign that swings, a shed whose door slides open, long
  grass, shrubs and trees. Included in the `levels_roundtrip` scenario.
- **Authoring (M20):**
  - JSON schemas (`Assets/Schemas/`) for levels and dialogue; every file
    names its schema with `$schema`, so editors complete keys and flag
    typos while you type. The C++ parsers stay the authority.
  - Errors say where: `line 20, column 19: ...` for syntax,
    `street.json:/entities/3/interactable/action/type: ...` for content.
    A value of the wrong type is now an error instead of a silent default.
  - Hot reload: with `ATOM_ASSET_ROOT=<repo>` the game reads the source
    tree and, once a second, reloads the level when its JSON, markers,
    models, collision or lightmap change (and dialogue when a dialogue
    file does). The player stays put; a broken file keeps the old level
    and shows the error on screen. `reload_level` / `expect_near` in the
    harness; `hot_reload` scenario.
  - Blender markers: `spawn:<name>` / `entity:<name>` empties are written
    to `<level>.markers.json`; the level file may leave out positions and
    take them from the markers (the file wins when both give one). The
    windmill field is placed this way.

### Changed
- Voice-leak checks count looping voices only; one-shots (a cicada call,
  a creak) end by themselves and made the checks flaky.
- The asset build exits with an error when its script fails.

### Measured
Release, uncapped, Iris Xe, 1280×720, at each level's default spawn:
street ≈ 3.5 ms (0.0.2: 2.6), shrine grounds ≈ 2.9 ms (1.8), interior
≈ 1.5 ms (1.5), windmill field ≈ 2.2 ms (new). The extra cost outdoors is
the denser, per-placement geometry of the bake, and alpha-tested cards in
both the scene and the shadow pass. Alpha-to-coverage is inactive on this
machine (R11G11B10 scene target). A 144 Hz frame (6.9 ms) still has ample
room.

## 0.0.2 — Interaction, dialogue and levels

The engine now represents several levels containing interactive objects and
NPC dialogue, and moves cleanly between them.

### Added
- **UI and text (M9):** TrueType fonts baked to an atlas (stb_truetype), a
  batched 2D overlay for text and panels drawn after the post pass, a
  fading controls hint and an F1 debug panel.
- **Interaction (M10):** generational handles (`SlotMap`) for entities;
  entities composed from optional capabilities (`Renderable`,
  `Interactable`); actions as data (`std::variant`: message, set flag,
  dialogue, change level) executed in one place; target selection by reach,
  a view cone that widens at close range, and line of sight
  (`CollisionWorld::Raycast`); persistent story flags; "[E]" prompts and
  feedback messages.
- **Dialogue (M11):** conversations in JSON (nlohmann/json) with choices
  gated by flags and flags set by outcomes, validated on load; a dialogue
  state machine separate from its view; typewriter text; the player freezes
  and the camera turns to the speaker. The shrine keeper NPC.
- **Levels (M12):** level files describing models, collision, spawns,
  lighting, ambience, emitters, footstep surfaces, particles, unease
  settings and entities; `Level` owns everything local and cleans up by
  RAII; `LevelManager` loads the next level before releasing the current
  one and fades through black around the swap.
- **Content:** a barred shrine gate and an enterable house on the street;
  the shrine grounds (level B) and a machiya interior (level C), built by
  the Blender pipeline; wood footsteps and an interior room tone.
- **Testing:** doctest unit tests and an in-game scenario harness
  (`ATOM_TEST_SCRIPT`) registered with ctest; the `levels_roundtrip`
  scenario is the v0.0.2 acceptance test.
- **Validation (M14):** checks that catch bugs where they start, with no
  stored images:
  - every level change in a scenario checks that the camera is at the
    spawn on the first frame (also a Debug assert during the fade-in);
  - the Blender build refuses to export geometry that would z-fight
    (overlapping coplanar faces of different materials);
  - level files reject unknown footstep surfaces, and levels refuse
    spawns without a floor or inside a wall;
  - `expect_surface` checks the footstep surface underfoot.
- `ATOM_START_LEVEL` to start in any level; version shown in the log and
  window title.

### Changed
- The street moved from C++ into `Assets/Levels/street.json`; `DemoApp`
  owns only persistent state.
- Ambience is started and stopped by levels; `AudioScape` became a named
  sound library plus the sounds that follow the player.
- Gameplay code builds as a library (`AtomGameLib`) shared by the game and
  the tests.

### Fixed
- After a level change, the fade-in showed the new level from the previous
  level's coordinates, then snapped to the spawn; momentum also carried
  through doors.
- Flickering walls in the machiya interior (coplanar overlapping faces at
  the tokonoma, the corridor and the doma); hidden faces of the town house,
  shrine hall and keeper are no longer exported.

### Measured
Release, uncapped, Iris Xe, 1280×720: street ≈ 2.6 ms, shrine grounds
≈ 1.8 ms, interior ≈ 1.5 ms per frame. Full test run ≈ 10 s.

## 0.0.1 — First technical demo

- SDL3 GPU device on Direct3D 12, swapchain, offline HLSL → DXIL.
- First-person camera and controller; collision with wall sliding and
  step-up; frustum culling.
- Blender-generated kit and an 80 m street (glTF), procedural textures.
- Off-screen HDR rendering, render scale, MSAA, height fog, shadow map,
  tonemapping, grade, grain and vignette.
- Particles, a procedural soundscape, and unease events (the figure in the
  fog, radio static, flickering vending machines).
