# AtomEngine

A small C++20 game engine built step by step as a learning and portfolio
project, on **SDL3's GPU API (Direct3D 12)**. Its demo is a first-person walk
through a foggy rural Japanese street in the spirit of 2000s horror (Silent
Hill 2's daytime town, Siren) — and, a night bus ride away, one street of a
neon city that implies a whole one — *2000s-inspired art direction on a
modern, resolution-independent renderer*.

Current version: **0.0.5** — the pachinko game: sit down at a machine in
the night city's hall and play it - a 2D physics simulation of balls and
nails, a lottery and fever rounds, tokens and a prize exchange. (0.0.4 built
the night city itself.)
See [CHANGELOG.md](CHANGELOG.md).

## What's in it

**Rendering** — forward renderer on SDL_GPU / D3D12 with HLSL shaders compiled
offline to DXIL; off-screen HDR target with configurable render scale and
MSAA; hemispheric + sun lighting with baked light (vertex colours and
lightmaps, per chunk); a few live point lights; exponential height fog; a
directional shadow map (texel-snapped, PCF); alpha-tested foliage and cloth,
and decals; emissive masks, a quarter-resolution glow and halo billboards;
a night-sky panorama; wet surfaces; rigid node animation and wind sway in
the vertex shader; ACES tonemapping, colour grade, film grain and vignette;
instanced particles; chunk and cell culling with near/mid/far layers,
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
flags and counters; branching, flag-gated dialogue from JSON.

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
stb, nlohmann/json 3.12.0, doctest 2.5.3.

## Playing

| Key | Action |
|---|---|
| WASD / Shift / Mouse | move / jog / look |
| E | interact; in dialogue: continue / confirm |
| W S or 1–4 | choose a dialogue option |
| Esc | release the mouse (again: quit) |
| F1 | debug overlay |
| F2 / F3 / F4 | render scale / baked light / MSAA |
| F5 / F6 / F7 | fog preset / shadows / post look |
| F8 / F9 / M | particles / unease events / mute |

At the pachinko machine:

| Key | Action |
|---|---|
| Space (hold) | launch balls |
| Up / Down, mouse wheel | launch strength |
| B or Enter | buy 50 balls for 10 tokens |
| Q or Backspace | stand up |

The demo: talk to the shrine keeper by the torii, find a way through the
shrine gate, try the door of the house at the east end of the street, and
take the field path at the west end to the windmill. At the bus stop past
the house, wait for the night bus to the city: walk its street and alley,
listen for the train, and step into the pachinko hall. The attendant has
tokens for a first night; one machine is free - sit down and play, and 300
balls buy something from the prize shelf.

Developer switches (environment variables):

| Variable | Effect |
|---|---|
| `ATOM_START_LEVEL=<level>[:<spawn>]` | start in another level (`street`, `shrine_grounds`, `machiya_interior`, `windmill_field`, `night_street`, `pachinko_hall`, `night_test`) |
| `ATOM_TEST_SCRIPT=<file>` | run a scenario script and exit with 0 (pass) / 1 (fail) |
| `ATOM_VSYNC=0` | uncapped frame rate for profiling |
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

## Layout

```text
Engine/   Assets Audio Core Physics Platform Renderer Scene UI
Game/     Dialogue Interaction Level Testing World  + the demo (DemoApp, player, audio, atmosphere)
Shaders/  HLSL, compiled to DXIL at build time
Tools/    Blender content scripts
Assets/   generated models, levels, dialogue, font
Tests/    unit tests and in-game scenarios
external/ pinned dependencies
```

## Licence

Code: see [LICENSE.txt](LICENSE.txt). The bundled font is a Latin subset of
Shippori Mincho (SIL Open Font License 1.1, see `Assets/Fonts/`).
