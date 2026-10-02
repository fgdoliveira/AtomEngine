# AtomEngine

A small C++20 game engine built step by step as a learning and portfolio
project, on **SDL3's GPU API (Direct3D 12)**. Its demo is a first-person walk
through a foggy rural Japanese street in the spirit of 2000s horror (Silent
Hill 2's daytime town, Siren) — and, a night bus ride away, one street of a
neon city that implies a whole one — *2000s-inspired art direction on a
modern, resolution-independent renderer*. A separate character lab, a
2000s model-viewer studio, shows how the engine animates characters.

Current version: **0.0.7** — into the dark: a flashlight found in the
machiya, a spot light with its own shadow map, and a dark passage under the
house - a cellar and a tunnel up to the windmill field's shed - where chalk
marks and a bolt are only found in the beam; plus developer tools (Dear
ImGui, F10) and a way to measure performance that a laptop's drift can't
fool. (0.0.6 built the character lab.)
See [CHANGELOG.md](CHANGELOG.md).

## What's in it

**Rendering** — forward renderer on SDL_GPU / D3D12 with HLSL shaders compiled
offline to DXIL; off-screen HDR target with configurable render scale and
MSAA; hemispheric + sun lighting with baked light (vertex colours and
lightmaps, per chunk); a few live point lights (culled per draw); a spot
light (the flashlight) with a Blinn-Phong highlight from each material's
roughness and its own perspective shadow map, culled to its cone;
exponential height fog; a directional shadow map (texel-snapped, PCF); alpha-tested foliage and cloth,
and decals; emissive masks, a quarter-resolution glow and halo billboards;
a night-sky panorama; wet surfaces; rigid node animation, skeletal skinning
(linear blend, up to 64 joints, in the vertex shader) and wind sway; ACES tonemapping, colour grade, film grain and vignette;
instanced particles, including dust only the beam shows, and a faked
visible beam; decals revealed only by the spot's light; chunk and cell culling with near/mid/far layers,
impostors and skyline cards; draw sorting; render-to-texture; a 2D
text/UI overlay.

**World** — glTF models, collision proxies, light bakes and lightmaps
generated procedurally by Blender scripts (deterministic, with a geometry
lint); levels described in JSON with schemas (models, spawns, lighting,
ambience, footstep surfaces, entities, animations, chunks and cells, lights,
sequences, screens), placed with Blender markers; a level manager that
loads, unloads (RAII), fades between levels and hot-reloads them; a shared
model cache and a grid-accelerated collision world.

**Gameplay** — a first-person controller with wall sliding and step-up;
input contexts (named actions, keys mapped per mode); entities built from
capabilities (drawn, interactable, animated, moving) instead of a class
hierarchy; data-driven actions and timed action sequences; persistent story
flags and counters; branching, flag-gated dialogue from JSON; a flashlight
(found, toggled, held a little behind the view) and interactables found
only in its beam; things that stay gone once taken.

**Tools** — Dear ImGui developer panels (F10: frame, render, lighting and
spot light tuning with Copy as JSON for the data files, level state), kept
out of every screenshot; frame-time logging and paired A/B benchmarks
(see Measuring performance).

**Animation** — clips sampled into explicit poses and blended per joint
(crossfades, a walk/run blend kept in phase); an animation state machine
as data (states, conditions, blend times, one-shots, in-place clips);
animation events (footsteps from foot-down times); a third-person
character on the player's body with a jump and a spring-arm camera;
debug views of the skeleton, the skin weights and the bind pose.

**Pachinko** — a playable machine: a deterministic 2D physics world (balls,
nails, rails; fixed substeps, no tunnelling), playfields as data, a launcher
with a strength knob, a seeded lottery with reaches and fever rounds as a
state machine, presented fullscreen at an integer scale; idle machines play
themselves on their screens.

**Audio** — a software mixer with 3D attenuation and panning and a room
reverb; ambience that crossfades per cell; every sound is synthesised in
code at startup (wind, cicadas, footsteps per surface, vending machine hum,
radio static, room tone, a windmill's creak, city traffic, neon buzz, a
train, a bus, a pachinko hall and its machine: clicks, chimes, reels, the
reach and the fanfare).

**Testing** — doctest unit tests for the pure logic, and an in-game
scenario harness that drives the real game from scripts.

## Building

Requirements: Windows 10/11, Visual Studio 2022+ (C++20), CMake ≥ 3.25, and
`dxc` (found automatically in the Windows SDK or the Vulkan SDK).

```sh
git clone --recursive <repo-url>
cmake -B build -S .
cmake --build build --config Release
build/bin/Release/AtomGame.exe
```

Dependencies are pinned git submodules: SDL 3.4.16, GLM 1.0.1, cgltf 1.15,
stb, nlohmann/json 3.12.0, doctest 2.5.3, Dear ImGui 1.92.9.

## Playing

| Key | Action |
|---|---|
| WASD / Shift / Mouse | move / jog / look |
| E | interact; in dialogue: continue / confirm |
| F | flashlight on / off (once found) |
| W S or 1–4 | choose a dialogue option |
| Esc | release the mouse (again: quit) |
| F1 | debug overlay |
| F2 / F3 / F4 | render scale / baked light / MSAA |
| F5 / F6 / F7 | fog preset / shadows / post look |
| F8 / F9 / M | particles / unease events / mute |
| F10 | developer tools (Dear ImGui) |

At the pachinko machine:

| Key | Action |
|---|---|
| Space (hold) | launch balls |
| Up / Down, mouse wheel | launch strength |
| B or Enter | buy 50 balls for 10 tokens |
| Q or Backspace | stand up |

In the character lab (`ATOM_START_LEVEL=character_lab`), the viewer:

| Key | Action |
|---|---|
| Arrows / mouse (click in), wheel or PgUp/PgDn | orbit / zoom |
| 1–4 | clip (Idle, Jump, Run, Walk), crossfaded |
| 5, then Z / X | walk/run blend, slider |
| 6 | the state machine, driven by a demo script |
| − / + , Space, . | speed, pause, one frame |
| B / K / W | bind pose / skeleton / skin weights |
| Tab | drive: WASD move, Shift run, Space jump, arrows / mouse camera, Tab back |

The lakeshore lab (`ATOM_START_LEVEL=lakeshore`) is a lake in a meadow for
stylized water and weather: a day sky, water tinted shallow to deep that
reflects the sky at grazing angles, glints in the sun and foams at the
shore, and a jetty to walk out on. Its look is authored in the level's
`lighting` (`skyGradient`, `water`), live in the F10 Lighting panel.

Weather and time of day are presets (`Assets/Environments/*.json`: clear
day, overcast, rain, fog, sunset, night). Each one changes the sun, ambient
light, fog, sky **and water** together, over whatever the level leaves the
same. A level names the ones it offers and starts in (`"environment"`);
switch and blend between them in the F10 Environment panel, tune what's
showing, and Copy as JSON for a preset file. Harness: `environment <preset>
[seconds]`, `expect_environment <preset>`.

Keys are bound by position, not by the symbol printed on them: on non-US
layouts − and + are the two keys left of Backspace.

The demo: talk to the shrine keeper by the torii, find a way through the
shrine gate, try the door of the house at the east end of the street, and
take the field path at the west end to the windmill. At the bus stop past
the house, wait for the night bus to the city: walk its street and alley,
listen for the train, and step into the pachinko hall. The attendant has
tokens for a first night; one machine is free - sit down and play, and 300
balls buy something from the prize shelf. Back in the house, a flashlight
lies on the entry step: with it, the dark corridor leads down into a cellar
and a tunnel - follow the chalk only the beam shows, unbolt the trapdoor,
and come up in the windmill field's shed.

Developer switches (environment variables):

| Variable | Effect |
|---|---|
| `ATOM_START_LEVEL=<level>[:<spawn>]` | start in another level (`street`, `shrine_grounds`, `machiya_interior`, `windmill_field`, `night_street`, `pachinko_hall`, `night_test`, `passage`; outside the demo: `character_lab`, `lakeshore`, `first_render`) |
| `ATOM_TEST_SCRIPT=<file>` | run a scenario script and exit with 0 (pass) / 1 (fail) |
| `ATOM_VSYNC=0` | uncapped frame rate for profiling |
| `ATOM_PRESENT=immediate` | with `ATOM_VSYNC=0`: tearing allowed, never waits (some displays hold the default to their refresh) |
| `ATOM_PERF_LOG=1` | after an engine warm-up, one line per block of frames: `PERF block … samples … median … p95 … mean … label …` |
| `ATOM_PERF_BLOCK=<frames>` / `ATOM_PERF_CSV=<file>` | block size (default 240) / the same rows as CSV |
| `ATOM_AUDIO_CAPTURE=<file.wav>` | record the first minute of audio output |
| `ATOM_ASSET_ROOT=<repo>` | read assets from the source tree and hot-reload the level and dialogue when their files change |

## Testing

```sh
ctest --test-dir build -C Release              # everything
ctest --test-dir build -C Release -LE scenario # unit tests only (no GPU)
ctest --test-dir build -C Release -L scenario  # in-game scenarios
```

Every scenario also checks, on each level change, that the new level is
first drawn from its spawn. The asset build has its own check: it refuses
to export overlapping coplanar faces of different materials (z-fighting).

Scenarios live in `Tests/Scenarios/*.atomtest`, one command per line, and
address entities by name:

```text
teleport_to shrine_keeper
interact shrine_keeper
choose 2
expect_flag keeper_permission
wait_for_level shrine_grounds
expect_voices_max 4
```

## Measuring performance

A laptop's speed isn't constant: heat, power source and boost clocks move
frame times by more than most changes cost (unplugged, this one ran 3×
slower; after 40 minutes of load it drifted 20 % within one run). So
AtomEngine compares A and B **close together in time** and reports the
**paired difference**, never two absolute numbers from separate sessions.

- **What does a feature cost?** In-process A/B, the tightest tool: a
  scenario line `bench <setting> <a> <b> <rounds> <seconds> [settle]`, for
  any `set` switch. It alternates A and B in the order AB BA AB BA…, lets
  each switch settle (rebuilt targets aren't steady state), and reports the
  median of the rounds' differences:
  `bench particles on/off: median paired delta (B - A) -0.41 ms (8 rounds, range …)`.
- **Did a build get slower?** `pwsh Tools/Perf/ab.ps1 -A <exe> -B <exe> -Level <level>`
  alternates the two executables (ABBA) and reports the median paired
  difference. Both need `ATOM_PERF_LOG` (v0.0.7 onward).
- **Trust the tools first:** `ab.ps1` with the same build as A and B must
  give ~0 ms. On the development laptop it gives up to ~0.15 ms, so smaller
  differences between builds are noise.
- **Why is it slow?** Capture a frame in PIX for Windows (Direct3D 12) or
  Intel GPA for per-pass GPU times; SDL_GPU exposes no GPU timers.

Checklist: plugged in, high-performance power plan, the laptop's own screen,
`ATOM_VSYNC=0` (or `ATOM_PRESENT=immediate`), a short idle first. The
300-frame warm-up in `ATOM_PERF_LOG` warms pipelines and caches, not the
hardware; the interleaving takes care of that. Timing is never a ctest
gate: `expect_bench_under` is for local use on known hardware.

## Content pipeline

Pachinko machines are JSON files in `Assets/Machines/` (schema in
`Assets/Schemas/machine.schema.json`): walls, rails, nails, the launcher,
pockets, the gate and the rules. `Tools/Machines/night_fever_layout.py`
lays out the shipped one.

All geometry and textures are generated by Python scripts in
`Tools/Blender/` and exported headlessly; the resulting `.glb` files are
committed, so building the engine never needs Blender. Rebuilds are
byte-identical.

```sh
blender -b --factory-startup -P Tools/Blender/build_assets.py
```

Lightmap bakes are cached in `build/bake_cache/`: a bake whose inputs
haven't changed is skipped (a full build takes about 16 minutes cold, 2
warm). After `--`, `--no-cache` bakes everything and `--gpu` bakes on an
NVIDIA GPU for quick light tuning — those lightmaps aren't byte-identical,
and a test refuses them in `Assets/`, so rebuild on the CPU before
committing.

Levels are JSON files in `Assets/Levels/`, dialogue in `Assets/Dialogue/`;
both declare a schema from `Assets/Schemas/` for editor completion and
checks. Level builders in Blender can place spawns and entities with
`spawn:<name>` / `entity:<name>` empties, exported to
`Assets/Levels/<level>.markers.json`. Run the game with
`ATOM_ASSET_ROOT=<repo>` to see edits to level files, markers, models and
dialogue within a second, without restarting.

## Documentation captures

`first_render` rebuilds the scene the engine first drew, a grid plane
with spinning cubes, for illustrating the first concepts (depth,
transforms, lighting, shadows, fog, MSAA, post). One command renders
before/after screenshots and frame sequences into `out/img/first_render/`
(not committed) and turns the sequences into looping GIFs:

```sh
pwsh Tools/Docs/capture_first_render.ps1        # needs a Release build and Blender 5.2
```

The shots are directed by `Tools/Docs/first_render.atomtest`, a scenario
script: `screenshot <stem>`, `capture <stem> <count> <every>`, `pan …
[stem]` (a panning camera, optionally filmed) and `set <what> <value>`
(msaa, scale, fog, shadows, post, sun, particles, fov, overlay, hud,
world, fixed_dt). Each file is named after the manual section it
illustrates.

The character lab has its own: skinning, the skeleton and weights, every
clip, a crossfade, the walk/run blend, the state machine, driving and the
spring arm, into `out/img/character_lab/`:

```sh
pwsh Tools/Docs/capture_character_lab.ps1
```

## Layout

```text
Engine/   Assets Audio Core Debug Physics Platform Renderer Scene UI
Game/     Character Dialogue Input Interaction Level Pachinko Testing World  + the demo (DemoApp, player, audio, atmosphere)
Shaders/  HLSL, compiled to DXIL at build time
Tools/    Blender content scripts, Perf (benchmark scripts), Docs, Machines
Assets/   generated models, levels, dialogue, data (flashlight), font
Tests/    unit tests and in-game scenarios
external/ pinned dependencies
```

## Licence

Code: see [LICENSE.txt](LICENSE.txt). The bundled font is a Latin subset of
Shippori Mincho (SIL Open Font License 1.1, see `Assets/Fonts/`).
Third-party assets used as they came (the lab's character) are credited in
[Assets/ThirdParty/README.md](Assets/ThirdParty/README.md).
