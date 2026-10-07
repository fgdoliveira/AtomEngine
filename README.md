# AtomEngine

[![CI](https://github.com/fgdoliveira/AtomEngine/actions/workflows/ci.yml/badge.svg)](https://github.com/fgdoliveira/AtomEngine/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.txt)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![CMake 3.26+](https://img.shields.io/badge/CMake-3.26%2B-064F8C?logo=cmake&logoColor=white)](https://cmake.org/)
[![Platform: Windows 10/11](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6)](#building)
[![Graphics: Direct3D 12](https://img.shields.io/badge/graphics-Direct3D%2012%20%28SDL%20GPU%29-555555)](https://wiki.libsdl.org/SDL3/CategoryGPU)
[![Shaders: HLSL to DXIL](https://img.shields.io/badge/shaders-HLSL%20%E2%86%92%20DXIL-555555)](#building)

[![SDL 3.4.18](https://img.shields.io/badge/SDL-3.4.18-1D4F8C)](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18)
[![GLM 1.0.1](https://img.shields.io/badge/GLM-1.0.1-2E7D32)](https://github.com/g-truc/glm/releases/tag/1.0.1)
[![Dear ImGui 1.92.9](https://img.shields.io/badge/Dear%20ImGui-1.92.9-7B3FA0)](https://github.com/ocornut/imgui/releases/tag/v1.92.9)
[![cgltf 1.15](https://img.shields.io/badge/cgltf-1.15-E65100)](https://github.com/jkuhlmann/cgltf/releases/tag/v1.15)
[![nlohmann/json 3.12.0](https://img.shields.io/badge/nlohmann%2Fjson-3.12.0-C62828)](https://github.com/nlohmann/json/releases/tag/v3.12.0)
[![doctest 2.5.3](https://img.shields.io/badge/doctest-2.5.3-00838F)](https://github.com/doctest/doctest/releases/tag/v2.5.3)

A small C++20 game engine built step by step as a learning and portfolio
project, on **SDL3's GPU API (Direct3D 12)**.

Current version: **0.0.13 "Drift"** — a second game. **DRIFT** is a
faithful port of a three.js web game: fly an endless path at dusk through
rings, orbs and rocks, to a soundtrack the engine synthesises live. It
brought toon shading, outlines and a live synth into the engine, a
framework layer the games share, and packages without developer tools.
(0.0.12 was latency: 36 → 24 ms from click to screen; 0.0.11 distribution.)
See [CHANGELOG.md](CHANGELOG.md); how it's built: [docs/Architecture.md](docs/Architecture.md).

## What's in it

**Rendering** — forward renderer on SDL_GPU / D3D12 with HLSL shaders compiled
offline to DXIL; off-screen HDR target with configurable render scale and
MSAA; hemispheric + sun lighting with baked light (vertex colours and
lightmaps, per chunk); a few live point lights (culled per draw); a spot
light (the flashlight) with a Blinn-Phong highlight from each material's
roughness and its own perspective shadow map, culled to its cone;
exponential height fog; a directional shadow map (texel-snapped, PCF); alpha-tested foliage and cloth,
and decals; emissive masks, a quarter-resolution glow and halo billboards;
a night-sky panorama and a procedural day sky; stylized water (Fresnel
toward the sky, a half-resolution planar reflection clipped by an oblique
near plane, glint, foam, rain rings); rain streaks; wet surfaces (rippled sign reflections, and the ground in
the rain); rigid node animation, skeletal skinning
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

**Weather** — environment presets as data (sun, ambient, fog, sky, water,
rain, wind), offered per level and blended deterministically at runtime;
the wind drives sway, leaves and the rain's slant.

**Tools** — Dear ImGui developer panels (F10: frame, render, lighting,
environment and spot light tuning with Copy as JSON for the data files, level state), kept
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

Requirements: Windows 10/11, Visual Studio 2022+ (C++20), CMake ≥ 3.26, and
`dxc` (found automatically in the Windows SDK or the Vulkan SDK).

```sh
git clone --recursive https://github.com/fgdoliveira/AtomEngine.git
cmake -B build -S .
cmake --build build --config Release
build/bin/Release/AtomGame.exe
```

Dependencies are git submodules in `external/`, each pinned to one commit
(`git submodule status` shows them):

| Library | Version | Licence | Used for | Ships in packages |
|---|---|---|---|---|
| [SDL](https://github.com/libsdl-org/SDL) | 3.4.18 | zlib | window, input, audio, the GPU API (Direct3D 12) | yes (`SDL3.dll`) |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | MIT (or Happy Bunny) | vector and matrix maths | yes (header-only) |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.92.9 | MIT | developer panels and the F1 overlay | linked, never started (development builds only) |
| [cgltf](https://github.com/jkuhlmann/cgltf) | 1.15 | MIT | loading glTF / `.glb` models | yes |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | MIT | levels, dialogue, settings | yes |
| [doctest](https://github.com/doctest/doctest) | 2.5.3 | MIT | unit tests | no (tests only) |

Fonts: Shippori Mincho (the demo) and Space Grotesk (DRIFT), both under the
SIL Open Font License 1.1. Every shipped licence travels in a package's
`THIRD_PARTY_NOTICES.txt`.

Build options (CMake `-D`):

| Option | Default | What it builds |
|---|---|---|
| `ATOM_BUILD_GAME` | ON | the `AtomGame` executable and its shaders (needs `dxc`) |
| `ATOM_BUILD_TESTS` | ON | `AtomTests`; with the game, also the in-game scenarios |
| `ATOM_DISTRIBUTION` | OFF | the game as players get it: a windowed program, no console (development builds keep theirs) |
| `ATOM_BUILD_PRESENTATION_PROBE` | OFF | presentation diagnostics: `PresentationProbe` (SDL or raw D3D12, window moves) and `SwapchainMatrix` (every swapchain kind on every adapter) |

`-DATOM_BUILD_GAME=OFF` builds the engine and game libraries and the unit
tests with no shader compiler - the configuration CI uses.

Something missing or failing? `pwsh Tools/Dev/doctor.ps1` checks the
prerequisites (Windows, CMake, Visual Studio's C++ tools, `dxc`, the
submodules) and says what to install. `-Configure` adds a trial configure
in a throwaway folder; `-GamePath build/bin/Release/AtomGame.exe` adds the
game's own report. It only reads: no drivers, power plans or files change.

## Playing

**DRIFT** (`build/bin/<config>/Drift.exe`): click to launch; WASD, the
arrows or the mouse steer; Shift boosts; M mutes; Esc quits. Its
package: `pwsh Tools/Dist/package.ps1 -Game Drift`. The demo:

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
same. Rain (`rain`, 0..1) falls as streaks slanted by the `wind`, rings the
water, darkens and wets the ground, and brings its own sound; the wind
also sways the foliage. A level names the ones it offers and starts in (`"environment"`);
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

## Settings and hardware

Two choices, kept apart on purpose:

- **GPU preference** (`low-power`, the default, or `high-performance`):
  which graphics adapter the game asks for. It's a *stability* choice, read
  once at start-up, so changing it takes a restart.
- **Graphics quality** (`high`, the default, `balanced`, `low`, or `auto`):
  how much the renderer draws. It applies at once.

| Tier | Render scale | MSAA | Shadows, particles | Water reflection |
|---|---|---|---|---|
| High | 1.0 | 4× | on | where the level has one |
| Balanced | 0.75 | 2× | on | off |
| Low | 0.5 | off | off | off |

F2–F8 still change single settings; the tier then shows as *Custom*.
`auto` uses the tier calibration chose, or High if it hasn't run.

**The F10 Settings panel** shows the adapter actually in use, the GPU
preference ("restart required"), the quality and the tier being drawn, the
calibration result, and *Calibrate now*, *Calibrate next launch* and
*Reset*. Choices are saved in `%APPDATA%\AtomEngine\AtomGame\settings.json`;
a damaged or unknown file is ignored, never fatal.

**Log.** Development builds open a console with the game; the
distributed build (`-DATOM_DISTRIBUTION=ON`, which the package command
uses) is a windowed program with no console. Either way each run writes
its output to `%APPDATA%\AtomEngine\AtomGame\logs\AtomGame.log`,
keeping the previous run as `AtomGame.previous.log`. It holds the version,
folder, SDL, the GPUs tried and why, the quality and every level load.
Started from a terminal, it prints there too. Scripted runs (scenarios,
benchmarks) print only, so they never overwrite the player's log. If the
game can't start when double-clicked, a message box gives the reason and
the log's path.

Command line (it overrides the `ATOM_*` variables, which override the
saved file, which overrides the defaults):

| Option | Effect |
|---|---|
| `--gpu low-power\|high-performance` | the adapter to ask for |
| `--quality auto\|low\|balanced\|high` | the quality |
| `--calibrate` | measure the tiers, save the result, quit (exit 0; 2 if no result) |
| `--diagnostics <file>` | write a report (SDL, adapter, present modes, MSAA, display, power, tier, effective settings) and quit |
| `--no-settings` | ignore the saved file and don't write it |
| `--reset-settings` | overwrite the saved file with the defaults |

Scenarios, benchmarks and `ab.ps1` never read the saved file: their
results don't depend on what someone last chose.

**Hybrid laptops** (an Intel or AMD iGPU plus an NVIDIA or AMD dGPU): the
built-in screen is usually wired to the integrated GPU. Some machines then
refuse a high-performance swapchain for that screen, and switching
monitors between the two GPUs can lose the swapchain. AtomEngine handles
both: if the high-performance device can't be created or can't present,
it starts on low-power and says which step failed, on which adapter, with
the driver's own error. The log then reads, for instance:

```text
GPU device created: backend=direct3d12 adapter="NVIDIA GeForce RTX 4060 Laptop GPU" preference=high_performance
GPU presentation failed on "NVIDIA GeForce RTX 4060 Laptop GPU": could not claim the window: Could not create swapchain! ...
GPU device created: backend=direct3d12 adapter="Intel(R) Iris(R) Xe Graphics" preference=low_power
```

`--diagnostics` reports the same as `gpu.fallback.adapter`, `.stage`
(`device` or `presentation`) and `.error`. A laptop with a hardware MUX
("NVIDIA GPU only" in the NVIDIA Control Panel's *Manage Display Mode*, or
the maker's app) can wire the panel to the dGPU itself.

A Direct3D 12 program (AtomEngine is one) reaches the dGPU only through
Windows' hybrid path. NVIDIA's per-program profile, which is how many
older D3D11 games end up on the dGPU on these laptops, doesn't apply to
D3D12, and neither does the `NvOptimusEnablement` export. Some muxless
laptops can't present a D3D12 dGPU image on their built-in panel at all;
the development laptop is one of them (ADR-006). There, use the dGPU on
an external monitor wired to it, or stay on low-power.
`SwapchainMatrix` (built with `-DATOM_BUILD_PRESENTATION_PROBE=ON`)
checks a machine in seconds: every swapchain kind, D3D11 and D3D12, on
every adapter.

If the swapchain is lost while playing, the game
saves a low-power fallback and quits cleanly (exit 3), and the next launch
explains it. In Windows' *Settings → Display → Graphics*, leave AtomGame on
*Let Windows decide*: forcing "High performance" there overrides both
preferences, and on such a machine the game can't start.

**Calibration** (opt-in) plays two heavy views - the night street, the
lakeshore in rain - at each tier twice, and picks the highest tier whose
worst 95th-percentile frame time is ≤ 13.3 ms (75 fps), then sets quality
to `auto`. It takes about a minute. It refuses to run on battery, and
reports no result when the display caps the frame rate. Calibrate plugged
in, on a cool machine, with the window left alone: the result describes
that moment.

Developer switches (environment variables). Development builds only: a
package (`ATOM_DISTRIBUTION=ON`, M82) compiles them out and ignores them,
as it does F1 and F10.

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
| `ATOM_ASSET_LOG=<file>` | append every asset file the game opens (once each): the evidence for the runtime payload |
| `ATOM_GPU=low-power\|high-performance` / `ATOM_QUALITY=<tier>` | as `--gpu` / `--quality`, below the command line |
| `ATOM_FRAMES_IN_FLIGHT=1..3` | frames the CPU may queue ahead of the GPU (default 2 for lower input latency, moved to 3 automatically if 2 can't hold the refresh rate; in SDL also the swapchain's buffers) |
| `ATOM_LATENCY_WAIT=late` | wait for the swapchain inside rendering, after input is read (0.0.11's order; the default waits first) |
| `ATOM_LATENCY_LOG=1` / `ATOM_LATENCY_FLASH=1` | a `LAT block` line per 20 clicks (click → frame → wait → submit → GPU done) / a black frame on each left click |
| `ATOM_WINDOW_POSITION=x,y` | open the window there (e.g. on another monitor) |
| `ATOM_CALIBRATE_SECONDS=<s>` | shorter calibration windows (tests) |
| `ATOM_SIMULATE_SWAPCHAIN_LOSS=<seconds>` | pretend the swapchain is lost after that many seconds of play, to test the fallback |

## Distribution

One command makes the Windows package players download:

```sh
pwsh Tools/Dist/package.ps1          # -NoSmoke on a machine without a GPU; -Game <name> for another game
```

It builds a Release game in its own folder (`build-dist/`, with
`-DATOM_DISTRIBUTION=ON`: windowed, no console), installs it into
`Dist/AtomGame/` with the CMake install rules, verifies it
(`Tools/Dist/verify.ps1`), starts it once from outside the repository,
and zips `Dist/AtomGame-v<version>-win64.zip`. The install rules in
`Game/CMakeLists.txt` are the one definition of what ships:

```text
AtomGame/
├── AtomGame.exe            C++ runtime linked in: no Visual C++ Redistributable needed
├── SDL3.dll                the only DLL
├── shaders/*.dxil          precompiled and signed
├── Assets/                 the shipped asset folders (the character lab stays out)
├── README.txt              for players: controls, settings, logs, what to try
├── LICENSE.txt
└── THIRD_PARTY_NOTICES.txt SDL, GLM, nlohmann/json, cgltf, stb, Dear ImGui, the font
```

The check fails the package if anything is missing (a shipped asset
folder, a shader, a licence), if anything development-only got in
(`.blend`, `.py`, `.pdb`, test scripts, schemas, the lab), if a binary
needs a DLL Windows doesn't have, or if a shader is unsigned. The same
revision gives the same files.

What ships and what doesn't (players keep the log and `--diagnostics`; developer tools are compiled out), and the
whole flow, are in [docs/Architecture.md](docs/Architecture.md) §9. A
release candidate is tested on a clean Windows VM (a local procedure).

**Troubleshooting a player's report.** Ask for:
1. `%APPDATA%\AtomEngine\AtomGame\logs\AtomGame.log` (and
   `AtomGame.previous.log` if they started it again since). It holds
   the version, the folder, SDL, every GPU tried and why it failed, the
   quality, each level load.
2. `AtomGame.exe --diagnostics report.txt`, run in the game's folder: the
   GPU, present modes, display, power and effective settings.

Things they can try: `--gpu low-power` (laptops), `--quality low`,
`--reset-settings`.

## Testing

```sh
ctest --test-dir build -C Release              # everything
ctest --test-dir build -C Release -LE scenario # unit tests only (no GPU)
ctest --test-dir build -C Release -L scenario  # in-game scenarios
```

**CI** (GitHub Actions, `.github/workflows/ci.yml`) runs on every pull
request and every push to master (and on demand): a fresh Windows machine
clones with submodules, configures with `-DATOM_BUILD_GAME=OFF` (no
shader compiler), builds `AtomTests` in Release and runs them. It answers
"does a clean clone build and pass?". A second job runs
`Tools/Dist/package.ps1 -NoSmoke`: it builds the game and its shaders,
then stages, verifies and zips the distribution. It's a check, not a
release channel: the ZIP isn't kept, and packages for players are made
and published by hand. The in-game scenarios and every performance measurement stay local: they
need a real GPU, and hosted machines time things too noisily.

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
  median of the rounds' differences (it always ends with the switch on B):
  `bench particles on/off: median paired delta (B - A) -0.41 ms (8 rounds, range …)`.
- **Did a build get slower?** `pwsh Tools/Perf/ab.ps1 -A <exe> -B <exe> -Level <level>`
  alternates the two executables (ABBA) and reports the median paired
  difference. Both need `ATOM_PERF_LOG` (v0.0.7 onward). Each run has a
  wall-clock limit (`-TimeoutSeconds`, default 60 s + the run + 30 s): a
  hung game - a covered window, a driver stall, a dialog box, where the
  script's own timeout (game time) stops counting - is killed and retried
  once, and the environment variables it sets are restored however it
  ends. ctest scenarios have their own 240 s limit.
- **Trust the tools first:** `ab.ps1` with the same build as A and B must
  give ~0 ms. On the development laptop it gives up to ~0.15 ms, so smaller
  differences between builds are noise.
- **Why is it slow?** Capture a frame in PIX for Windows (Direct3D 12) or
  Intel GPA for per-pass GPU times; SDL_GPU exposes no GPU timers.

Checklist: plugged in, high-performance power plan, the laptop's own screen,
`ATOM_VSYNC=0` (or `ATOM_PRESENT=immediate`), a short idle first. Frames in
flight change frame times uncapped (3 was ~0.5 ms faster than 2 on the
Iris Xe): builds before 0.0.10 and from 0.0.12 default to 2, 0.0.10 and
0.0.11 to 3. To compare *code* across that line, set
`ATOM_FRAMES_IN_FLIGHT` the same on both sides (builds before 0.0.10
ignore it and use 2). The
300-frame warm-up in `ATOM_PERF_LOG` warms pipelines and caches, not the
hardware; the interleaving takes care of that. Timing is never a ctest
gate: `expect_bench_under` is for local use on known hardware.

**Input latency** (`Tools/Perf/latency.ps1`): how long a click takes,
per configuration, by injecting clicks into the running game (hands off
the mouse and keyboard while it runs). Two modes:

| Mode | Measures | Needs | When |
|---|---|---|---|
| `-Mode Engine` (default) | click → frame start → swapchain wait → submit → GPU done, from the engine's own timing (`ATOM_LATENCY_LOG=1`) | nothing: never asks for elevation | every iteration, automation |
| `-Mode PresentMon` | click → frame on screen, end to end, with Intel PresentMon (`-EngineLog` adds the engine's stages) | PresentMon installed, and ETW access | before a milestone's PR; after a change to presentation, display or frame pacing |

PresentMon is installed separately, from Intel or
github.com/GameTechDev/PresentMon. The script finds it with
`-PresentMonPath`, `ATOM_PRESENTMON` or the default install path, and
never downloads anything. Its event tracing needs an administrator or a
member of Windows' built-in **Performance Log Users** group. Without
either, the script explains and asks for elevation for that run only;
`-NoElevate` makes it exit instead, for unattended use. To run PresentMon
without any prompt, you can join the group yourself. This is optional and
changes a Windows security group; the script never does it:

```powershell
# in an administrator PowerShell, once; then sign out and back in
Add-LocalGroupMember -SID S-1-5-32-559 -Member $env:USERNAME
```

`ATOM_LATENCY_FLASH=1` turns the frame black on each left click (a
visible effect of the input). `ATOM_LATENCY_WAIT=late` and
`ATOM_FRAMES_IN_FLIGHT` restore older frame orders for comparison.

## Development workflow

Check a change with the cheapest step that can catch its mistakes, and
climb only when it passes (`Tools/Dev/check.ps1`):

| Change | Check |
|---|---|
| `.md`, CHANGELOG, `.gitignore`, the CI workflow | `-Level docs` - nothing to build |
| C++ or shaders, inner loop | `-Level quick` - incremental build, unit tests (seconds) |
| content JSON (levels, presets, dialogue) | `-Level quick` (the authoring tests), plus that level's scenario |
| a feature | `-Level feature -Scenario <names>` - quick + the scenarios it touches |
| a milestone | `-Level changed` - what the branch touched (below) |
| the release commit, once per version | `-Level full` - Debug and Release, everything |

**`-Level changed`** compares the branch with `origin/master` (`-Base` to
change that), including uncommitted and untracked files, and maps each
changed file to scenarios through `Tools/Dev/changed.psd1`. It prints every
file with what it chose and why, then builds Debug once and runs the unit
tests plus those scenarios. A change to packaging or a `CMakeLists.txt`
also stages and verifies the package. Docs-only branches run nothing.
A file no rule covers is flagged and gets a broad rendering set, never
silently nothing. `-DryRun` prints the plan without running it. The table
is plain data: when a scenario or a folder is added, add its rule.

Use the configuration where the defect shows (`-Config Debug` for asserts
and lifetime checks, Release for anything timed). Rebuild assets only when
Blender scripts or content products change. CI runs the unit tests and the package check on pull
requests and pushes to master; scenarios are always a local job.

**Privacy.** The repository is public, so nothing personal goes in it:
`pwsh Tools/Dev/privacy.ps1` fails on a local user path (`C:\Users\<name>`),
an email address or a key in any tracked file, binaries included. With
`-Range origin/master..HEAD` it also checks that each commit is signed with
a noreply address. Set it once per clone:
`git config user.email <id>+<user>@users.noreply.github.com` (GitHub,
Settings > Emails). CI runs the check on every pull request.

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
Framework/ what every game shares: the run log, settings and command line, calibration, diagnostics
Game/     Character Dialogue Environment Input Interaction Level Pachinko Testing World  + the demo (DemoApp, player, audio, atmosphere)
Games/    other games: Drift/ (its rules and music in DriftLib, its assets, its package)
Shaders/  HLSL, compiled to DXIL at build time
Tools/    Blender content scripts, Perf (benchmark scripts), Docs, Machines
Assets/   generated models, levels, dialogue, environments, data (flashlight), font;
          the runtime payload - the folders copied next to the game - is
          the list in Game/CMakeLists.txt (all but Schemas/)
Tests/    unit tests and in-game scenarios
docs/     the technical manual (concepts) and Architecture.md (structure)
.github/  CI
external/ pinned dependencies
```

## Licence

Code: MIT, see [LICENSE.txt](LICENSE.txt). Third-party libraries keep their
own licences (the table in [Building](#building)). Fonts: a Latin subset of
Shippori Mincho for the demo (`Assets/Fonts/`) and Space Grotesk for DRIFT
(`Games/Drift/Assets/Fonts/`), both SIL Open Font License 1.1.
Third-party assets used as they came (the lab's character) are credited in
[Assets/ThirdParty/README.md](Assets/ThirdParty/README.md).
