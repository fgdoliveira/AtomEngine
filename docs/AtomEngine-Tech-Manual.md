# AtomEngine — Technical Manual

A study guide to every concept the engine uses, as of **v0.0.13 / M83**. v0.0.1 (M2–M8) covers rendering, lighting and fog, shadows, tonemapping and grading, particles, audio and the scripted unease moments. v0.0.2 (M9–M14) adds text and UI, entities and interaction, dialogue, data-driven levels, automated testing and validation (§28–§33). v0.0.3 (M15–M20) adds baked lighting (vertex colours and lightmaps), alpha-tested materials, decals, rigid animation and vertex sway, a fourth level, and authoring tools: schemas, precise errors, hot reload and Blender markers (§34–§39). v0.0.4 (M22–M28) adds the night city: chunks, cells and distance layers, a collision grid, draw sorting and a model cache; emissive masks, glow, halos and a night sky; facade shells, impostors and skyline cards; per-chunk lightmaps, live lights and a wet road; action sequences; render-to-texture screens, a fixed timestep and a room reverb; and a cached asset build (§40–§46). v0.0.5 (M29–M34) makes a pachinko machine playable: documentation captures, input contexts and a mode switch into a 2D game, 2D physics, playfields and rules as data, a seeded lottery, and counters for an economy (§47–§52). v0.0.6 (M35–M40) animates characters: skeletal skinning, a model-viewer lab with debug views, pose blending and an animation state machine, and a third-person character with a spring-arm camera; and a regression found by measuring against the last release (§53–§57). v0.0.7 (M41–M46) goes into the dark: developer tools with Dear ImGui, spot lights with a specular highlight, a spot shadow map, a flashlight that reveals what only its beam shows, a dark passage between two levels, light culling, and a way of measuring performance that a laptop's drift can't fool (§58–§63). v0.0.8 (M47–M52) adds water and weather: a procedural day sky, stylized water in a lakeshore lab, environment presets blended at runtime, rain and wind, a planar reflection, and a regression caught by measuring against the last release (§64–§69). v0.0.9 (M53–M58) hardens the engine after an architecture audit: GPU lifetimes checked at shutdown, honest CMake dependencies, continuous integration, an explicit runtime asset payload, diagnostics moved out of the game coordinator, load timings, an architecture document, and tooling for hung benchmarks and cheap validation (§70–§75). v0.0.10 (M59–M65) makes the engine portable across machines: a settings model that keeps the GPU choice apart from the graphics quality, a command line read before the device exists, saved settings and a Settings panel, a diagnostics report and a doctor script, a high-performance GPU option that falls back safely on hybrid laptops, opt-in calibration, and the trade-off between frames in flight, GPU clocks and input latency (§76–§82). v0.0.11 (M66–M71) takes the game out of the repository: the GPU device separated from window presentation, so a hybrid laptop's failure is reported at the step where it happens, then a statically linked C++ runtime, a windowed program that keeps a log, one package command built on CMake's install rules, a CI check that a clean clone can make the package, a policy for what ships, and a clean-machine test (§83–§89). v0.0.12 (M72–M75) measures input latency: click-to-display with PresentMon, where the time goes inside the engine, waiting for the swapchain before reading input, 2 frames in flight with a safeguard, and measurement that needs no administrator rights (§90–§94). v0.0.13 (M76–M83) adds a second game, DRIFT, a faithful port of a three.js web game: a framework layer the games share, rules ported line by line as pure seeded code, toon shading and inverted-hull outlines, a live synth on the audio thread with a lock-free command queue, validation chosen by what changed, packages without developer tools, one F1 overlay and one diagnostics report for every game, and two old bugs a second game exposed (double precision in a Debug shader, blurry text from a mipmapped font atlas) (§95–§103).
Each section follows the same shape: **the concept → how AtomEngine does it → where to look in the code**.

> This file lives in `docs/`. It is only updated on request.

---

## Contents

1. [The big picture](#1-the-big-picture)
2. [The frame loop](#2-the-frame-loop)
3. [Time and input](#3-time-and-input)
4. [How the GPU is driven: SDL_GPU on Direct3D 12](#4-how-the-gpu-is-driven-sdl_gpu-on-direct3d-12)
5. [Shaders: HLSL → DXIL](#5-shaders-hlsl--dxil)
6. [Meshes, vertex layouts and uploading data](#6-meshes-vertex-layouts-and-uploading-data)
7. [Graphics pipelines and fixed-function state](#7-graphics-pipelines-and-fixed-function-state)
8. [Coordinate systems and transform matrices](#8-coordinate-systems-and-transform-matrices)
9. [The depth buffer](#9-the-depth-buffer)
10. [The camera and the player controller](#10-the-camera-and-the-player-controller)
11. [Textures, mipmaps and samplers](#11-textures-mipmaps-and-samplers)
12. [Colour spaces: sRGB vs linear](#12-colour-spaces-srgb-vs-linear)
13. [Materials and lighting](#13-materials-and-lighting)
14. [Assets: glTF and the Blender pipeline](#14-assets-gltf-and-the-blender-pipeline)
15. [Collision](#15-collision)
16. [Frustum culling](#16-frustum-culling)
17. [Off-screen rendering and the post pass](#17-off-screen-rendering-and-the-post-pass)
18. [HDR render target formats](#18-hdr-render-target-formats)
19. [Anti-aliasing and MSAA](#19-anti-aliasing-and-msaa)
20. [Render scale](#20-render-scale)
21. [Presentation, vsync and measuring performance](#21-presentation-vsync-and-measuring-performance)
22. [Fog](#22-fog)
23. [Shadow mapping](#23-shadow-mapping)
24. [Tonemapping, grading, grain and vignette](#24-tonemapping-grading-grain-and-vignette)
25. [Particles](#25-particles)
26. [Audio](#26-audio)
27. [Scripted atmosphere: the unease moments](#27-scripted-atmosphere-the-unease-moments)
28. [Text and UI overlay (M9)](#28-text-and-ui-overlay-m9)
29. [Entities and interaction (M10)](#29-entities-and-interaction-m10)
30. [Dialogue (M11)](#30-dialogue-m11)
31. [Levels and level transitions (M12)](#31-levels-and-level-transitions-m12)
32. [Testing: unit tests and the in-game harness (M12–M13)](#32-testing-unit-tests-and-the-in-game-harness-m12m13)
33. [Validation: catching bugs where they start (M14)](#33-validation-catching-bugs-where-they-start-m14)
34. [Baked lighting I: vertex colours (M15)](#34-baked-lighting-i-vertex-colours-m15)
35. [Baked lighting II: lightmaps (M16)](#35-baked-lighting-ii-lightmaps-m16)
36. [Alpha-tested materials (M17)](#36-alpha-tested-materials-m17)
37. [Decals (M18)](#37-decals-m18)
38. [Animation and vertex sway (M19)](#38-animation-and-vertex-sway-m19)
39. [Authoring iteration (M20)](#39-authoring-iteration-m20)
40. [Representation and performance foundations (M22)](#40-representation-and-performance-foundations-m22)
41. [Night rendering (M23)](#41-night-rendering-m23)
42. [Middle and far layers: shells, impostors and skyline cards (M24)](#42-middle-and-far-layers-shells-impostors-and-skyline-cards-m24)
43. [The night street: cells, night lightmaps, live lights, wet road (M25)](#43-the-night-street-cells-night-lightmaps-live-lights-wet-road-m25)
44. [Action sequences: the night bus (M26)](#44-action-sequences-the-night-bus-m26)
45. [Render-to-texture and the pachinko hall (M27)](#45-render-to-texture-and-the-pachinko-hall-m27)
46. [The asset build at scale: bake cache and GPU baking](#46-the-asset-build-at-scale-bake-cache-and-gpu-baking)
47. [Documentation captures: the engine photographs itself](#47-documentation-captures-the-engine-photographs-itself)
48. [Input contexts and the machine mode (M29)](#48-input-contexts-and-the-machine-mode-m29)
49. [2D physics (M30)](#49-2d-physics-m30)
50. [Playfields as data, and tuning by simulation (M31)](#50-playfields-as-data-and-tuning-by-simulation-m31)
51. [Rules as a state machine: the lottery and the fever (M32)](#51-rules-as-a-state-machine-the-lottery-and-the-fever-m32)
52. [Counters and the economy (M33)](#52-counters-and-the-economy-m33)
53. [Skeletal skinning (M35)](#53-skeletal-skinning-m35)
54. [The character lab: a viewer and debug views (M36)](#54-the-character-lab-a-viewer-and-debug-views-m36)
55. [Pose blending and the animation state machine (M37)](#55-pose-blending-and-the-animation-state-machine-m37)
56. [A third-person character: drive mode (M38)](#56-a-third-person-character-drive-mode-m38)
57. [Releasing the lab: captures, and a regression found by measuring (M39–M40)](#57-releasing-the-lab-captures-and-a-regression-found-by-measuring-m39m40)
58. [Developer tools: Dear ImGui (M41)](#58-developer-tools-dear-imgui-m41)
59. [Spot lights and specular highlights (M42)](#59-spot-lights-and-specular-highlights-m42)
60. [The spot shadow map (M43)](#60-the-spot-shadow-map-m43)
61. [The flashlight: light as gameplay (M44)](#61-the-flashlight-light-as-gameplay-m44)
62. [The passage: dust, a beam, and darkness that isn't black (M45)](#62-the-passage-dust-a-beam-and-darkness-that-isnt-black-m45)
63. [Light culling and measuring performance honestly (M46)](#63-light-culling-and-measuring-performance-honestly-m46)
64. [A day sky and the environment state (M47)](#64-a-day-sky-and-the-environment-state-m47)
65. [Stylized water (M48)](#65-stylized-water-m48)
66. [Weather as data: presets and blending (M49)](#66-weather-as-data-presets-and-blending-m49)
67. [Rain and wind (M50)](#67-rain-and-wind-m50)
68. [A planar reflection (M51)](#68-a-planar-reflection-m51)
69. [Releasing 0.0.8: a branch that cost when skipped (M52)](#69-releasing-008-a-branch-that-cost-when-skipped-m52)
70. [An architecture audit, and checked GPU lifetimes (M53)](#70-an-architecture-audit-and-checked-gpu-lifetimes-m53)
71. [Honest CMake: PUBLIC, PRIVATE and build options (M54)](#71-honest-cmake-public-private-and-build-options-m54)
72. [Continuous integration (M55)](#72-continuous-integration-m55)
73. [The runtime asset payload, from evidence (M56)](#73-the-runtime-asset-payload-from-evidence-m56)
74. [Diagnostics out of the coordinator, and load timings (M57)](#74-diagnostics-out-of-the-coordinator-and-load-timings-m57)
75. [Writing the architecture down, and tooling that doesn't hang (M58)](#75-writing-the-architecture-down-and-tooling-that-doesnt-hang-m58)
76. [A settings model: two decisions, kept apart (M59)](#76-a-settings-model-two-decisions-kept-apart-m59)
77. [The command line and choosing the GPU before it exists (M60)](#77-the-command-line-and-choosing-the-gpu-before-it-exists-m60)
78. [Saved settings and the F10 Settings panel (M61)](#78-saved-settings-and-the-f10-settings-panel-m61)
79. [Diagnostics and a doctor (M62)](#79-diagnostics-and-a-doctor-m62)
80. [Hybrid laptops and the high-performance fallback (M63)](#80-hybrid-laptops-and-the-high-performance-fallback-m63)
81. [Calibration, frames in flight and latency (M64)](#81-calibration-frames-in-flight-and-latency-m64)
82. [Releasing 0.0.10: what the RTX taught (M65)](#82-releasing-0010-what-the-rtx-taught-m65)
83. [Device vs presentation: GPUDevice](#83-device-vs-presentation-gpudevice)
84. [The C++ runtime: from a DLL to linked in (M66)](#84-the-c-runtime-from-a-dll-to-linked-in-m66)
85. [Console or window, and a log (M67)](#85-console-or-window-and-a-log-m67)
86. [One package command, from one definition (M68)](#86-one-package-command-from-one-definition-m68)
87. [CI as a check, not a release channel (M69)](#87-ci-as-a-check-not-a-release-channel-m69)
88. [What ships, and the clean-machine test (M70)](#88-what-ships-and-the-clean-machine-test-m70)
89. [Releasing 0.0.11 (M71)](#89-releasing-0011-m71)
90. [Measuring input latency with PresentMon (M72)](#90-measuring-input-latency-with-presentmon-m72)
91. [Where a click's time goes, inside the engine (M73)](#91-where-a-clicks-time-goes-inside-the-engine-m73)
92. [Wait first, then read input (M74)](#92-wait-first-then-read-input-m74)
93. [Measuring without administrator rights](#93-measuring-without-administrator-rights)
94. [Releasing 0.0.12 (M75)](#94-releasing-0012-m75)
95. [A framework both games share (M76)](#95-a-framework-both-games-share-m76)
96. [Porting a web game: DRIFT flies (M77)](#96-porting-a-web-game-drift-flies-m77)
97. [Rules as pure, seeded code (M78)](#97-rules-as-pure-seeded-code-m78)
98. [Toon shading and inverted-hull outlines (M79)](#98-toon-shading-and-inverted-hull-outlines-m79)
99. [A live synth on the audio thread (M80)](#99-a-live-synth-on-the-audio-thread-m80)
100. [Validating what changed (M81)](#100-validating-what-changed-m81)
101. [Lean packages, one overlay, one report (M82)](#101-lean-packages-one-overlay-one-report-m82)
102. [Two old bugs a second game exposed](#102-two-old-bugs-a-second-game-exposed)
103. [Releasing 0.0.13 (M83)](#103-releasing-0013-m83)
104. [Anatomy of a frame and what it costs](#104-anatomy-of-a-frame-and-what-it-costs)
105. [Build system and project layout](#105-build-system-and-project-layout)
106. [Glossary](#106-glossary)

---

## 1. The big picture

A game engine is, at its core, a loop that runs dozens to hundreds of times per second:

```
read input → update the world → describe what to draw → let the GPU draw it → show it
```

AtomEngine splits responsibilities like this:

| Layer | Owns | Folder |
|---|---|---|
| Platform | window, input events | `Engine/Platform` |
| Core | the loop, timing, game hooks | `Engine/Core` |
| Renderer | GPU device, pipelines, textures, meshes, passes | `Engine/Renderer` |
| Scene | camera | `Engine/Scene` |
| Assets | glTF → GPU meshes/textures/materials | `Engine/Assets` |
| Physics | collision against static geometry | `Engine/Physics` |
| Audio | mixer, 3D attenuation, WAV loading | `Engine/Audio` |
| UI | font atlas, 2D text/rect overlay | `Engine/UI` |
| Game | the demo: player, sounds, particles, unease moments, hotkeys; plus `World` (entities), `Interaction`, `Dialogue`, `Level`, `Testing` | `Game/` |

Since v0.0.2 the gameplay code is built as a library (`AtomGameLib`) shared by the game and the tests, and each place is a **level file** loaded at runtime (§31).

The **engine never knows about the game**. The game derives from `Atom::Application` and fills in hooks. This is the same split every real engine makes: reusable systems below, game-specific logic above.

---

## 2. The frame loop

**Concept.** Each iteration ("frame") must: process OS events, advance simulation by the elapsed time, and render. If a frame is slow, the next frame's elapsed time is larger, so movement stays correct in *world* units per second.

**AtomEngine.** `Application::Run()`:

```
Initialize()  → SDL, window, renderer, then game OnInitialize()
loop:
    ProcessEvents()          // SDL events → Input
    OnUpdate(time.Tick())    // game: move player, submit draws
    renderer.Render()        // GPU work for everything submitted
Shutdown()    → game OnShutdown(), renderer, window, SDL
```

The game overrides `OnInitialize`, `OnUpdate(dt)` and `OnShutdown` (**template-method pattern**). Order matters at shutdown: game-owned GPU resources (meshes, textures) must be destroyed *before* the GPU device, which is why `OnShutdown` runs first.

**Code.** `Engine/Core/Application.{h,cpp}`, `Game/DemoApp.{h,cpp}`.

---

## 3. Time and input

### Delta time

**Concept.** `dt` = seconds since the previous frame. Multiply speeds by `dt` so a player walks 1.4 m/s whether the game runs at 60 or 600 fps. A huge `dt` (window dragged, breakpoint hit) would teleport things, so it is **clamped**.

**AtomEngine.** `Time::Tick()` reads `SDL_GetPerformanceCounter()` (a high-resolution CPU clock), divides by its frequency, clamps to 0.1 s.

### Input

**Concept.** OS input arrives as *events* (key down, key up, mouse moved). Games usually want *state* ("is W held right now?") and *edges* ("was F2 pressed this frame?").

**AtomEngine.** `Input` keeps:
- `m_keysDown` — held state, set on key-down, cleared on key-up (and on focus loss so keys don't "stick").
- `m_keysPressed` — cleared at the start of every frame, set on non-repeat key-down. `WasKeyPressed` = edge.
- Mouse delta accumulated from motion events while **relative mouse mode** is on (cursor hidden and locked; you receive movement, not position — essential for mouse-look).

**Code.** `Engine/Core/Time.*`, `Engine/Platform/Input.*`.

---

## 4. How the GPU is driven: SDL_GPU on Direct3D 12

**Concept.** Modern graphics APIs (D3D12, Vulkan, Metal) are *explicit*: you record commands into a **command buffer**, then **submit** it; the GPU executes later, asynchronously. SDL_GPU is a thin portable layer over them; AtomEngine forces the D3D12 backend.

Key objects:

| Object | What it is |
|---|---|
| **Device** | Connection to one GPU. Creates everything else. |
| **Swapchain** | A small ring of images owned by the window. You draw into one, *present* it, and the OS shows it. |
| **Command buffer** | A list of recorded GPU work for one frame. |
| **Render pass** | A scope in which you draw into specific *attachments* (colour/depth textures). Begins with a *load op*, ends with a *store op*. |
| **Copy pass** | A scope for uploads/downloads (CPU ↔ GPU memory). |
| **Transfer buffer** | CPU-visible staging memory used by copy passes. |
| **Buffer / Texture** | GPU memory for vertices/indices or images. |
| **Pipeline** | Compiled shaders + all fixed-function state (see §7). |
| **Sampler** | How a shader reads a texture (filtering, wrapping). |

**Load/store ops.** At the start of a render pass each attachment is `CLEAR`ed, `LOAD`ed (keep old contents) or `DONT_CARE`. At the end it is `STORE`d, `DONT_CARE`d (discarded), or `RESOLVE`d (MSAA, §19). Choosing `DONT_CARE` where possible saves memory bandwidth — very important on integrated GPUs.

**Cycling.** `cycle = true` on an attachment lets SDL hand you a fresh internal copy if the previous frame's GPU work still uses it, instead of stalling the CPU.

**A frame in AtomEngine:**

```
cmd = AcquireCommandBuffer
swap = WaitAndAcquireSwapchainTexture(cmd)   // may be null if minimised
Ensure off-screen targets exist at the right size
UploadParticles(cmd)    // copy pass: this frame's billboards → GPU buffer
RenderShadowPass(cmd)   // depth-only, from the sun, into the shadow map
RenderScenePass(cmd)    // opaque world, then particles, into the scene target
RenderPostPass(cmd)     // tonemap + grade + grain + vignette → swapchain image
Submit(cmd)             // GPU runs it; the swapchain image is presented
```

Several passes in one command buffer: SDL inserts the synchronisation between them (e.g. "the shadow map finished being written before the scene pass samples it"), which in raw D3D12 you would write yourself as *resource barriers*.

**Code.** `Engine/Renderer/Renderer.cpp` (`CreateAndClaimGPUDevice`, `Render`, `RenderScenePass`, `RenderPostPass`).

---

## 5. Shaders: HLSL → DXIL

**Concept.** Shaders are small programs the GPU runs per vertex (**vertex shader**) and per pixel/fragment (**pixel/fragment shader**). D3D12 consumes **DXIL** bytecode, compiled from **HLSL** by Microsoft's **dxc** compiler.

**Offline compilation.** AtomEngine compiles shaders at *build* time, not at runtime: `Shaders/CMakeLists.txt` finds `dxc.exe` (Windows SDK or Vulkan SDK) and emits `bin/<Config>/shaders/<Name>.<stage>.dxil`. Benefits: compile errors show up in the build, no compiler shipped with the game, faster start-up. Naming: `Basic.vert.hlsl` → vertex profile `vs_6_0`, `.frag` → `ps_6_0`.

**Binding model (SDL_GPU on D3D12).** SDL maps resources to fixed HLSL registers/spaces:

| Stage | Textures + samplers | Uniform buffers |
|---|---|---|
| Vertex | `t[n], s[n]` in `space0` | `b[n]` in `space1` |
| Fragment | `t[n], s[n]` in `space2` | `b[n]` in `space3` |

Vertex inputs use semantics `TEXCOORD0..N` matching the attribute *location* numbers.

**Uniforms** are small constants (matrices, colours) pushed with `SDL_Push*UniformData(cmd, slot, data, size)`. The C++ struct layout must mirror the HLSL `cbuffer` (e.g. `ObjectUniforms` ↔ `Basic.vert.hlsl`). A pushed slot stays bound for the rest of the command buffer until pushed again, which gives two natural frequencies:
- **per draw** — slot 0: model matrix, material colours,
- **per frame** — slot 1 of the fragment stage: `SceneUniforms` (sun, sky, fog, shadow matrix, camera position), pushed once per pass.

**cbuffer packing.** HLSL packs constants in 16-byte registers; a `float3` followed by a `float` share one register. That's why AtomEngine's uniforms are built from `float4`/`float4x4` only and smuggle scalars into `.w` components (e.g. `u_fogColor.w` = fog density) — the C++ side can then be plain `glm::vec4`s with no padding surprises.

**Shared includes.** `Shaders/Common.hlsli` holds `SceneUniforms` and `ComputeFog`, used by both the scene and particle shaders. dxc resolves `#include` relative to the source file; CMake lists the include as a dependency of every shader so editing it recompiles them all.

**Matrix convention.** GLM stores matrices column-major; HLSL `cbuffer` matrices default to column-major too, so `mul(M, v)` in HLSL matches `M * v` in C++.

**Code.** `Shaders/*.hlsl`, `Shaders/CMakeLists.txt`, `Engine/Renderer/Shader.cpp` (`LoadShader`).

---

## 6. Meshes, vertex layouts and uploading data

**Concept.** A mesh = **vertices** (position, normal, UV…) + **indices** (triplets of vertex numbers forming triangles). Indices let neighbouring triangles share vertices.

**AtomEngine vertex:**

```cpp
struct Vertex { glm::vec3 position; glm::vec3 normal; glm::vec2 uv; }; // 32 bytes
```

The pipeline's *vertex input state* tells the GPU the **stride** (32 bytes) and each attribute's **offset** and **format**.

**Uploading.** GPU buffers are not directly writable by the CPU. The path is:

```
CPU memcpy → transfer buffer (mapped) → copy pass: UploadToGPUBuffer → GPU buffer
```

Meshes also store their **local AABB** (min/max corners) for culling (§16).

**Code.** `Engine/Renderer/Mesh.*`.

---

## 7. Graphics pipelines and fixed-function state

**Concept.** A **pipeline** bakes together everything needed to draw: shaders, vertex layout, primitive type, **rasterizer** state (fill mode, **culling**, winding), **depth** test/write, **MSAA** sample count, and the **formats** of the attachments it will draw into. Changing any of these means a *different* pipeline. Pipelines are expensive to create, cheap to bind.

**Back-face culling & winding.** Triangles whose corners appear **clockwise** on screen are treated as facing away and skipped (`CULLMODE_BACK`, `FRONTFACE_COUNTER_CLOCKWISE`). All geometry is authored counter-clockwise when seen from outside. Halves the pixel work on closed objects.

**AtomEngine pipelines:**
- **Scene** pipelines — one per MSAA level (1×/2×/4×), created lazily (`GetScenePipeline`), because sample count is part of pipeline state. Since v0.0.3 also per face culling (double-sided cards) and alpha-to-coverage (§36), plus **decal** pipelines per MSAA level (§37). The vertex layout grew to position, normal, uv, baked colour (alpha: sway weight) and lightmap uv.
- **Post** pipeline — fullscreen triangle, no vertex buffer, no depth, cull none, targets the swapchain format.
- **Shadow** pipeline — position (and, since v0.0.3, uv and colour: alpha testing and sway, §36, §38) vertex input, no colour target (depth only), cull none, slope depth bias (§23).
- **Particle** pipelines — one per MSAA level; per-instance vertex input, alpha blending, depth test without depth write (§25).

**Code.** `Renderer::GetScenePipeline`, `Renderer::CreatePostPipeline`.

---

## 8. Coordinate systems and transform matrices

**Spaces a vertex travels through:**

```
local (model) space ──model──▶ world ──view──▶ camera/view ──projection──▶ clip ──÷w──▶ NDC ──viewport──▶ pixels
```

- **Model matrix**: where an object sits in the world (translation, rotation, scale).
- **View matrix**: the inverse of the camera's transform — moves the world so the camera is at the origin looking down −Z (`glm::lookAt`).
- **Projection matrix**: perspective (`glm::perspective`) — far things shrink; maps the view frustum to clip space.
- **MVP** = projection × view × model. AtomEngine sends `viewProjection` and `model` separately (the model is needed for world-space normals and later for fog/shadows).

**Conventions in AtomEngine:**
- **Right-handed, Y-up** world. Camera looks down −Z at yaw 0.
- **Depth range 0..1** (`GLM_FORCE_DEPTH_ZERO_TO_ONE`) — D3D/Vulkan convention (OpenGL uses −1..1).
- **glTF is Y-up**; **Blender is Z-up**. The glTF exporter converts: Blender (x, y, z) → glTF (x, z, −y). So Blender's −Y ("front" of kit pieces) becomes glTF +Z.

**Normals** are transformed by the model matrix's 3×3 part. That's only correct for rotation + *uniform* scale; non-uniform scale would need the inverse-transpose (noted in the shader).

**Code.** `Shaders/Basic.vert.hlsl`, `Engine/Scene/Camera.cpp`, `Renderer::RenderScenePass`.

---

## 9. The depth buffer

**Concept.** Triangles are drawn in arbitrary order; the **depth buffer** stores, per pixel, the distance of the nearest surface so far. A new pixel is kept only if it is closer (**depth test**) and then records its depth (**depth write**). Cleared to 1.0 (far) each frame.

**AtomEngine.** `D32_FLOAT` depth, test `LESS_OR_EQUAL`, store op `DONT_CARE` (nothing reads depth after the pass yet — shadows/fog in M7.2+ compute depth differently). Recreated whenever the scene target size or MSAA changes.

**Z-fighting.** Two surfaces at the same depth flicker. The ground sits 2 cm below the road for this reason.

**Code.** `Engine/Renderer/RenderTargets.cpp`.

---

## 10. The camera and the player controller

### Camera
Yaw (turn left/right) and pitch (look up/down); pitch clamped just short of ±90° so `lookAt` never degenerates. Forward vector = `(sin yaw·cos pitch, sin pitch, −cos yaw·cos pitch)`. "Flat" forward/right ignore pitch so looking up doesn't make you walk into the sky.

### Controller
- **Mouse look**: pixels × sensitivity (0.0022 rad/px).
- **Movement**: WASD → a direction in the camera's flat basis; diagonal normalised so it's not faster.
- **Frame-rate-independent smoothing**: velocity approaches the target with `blend = 1 − e^(−k·dt)`. Using `e^(−k·dt)` (instead of a fixed lerp factor) gives the same feel at any frame rate.
- **Head-bob**: a sine wave driven by *distance walked*, not time, so steps match speed and it fades out when stopping.
- **Gravity** and floor handling: see Collision (§15).
- **Teleport** (v0.0.2): placing the player — at a spawn, or from a test script — sets the feet **and the camera** at once and clears velocity and head-bob weight, so the next frame is drawn from the new spot and no momentum carries through a door (§31).

**Code.** `Engine/Scene/Camera.*`, `Game/PlayerController.*`.

---

## 11. Textures, mipmaps and samplers

**Textures** are GPU images. AtomEngine uploads RGBA8 pixels through a transfer buffer (like meshes).

**Mipmaps.** A chain of pre-shrunk copies (256, 128, 64 … 1 px). When a surface is far away, one screen pixel covers many texels; sampling the full-size image then *aliases* (shimmers). The GPU instead picks the mip level whose texel size ≈ pixel size. AtomEngine generates the chain on the GPU (`SDL_GenerateMipmapsForGPUTexture`), which is why textures are created with `COLOR_TARGET` usage too. **Not every texture wants mipmaps:** one always drawn at its own size, like a font atlas, gains nothing from smaller levels and can be blurred by them. Since v0.0.13 `CreateTexture` takes a `mipmaps` flag, and font atlases have a single level (§102).

**Samplers** decide how a texture is read:
- **Filtering**: nearest (blocky) vs linear (smooth) within a level; mipmap mode picks/blends levels (**trilinear** = linear + linear mip blend).
- **Addressing**: `REPEAT` for tiling materials; `CLAMP_TO_EDGE` for the post pass so the screen edges don't wrap.

**UVs in metres.** Kit geometry sets UVs = local position ÷ "metres per tile", so every material tiles at a consistent physical size and neighbouring pieces line up.

**Code.** `Engine/Renderer/Texture.cpp`, `Renderer::CreateDefaultResources`.

---

## 12. Colour spaces: sRGB vs linear

**Concept.** Monitors and image files store colour **sRGB-encoded** (non-linear, spends more precision on darks). Lighting maths must happen in **linear** light (physically additive). Mixing them up makes images too dark and lighting look wrong.

**AtomEngine's chain:**

```
PNG (sRGB) → texture format *_UNORM_SRGB → sampling decodes to linear
→ lighting in linear → scene target (linear HDR)
→ swapchain SDR_LINEAR (an sRGB swapchain) encodes back to sRGB on write → monitor
```

That's why the clear colour is `0.34` in code but looks like ~`0.62` grey: `0.34` linear ≈ `0.62` sRGB.

**Code.** `Texture::Create` (format choice), swapchain setup in `GPUDevice::ConfigurePresentation` (§83).

---

## 13. Materials and lighting

### Materials
**Material** = base-colour texture × base-colour factor, plus an emissive factor (emission = base colour × factor; used by the vending machine front). Missing textures use a 1×1 white texture so the shader never branches.

Materials are **data the game can change while running**: `Model::FindMaterial("atom_vending_front")` returns a pointer into the model's material list, and every draw that uses it follows the change on the next frame (that's how the vending machines flicker, §27). Material names come from Blender through glTF unchanged.

### The lighting model
Each pixel's light is a sum of two terms, both in linear colour:

```
ambient = lerp(groundColor, skyColor, normal.y * 0.5 + 0.5)   // hemisphere
sun     = max(0, dot(normal, toSun)) * sunColor                // Lambert
lit     = baseColor * (ambient * shadowedSky + sun * shadow)
```

- **Hemispheric ambient** approximates an overcast sky: surfaces facing up receive the bright sky, surfaces facing down receive dim light bounced off the ground, walls get a mix. It is the cheapest "global illumination" there is and suits the flat daylight of the setting.
- **Lambert** diffuse is the cosine law: a surface receives light ∝ cos(angle to the light). Overcast sun is weak, so `sunColor` is low.
- **Shadows** (§23) remove the sun term and a share of the ambient, so they stay readable even though the sun is weak.
- **No specular highlights** by design — restrained materials are part of the look.

### How the values reach the shader
`struct SceneLighting` (sun direction/colour, sky/ground colours, fog, shadow settings) is owned by the game and handed over with `Renderer::SetLighting()`. The renderer converts it into the per-frame `SceneUniforms` buffer (§5). The **sky is cleared to the fog colour**, so where there is no geometry you see "fog".

The default sun sits low in the north-east so the north-side houses lay long shadows across the road — with a sun behind the camera, shadows fall behind objects and become invisible (a lesson from M7.3).

**Code.** `Engine/Renderer/Material.h`, `Engine/Renderer/Lighting.h`, `Shaders/Basic.frag.hlsl`, `DemoApp::ApplyLighting`.

---

## 14. Assets: glTF and the Blender pipeline

### glTF
The "JPEG of 3D": JSON scene description + binary buffers (`.glb` packs both in one file). Structure: **nodes** (hierarchy + transforms) → **meshes** → **primitives** (one material each; accessors for POSITION, NORMAL, TEXCOORD_0, indices) → **materials** (PBR metallic-roughness; base colour texture) → **images**.

**AtomEngine loader** (`Model::Load`, using **cgltf** to parse and **stb_image** to decode PNG/JPEG):
- one GPU `Mesh` per primitive; shared primitives are uploaded once,
- one `Texture` per image, one `Material` per glTF material (+ a fallback),
- one `Part` per (node, primitive) with the node's **world transform** baked in.

**Instancing via shared meshes.** The street places the same house many times; Blender "linked duplicates" export as many nodes pointing to one mesh, so the file and GPU memory hold each house once.

### Blender pipeline (`Tools/Blender/`)
- `atom_textures.py` — procedural, **tileable** textures in numpy (value noise, fBm, patterns). Fixed seeds → reproducible.
- `atom_kit.py` — builds each kit piece from boxes/cylinders/quads with explicit UVs and materials; defines **collision proxies** (simple boxes) per piece.
- `atom_street.py` — lays out the street from linked instances, power-line catenaries, ground, bounds.
- `atom_levels.py` (v0.0.2) — the shrine grounds (level B), the machiya interior (level C) and, since v0.0.3, the windmill field (level D), each with its own collision; the shrine keeper and the other kit pieces live in `atom_kit.py`.
- **Lint** (v0.0.2): every mesh is checked for z-fighting before export; a clash of materials stops the build (§33). Since v0.0.3 it also rejects near-coplanar faces closer than 5 mm unless one is a decal (§37), masked textures whose alpha never crosses the cutoff (§36) and overlapping lightmap UVs (§35).
- **Bakes** (v0.0.3): `atom_bake.py` bakes light into vertex colours and writes sway weights into their alpha (§34, §38); `atom_lightmap.py` bakes lightmaps (§35), since v0.0.4 per chunk, in context and cached (§43, §46). Markers become `<level>.markers.json` (§39). Output stays byte-identical.
- **v0.0.4**: `atom_city.py` (mid shells, skyline, impostor rendering, §42), `atom_night.py` (the night street, §43), `atom_pachinko.py` (the pachinko hall, §45).
- `build_assets.py` — entry point. Exports **headless only** (`blender -b --factory-startup -P …`); in a live Blender it only builds a preview scene. Output is **deterministic** (byte-identical rebuilds), and the `.glb` files are committed so building the engine never needs Blender.

---

## 15. Collision

**Why separate collision geometry?** Render meshes are detailed (window frames, lattice, roof overhangs); walking needs only "you can't go through the house". Simple **proxies** (boxes) are faster and give smoother movement. They are exported to `street_col.glb` and loaded as a **triangle soup** in world space.

**The player body** is a vertical stack of three **spheres** (radius 0.3 m) from just above step height to head height.

**Walls — sphere vs triangle:** for each nearby triangle, find the **closest point on the triangle** to the sphere centre (Ericson's algorithm); if closer than the radius, push the sphere out along the separating direction. The push is made **horizontal-only** so walls never shove you up/down. Iterating a few times resolves corners.

**Sliding** falls out naturally: you push into a wall, only the into-wall component is removed. The controller then sets velocity = *actual* displacement ÷ dt so pushing into a wall doesn't build hidden speed.

**Tunnelling.** At 3.2 m/s and a slow frame you could move more than a thin fence's thickness in one step and pass through. Fix: **sub-step** the movement in pieces ≤ 0.1 m.

**Floor & steps.** A **downward ray** from `feet + stepHeight` finds the highest walkable (up-facing) triangle below. If the floor is within 0.35 m above your feet you **step up** onto it (curbs); the camera eases up so it doesn't pop. When grounded, the ray also looks one step *down* so walking off a curb snaps to the road instead of floating.

**Broad-phase.** Each triangle stores its AABB; cheap AABB rejection happens before the exact test. With 840 triangles a brute-force loop is fine; big levels would need a spatial grid/BVH.

**Code.** `Engine/Physics/CollisionWorld.*`, `Game/PlayerController.cpp`.

---

## 16. Frustum culling

**Concept.** The camera sees a truncated pyramid (**frustum**) bounded by 6 planes. Anything completely outside needn't be sent to the GPU.

**AtomEngine:**
1. **Extract planes** from the view-projection matrix (Gribb–Hartmann): each plane is a sum/difference of matrix rows (adapted for 0..1 depth: near = row 2, far = row 3 − row 2).
2. **World AABB** of each draw: transform the mesh's local box centre; the world half-extent is `Σ |column_i of model| · localExtent_i` (Arvo's method) — a quick conservative box.
3. **Test**: if the box is fully behind any plane (`distance < −projected radius`) → skip.

Result on the street: 177/196 parts drawn looking down the street, ~5 facing a wall. The window title shows `draws drawn/submitted`.

**Code.** `ExtractFrustum`, `IsVisible` in `Renderer.cpp`.

---

## 17. Off-screen rendering and the post pass

**Concept.** Instead of drawing straight into the swapchain image, draw the world into your own texture (the **scene target**), then run a **post pass** that reads it and writes the final image. This enables everything "after" the 3D: fog in screen space, tonemapping, colour grading, grain, upscaling, etc.

**Fullscreen triangle.** The post pass draws one oversized triangle (3 vertices generated from `SV_VertexID`, no vertex buffer) that covers the screen; its UVs span 0..1 across the visible area. Cheaper than a quad (no diagonal seam, better GPU cache behaviour).

**AtomEngine:** scene target → post pass → swapchain. Since M7.4 the post pass also tonemaps, grades and adds grain and a vignette (§24). With the post look off (F7) it is a plain copy — useful to see what the scene itself looks like.

**Code.** `Shaders/Fullscreen.vert.hlsl`, `Shaders/Post.frag.hlsl`, `Renderer::RenderPostPass`, `Engine/Renderer/RenderTargets.*`.

---

## 18. HDR render target formats

**Concept.** Lit colours can exceed 1.0 (bright sky, emissive panels). An 8-bit target clips them. **HDR** (floating-point) targets keep them until tonemapping squeezes them into displayable range.

| Format | Bytes/pixel | Notes |
|---|---|---|
| `R8G8B8A8_UNORM` | 4 | LDR; clips at 1.0 |
| `R11G11B10_UFLOAT` | 4 | HDR, no alpha, no negatives — **AtomEngine's choice** |
| `R16G16B16A16_FLOAT` | 8 | HDR with alpha; fallback if the packed format is unsupported |

On an integrated GPU, **memory bandwidth** (bytes moved per frame) is often the real limit, so halving the target size matters more than it looks.

---

## 19. Anti-aliasing and MSAA

**Aliasing.** Each pixel is coloured from one sample point; edges become stair-stepped and thin things (power lines) break up or *crawl* when moving.

**MSAA (multisample anti-aliasing).** The rasterizer tests **coverage** at N sample points per pixel (2, 4…) and stores N colour/depth samples, but runs the pixel shader only **once per triangle per pixel**. Edge pixels end up with a mix of samples from different triangles. At the end the samples are averaged — **resolve**.

- Smooths *geometric* edges (silhouettes, wires, slats). Does **not** fix texture shimmer (that's mipmaps' job) or sub-pixel-thin geometry that no sample hits.
- Cost is mostly **memory**: an N× MSAA target is N times larger.

**AtomEngine:** multisampled colour + depth are attachments of the scene pass; the colour store op is `RESOLVE` into a single-sample texture that the post pass samples. With `RESOLVE`, the multisampled data never needs writing back to memory (a big bandwidth saving on tiled/integrated GPUs). Supported sample counts are queried (`SDL_GPUTextureSupportsSampleCount`) and requests clamped. **F4** cycles 4×/2×/1×.

Measured (Release, Iris Xe): 4× costs ≈ 0.2–0.3 ms at 720p/1080p → default 4×.

---

## 20. Render scale

**Concept.** Render the 3D scene at a fraction of the window size and scale up in the post pass. Fewer pixels to shade → faster, at some sharpness cost. It's a *performance* setting, not the look of the game.

**AtomEngine:** `RenderSettings::renderScale` (100/85/75/50 %, **F2**). Scene target = window × scale (rounded, min 1 px). The projection's aspect ratio uses the scene target size. The post pass upsamples with a **bilinear** clamp sampler. A retro 360-line nearest-neighbour mode is planned as an option (M7.1b).

**When it helps.** Only when the GPU is limited by per-pixel work (shading, fog, shadows sampling, MSAA bandwidth). Today's scene is so light that the difference is within measurement noise; it will matter after M7.2–M7.4.

---

## 21. Presentation, vsync and measuring performance

### Present modes
| Mode | Behaviour |
|---|---|
| **VSYNC** | Present waits for the monitor's refresh; no tearing; fps capped at the refresh rate (144 Hz here). Default. |
| **MAILBOX** | Renders as fast as possible; the newest finished frame is shown at refresh; no tearing. |
| **IMMEDIATE** | Presents immediately; may tear. |

`ATOM_VSYNC=0` (environment variable) selects MAILBOX (or IMMEDIATE) for profiling.

### Why M7.1 "shows more fps" than earlier milestones
It doesn't really. Until M7.1 every run used **VSYNC**, so the title always read ≈ **144 fps** — the monitor's refresh rate, a *cap*, not a measurement. M7.1 added the uncapped mode, and the measurements were taken in a **Release** build (optimised code, no GPU debug layer), while earlier titles came from **Debug** builds. Uncapped, the frame costs ~1.45 ms at 720p (≈ 688 fps). M7.1 actually does *more* work than M6 (an extra fullscreen pass + MSAA); under a 144 fps cap you just can't see it.

### fps vs milliseconds
Always reason in **frame time** (ms): `ms = 1000 / fps`. Costs add in milliseconds, not in fps:
- 800 → 700 fps looks like "−100 fps" but is only +0.18 ms.
- 60 → 50 fps is +3.3 ms — a much bigger real cost.
Budget at 60 Hz = 16.7 ms; at 144 Hz = 6.9 ms.

### Measuring properly
- Release build, vsync off, same camera view, let it settle, several readings.
- Laptops change clocks with power/thermal state → noisy numbers; treat ± 10–20 % differences as noise unless repeatable.
- The title's fps is an average over 0.5 s.

---

## 22. Fog

**Concept.** Real air scatters light: the farther a surface, the more its colour is replaced by the colour of the air. Games model this as a blend:

```
final = lerp(surfaceColour, fogColour, fogAmount)
```

**Exponential fog.** Treat air as a medium with *density* σ (per metre). Light surviving a distance d falls off as `e^(−σ·d)` (Beer–Lambert law), so

```
fogAmount = 1 − e^(−σ·d)
```

It never reaches exactly 1, but after `d ≈ 3/σ` metres it is 95 % fog — a handy rule: *density 0.085 ≈ fully fogged by ~35 m*.

**Height fog.** Real ground fog is thicker low down. Let density fall off with height: `σ(y) = σ₀ · e^(−k·(y − base))`. Along a view ray the total fog is the *integral* of σ over the ray. For this density there's a closed form, so the shader computes

```
opticalDepth = σ₀ · e^(−k·(camY − base)) · d · (1 − e^(−k·Δy)) / (k·Δy)
fogAmount    = 1 − e^(−opticalDepth)
```

where Δy is the ray's height change. Looking down the street (Δy ≈ 0) the last factor → 1 (plain exponential fog); looking up it shrinks, so the sky clears faster than the ground. That's why rooftops and wires stay faintly visible while the road dissolves.

**The sky is fog.** The scene target is cleared to `fogColour`, and fog is applied per pixel in the same colour, so geometry fades into the sky seamlessly — the Silent Hill trick for hiding where the world ends.

**Cost.** A handful of `exp`/`length` per pixel: ~0.04 ms at 720p. Fog is cheap because it's computed while shading pixels that are drawn anyway (no extra pass).

**AtomEngine.** `ComputeFog` in `Shaders/Common.hlsli` (shared with particles, so they fog identically). Presets on **F5**: off (default), dense (σ 0.13), medium (0.085), light (0.045); height falloff k = 0.08 m⁻¹.

---

## 23. Shadow mapping

**Concept.** A point is in shadow if something sits between it and the light. A **shadow map** answers that with the depth buffer: render the scene *from the light* storing only depth; later, for each pixel you shade, transform its world position into the light's view and compare its depth to the stored one. If the stored depth is closer, something blocks the light.

```
Pass 1 (shadow pass):  scene → depth only, light's view-projection → shadow map
Pass 2 (scene pass):   p_light = LightViewProj · worldPos
                       lit = (p_light.z <= shadowMap[p_light.xy]) ? 1 : 0
```

**Directional light → orthographic projection.** The sun is so far away that its rays are parallel, so the light's "camera" is an orthographic box rather than a perspective frustum.

**Fitting the box.** A box covering the whole street would waste resolution. AtomEngine uses a **60 m box centred on the camera**, 2048² texels → ≈ 3 cm per texel. Things further than 30 m are unshadowed — hidden by fog anyway.

**Shimmering and texel snapping.** If the box slides continuously with the camera, each frame rasterises the scene into the shadow map at a slightly different sub-texel offset; shadow edges crawl and sparkle as you walk. Fix: keep the light's *orientation* fixed and move the box only in **whole-texel steps** (round its centre to the texel grid in light space). Static geometry then rasterises identically every frame.

**Shadow acne and peter-panning.** Comparing depths of the *same* surface suffers precision/sampling errors — random self-shadowing speckles called **acne**. Remedies:
- **Slope-scaled depth bias** in the shadow pipeline's rasterizer: surfaces at grazing angles to the light get pushed away more.
- **Normal offset**: look up the shadow map from a point nudged 6 cm along the surface normal.
Too much bias detaches shadows from their casters (**peter-panning** — objects look like they float). The values are a balance.

**Soft edges: PCF.** A single comparison gives hard, jagged edges. **Percentage-closer filtering** takes several comparisons around the point and averages the *results* (0/1), not the depths. AtomEngine uses:
- a **comparison sampler** (`enable_compare`, `LESS_OR_EQUAL`) — the hardware performs 4 comparisons and bilinearly blends them in one fetch,
- a **3×3 grid** of those fetches → soft, overcast-friendly penumbrae.

**Both faces cast.** The kit contains single-sided quads (doors, ground), so the shadow pipeline has culling off.

**Strength.** Shadows remove the sun term and 35 % of the ambient (`shadowAmbientShare`), so they read even in weak overcast light.

**Culling.** The shadow pass reuses the frame's draw queue, culled against the *light's* frustum (`DrawQueue` is shared by both passes). Casters outside the camera view but inside the light box still cast.

**Cost.** ~1 ms at 720p on the Iris Xe (a fixed 2048² depth pass of ~100 draws; independent of window size). **F6** toggles it. Levers if needed: 1024² map, fewer casters, cascaded maps for large views (not implemented).

**Code.** `Renderer::CreateShadowResources`, `ComputeLightViewProjection`, `RenderShadowPass`, `ComputeShadow` in `Basic.frag.hlsl`, `Shaders/Shadow.*.hlsl`.

---

## 24. Tonemapping, grading, grain and vignette

All in the post pass (`Shaders/Post.frag.hlsl`), in this order, at **output** resolution:

1. **Exposure** — a multiplier on the HDR scene colour (like camera exposure).
2. **Tonemapping** — HDR values can exceed 1.0; the display can't. Clipping at 1.0 makes bright areas flat and harsh. A *tonemapping curve* compresses the range smoothly: dark and mid tones barely change, highlights roll off towards 1. AtomEngine uses **Narkowicz's fit of the ACES filmic curve**:
   `f(x) = x(2.51x + 0.03) / (x(2.43x + 0.59) + 0.14)` — it also adds a gentle "film" contrast (it's why shadows looked deeper after M7.4).
3. **Grade** — *desaturation* toward luminance (`lerp(luma, colour, 0.72)`), then a *tint* multiply (0.97, 1.02, 0.95) for a slight grey-green cast. Luminance uses Rec.709 weights (0.2126, 0.7152, 0.0722) because the eye is most sensitive to green.
4. **Film grain** — per-pixel pseudo-random noise from a cheap integer-style hash of (pixel x, pixel y, frame index), so it changes every frame like real film. It's multiplicative (±3.5 %) and weighted toward mid-tones: grain in pure black or white looks like noise, in mid-tones it reads as texture. It's applied at output resolution so it stays fine-grained even at low render scales.
5. **Vignette** — darken toward the corners with a `smoothstep` of the distance from centre, corrected for aspect ratio so it's round.

Order matters: grading after tonemapping works on display-range values (predictable), grain after grading isn't desaturated away.

**Settings.** `RenderSettings::post` (exposure, saturation, tint, grain, vignette). **F7** cycles full → grade only → off. Cost ≈ 0.1 ms.

---

## 25. Particles

**Concept.** Effects made of many small, simple, often transparent sprites (leaves, ash, smoke, fog puffs). Three standard techniques combine here:

**Billboards.** Each particle is a quad that always faces the camera. The vertex shader builds the corners from the camera's **right** and **up** vectors (the first two columns of the inverse view matrix): `corner = centre + (right·x + up·y) · size/2`, rotated in the screen plane for spinning leaves.

**Instancing.** Drawing 200 particles as 200 draw calls would waste CPU. Instead, one draw call renders **6 vertices × N instances**: per-particle data (position, size, colour, rotation, atlas cell) lives in a vertex buffer with **per-instance** input rate, and the six corner vertices come from `SV_VertexID` — no vertex buffer for the quad at all.

**Per-frame upload.** Particles move every frame, so their data is re-uploaded each frame: memcpy into a transfer buffer mapped with **cycle = true** (SDL gives a fresh buffer if the GPU still reads last frame's), then a copy pass into the GPU instance buffer — before any render pass begins.

**Alpha blending and sorting.** Transparent surfaces are blended: `result = src·α + dst·(1−α)`. This is *order-dependent*, so particles are **sorted back to front** by distance each frame and drawn **after** all opaque geometry. They **test** depth (hidden behind walls) but **don't write** it (so overlapping particles don't cut holes in each other).

**Texture atlas.** One 128×64 texture holds two 64×64 cells — a soft fog puff and a leaf — generated in code at start-up (white RGB, shape in alpha, tinted per particle). The atlas cell is chosen per instance, so all particles share one texture and one draw.

**Fog and near-fade.** The particle shader applies the same `ComputeFog`, and fades particles within ~1 m of the camera so a leaf never fills the screen.

**The simulation (game side, `Game/Atmosphere`).**
- *Leaves and ash* live in a box that **follows the player** and **wraps** at its edges: whatever drifts out one side re-enters on the other, so 160 flakes feel like a whole street's worth. They fall, sway (sine flutter), spin, and ride a **gusting wind** (product of slow sines).
- *Fog banks*: 36 large soft puffs near the ground, fading in and out over their lifetime, drifting with the wind, respawning upwind at the edge of a 32 m radius. Slightly brighter than the fog so they read as wisps.

**Cost.** ~0.15 ms. **F8** toggles.

**Code.** `Engine/Renderer/Particles.h`, `Renderer::UploadParticles`, `DrawParticles`, `Shaders/Particle.*.hlsl`, `Game/Atmosphere.*`.

---

## 26. Audio

### Digital audio in one paragraph
Sound is air pressure over time; digitally it's a list of **samples** (here 32-bit floats in −1..1) at a **sample rate** (48 000 per second). Stereo interleaves left/right: L R L R … One L/R pair is a **frame**. A 1-second stereo buffer = 48 000 frames = 96 000 floats.

### The mixer (`Engine/Audio/AudioSystem`)
- SDL opens the default device with an **audio stream** and a **callback**. On SDL's **audio thread**, whenever the device needs more data, the callback asks the mixer for N frames.
- **Voices**: each playing sound is a voice with a cursor into its buffer, pitch (playback rate), loop flag and gain. Mixing = for each output frame, sum every voice's current sample × its gain.
- **Pitch** by stepping the cursor by `pitch` per frame, with **linear interpolation** between neighbouring samples.
- **Click-free gain changes**: gains are *ramped* toward their targets over ~10 ms. An instant jump in amplitude is a discontinuity in the waveform, which you hear as a click.
- **Soft clipping** (`tanh`) on the final mix: if many sounds stack above 1.0 they saturate gently instead of distorting harshly.
- **Thread safety**: the game thread changes voices only while holding the stream lock (`SDL_LockAudioStream`); SDL holds the same lock while running the callback. So the two threads never touch the voice list at the same time. The lock is held briefly — the audio thread must never wait long or the sound stutters.

### 3D sound
- **Distance attenuation**: inverse-distance rolloff `gain = minDistance / max(distance, minDistance)`, plus a fade to exactly zero near `maxDistance` so far voices vanish instead of lingering faintly.
- **Panning**: how much the source lies to the listener's right = `dot(direction, listenerRight)`. It's mapped to left/right gains with a **constant-power pan law** (`cos`/`sin` of an angle), so a sound doesn't get quieter as it pans through the centre (a linear crossfade would dip ~3 dB there).
- The listener is the camera; vending machine hums and cicada calls are spatial, the ambient beds are not.

### Procedural sound design (`Game/SoundSynth`)
No sound files: every sound is synthesised at start-up from a fixed seed, with standard DSP building blocks:
- **White noise** → random samples; **one-pole low-pass** (`y += (x − y)·a`) removes highs (wind, rumble); **biquad** filters (RBJ cookbook) for band-pass/high-pass (cicada sizzle, radio hiss, footstep crunch).
- **Oscillators**: sums of sines (drone, 120 Hz compressor hum = 60 Hz mains doubled, as in western Japan).
- **Envelopes**: `e^(−t/τ)` decays for footsteps; `sin(π t / length)` swells for cicada notes.
- **Modulation**: amplitude modulation at ~37 Hz gives cicada buzz; a smoothed random "wander" drives gusts and swells.
- **Seamless loops**: the last ~2 s are **crossfaded** into the start with an equal-power fade, so a loop has no seam or click.
- Examples: *wind* = low-passed noise with a gust envelope; *higurashi* = a train of short trilled ~4.3 kHz notes falling in pitch and slowing; *footsteps* differ per surface (asphalt thud + scuff, dirt crunchy grains, stone bright click).

**Footsteps** trigger once per head-bob cycle (`PlayerController::GetStepCount`), with the surface chosen from where the feet are (road, gutter, shrine path, dirt). Since v0.0.2 the surfaces come from the level file (a default plus zones, §31) — e.g. earth in the machiya's doma, floorboards past the step — and a new *wood* footstep (hollow knock with a creak) and an interior **room tone** were added. Levels start and stop their own ambience; `AudioScape` is the named sound library plus the sounds that follow the player.

**Live synthesis (v0.0.13).** Everything above is synthesised once, then played. DRIFT's music is synthesised *live* on the audio thread by `Atom::Synth` and `SynthStream`, with no locks there: a lock-free queue carries the game's commands instead of the stream lock (§99).

**WAV support.** `AudioSystem::LoadWav` loads any WAV and converts it to mono 48 kHz float with `SDL_ConvertAudioSamples`, so recorded sounds can replace any synthesised one.

**Dev capture.** `ATOM_AUDIO_CAPTURE=file.wav` records the first minute of the final mix to a float WAV — used to verify levels, clipping and behaviour without listening (RMS per window = loudness; zero-crossing rate ≈ brightness).

**M** mutes.

---

## 27. Scripted atmosphere: the unease moments

Not engine features but **game-design systems** built on them (`Game/UneaseDirector`). The principle: *ambiguity over jump scares* — the player should wonder "was something there?".

- **The figure.** A thin dark silhouette (a few boxes, near-black material) stands at one of six hand-picked spots visible from the road. It **appears only where the player isn't looking** (angle test against the camera's forward vector) and at least 28 m away, always faces the player, and **vanishes once they come within 14 m**; it returns 25–45 s later. Lesson learned while building it: spots must be visible from farther than the vanish distance, otherwise the figure is never seen at all.
- **Radio static** (a nod to Silent Hill's radio) rises as the player approaches the figure — a looping static voice whose gain follows distance, eased in slowly and out quickly.
- **Vending machine flicker.** Near a machine, after a cooldown, the emissive screens stutter, go dark for a second and come back, while the hum dips — driven by changing the shared material at runtime (§13) and the hum voices' gain.

**F9** toggles; appearances are logged to the console.

In v0.0.2 the director's switches (e.g. whether the figure appears) come from each level's file (§31), so the shrine and the house interior stay free of the figure.

---

## 28. Text and UI overlay (M9)

**Concept.** Game text is not drawn from a font file directly: a **font** (TrueType, `.ttf`) describes glyphs as curves, and the GPU needs pixels. The standard approach is to **bake** glyphs once into a **glyph atlas** — one texture with every character rasterised — and then draw each character as a textured quad. A second standard idea is the **immediate-mode** UI: every frame, the game simply says "draw this rectangle, this text"; nothing persists, so there's no widget tree to keep in sync with game state.

**Font baking (`Engine/UI/Font`).**
- `stb_truetype` rasterises the ranges the game needs — ASCII, Latin-1, and typographic punctuation (curly quotes, dashes, ellipsis, used by dialogue) — into one coverage atlas at a fixed pixel height. Two fonts are baked: a large one (prompts, dialogue) and a small one (debug panel, speaker names).
- **2× horizontal oversampling**: each glyph is rasterised at double width. Text positioned between pixels then samples a sub-pixel-accurate glyph, so small text stays crisp. (Until v0.0.13 the atlas also had mipmaps, and with two texels per pixel the GPU sampled the half-size level: text was blurry from the start. §102 tells how it was found.)
- The atlas stores **white RGB with coverage in alpha**, so any colour is just a tint. It's a **linear** texture (not sRGB): alpha is a coverage fraction, not a colour.
- **Kerning** (per-pair spacing like "AV") is precomputed into a map of non-zero pairs, so the header doesn't expose stb types.
- The bundled font is a Latin subset of *Shippori Mincho* (SIL OFL) — a mincho (serif) face that suits the setting.

**The overlay (`Engine/UI/UIRenderer`).**
- API in window pixels, origin top-left: `DrawRect`, `DrawText`, `MeasureText`, `WrapText` (inserts line breaks at spaces to fit a width). Text is **UTF-8**; malformed bytes decode as `?`.
- Colours are given in **sRGB** (as picked in a paint program) and converted to linear for blending (§12).
- Solid rectangles sample a **white texel** in the atlas, so a single shader and pipeline draw both rectangles and text. Consecutive quads with the same texture are **batched** into one draw call.
- Vertices are uploaded each frame (like particles, §25) and drawn **after the post pass**, straight into the swapchain image with `LOAD` (keep what post wrote). So UI is never tonemapped, graded, fogged or grained, and is always at window resolution regardless of render scale (§20).
- The pen is snapped to whole pixels vertically; horizontal positions use the oversampling.

**What the game draws.** The fading controls hint at start, the `[E] prompt` of the current target, the message line (`MessageFeed`: one line of feedback that fades after a few seconds; a new message replaces the old), the dialogue panel (§30), and the **F1** debug panel (frame time, render settings, level, position, draws, voices). Since v0.0.13 the F1 panel is the engine's ImGui overlay, shared by every game (§101).

**Code.** `Engine/UI/Font.*`, `Engine/UI/UIRenderer.*`, `Shaders/UI.*.hlsl`, `Game/Interaction/MessageFeed.h`, `DemoApp::DrawOverlay`.

---

## 29. Entities and interaction (M10)

### Handles instead of pointers: the slot map
**Concept.** Objects come and go (a level unloads, an item is picked up). A raw pointer or index to a removed object **dangles**: it points at freed memory or at whatever reused the slot. A **generational handle** fixes this: a handle is `(slot index, generation)`. Each slot has a generation counter that increments when its object is removed. A lookup succeeds only if the generations match, so a stale handle simply returns `nullptr` — it fails safely instead of corrupting memory.

**AtomEngine.** `Atom::SlotMap<T>` (`Engine/Core/SlotMap.h`): O(1) insert/remove, slots reused through a free list, generation 0 never issued (so a default handle is "null"), and on wrap-around 0 is skipped. `EntityId` is such a handle. The game keeps its current target and dialogue speaker as `EntityId`s, so an entity vanishing with its level can't leave a dangling reference.

### Composition over inheritance
**Concept.** A class hierarchy (`Door : Interactable : Prop : Entity`) gets rigid fast — what about a door that's also a speaker? The common alternative is **composition**: an entity is a name and a transform plus a set of **optional capabilities**. Systems care about capabilities, never types. (Full ECS engines take this further, storing each component type in its own array; here plain `std::optional` members are enough.)

**AtomEngine.** `Entity` (`Game/World/GameWorld.h`) has `name`, `position`, and optional `Renderable` (a model and yaw — "this is drawn") and `Interactable` (prompt, action, focus point, radius, optional flag gate). The shrine keeper, a door, a letter on a table and a stone lantern are all just entities with different capability data.

### Actions as data
**Concept.** If each object's "on use" is code, content lives in C++ and changing a door means recompiling. Instead, describe *what happens* as **data**, and interpret it in one place.

**AtomEngine.** `Action` is a `std::variant` of plain structs: `ShowMessage`, `SetFlag` (+ optional message), `StartDialogue`, `ChangeLevel` (level + spawn), and since M19 `PlayAnimation` (entity + clip, §38). `ActionExecutor` performs them with `std::visit`; because `std::visit` must handle every alternative, **adding an action type without handling it is a compile error**, not a silent no-op. Interactables can be **gated**: without a required flag, `lockedAction` runs instead (the shrine gate says it's barred until the keeper allows you).

**Story flags.** `GameState` holds named flags (`keeper_permission`, `read_letter`, …). It is owned by the game, not the level, so progress survives level changes.

### Choosing the target
Every frame, `InteractionSystem::FindTarget` picks the best interactable:
1. **Reach**: the entity's focus point within its radius of the eye.
2. **View cone**: the angle between the camera's forward vector and the direction to the focus must be small — the cone **widens at close range** (≈35° at full reach, ≈70° at arm's length), because something right in front of you is off-centre even when you clearly face it.
3. **Line of sight**: `CollisionWorld::Raycast` from the eye to the focus point (ray–triangle tests against the collision mesh); a hit more than a small slack before the target means a wall is in the way.
4. Of the survivors, the one best aligned with the view wins.

The chosen target shows `[E] <prompt>`; E executes its action.

**Code.** `Engine/Core/SlotMap.h`, `Game/World/*`, `Game/Interaction/*`, `Engine/Physics/CollisionWorld::Raycast`.

---

## 30. Dialogue (M11)

**Concept.** Branching conversation is **content**, so it belongs in data files, walked by a small interpreter. Two further standard separations: a **state machine** makes the flow explicit (what can happen in each state), and **model/view separation** keeps logic independent from how it looks.

**Data (`Assets/Dialogue/*.json`, parsed with nlohmann/json).** A dialogue is a set of **nodes**; each node has a speaker and a line. A node with **choices** waits for one; a node without continues to `next`; `"end"` closes the conversation. Flags connect dialogue to the world: a choice can **require** a flag or be hidden by one (`requires` / `requiresNot`), and choosing it — or reaching a node — can **set** a flag (`sets`). The keeper's conversation changes after you bow at the torii, and a choice sets `keeper_permission`, which unbars the shrine gate.

**Validation on load**: unique node ids, the start node exists, every `next` and choice target points to a node or `"end"`. A broken file is reported and skipped, never half-loaded.

**Runner (`DialogueRunner`) — a state machine:**
```
Inactive --Start--> Revealing --(text done / Advance)--> WaitingForInput
WaitingForInput --Confirm--> Revealing (next node)  or  Ended (at "end")
```
It never draws and never reads devices: the game feeds it commands (`Advance`, `MoveSelection`, `Confirm`). *Revealing* is the **typewriter**: a character counter advancing at a fixed rate; E completes the line at once. Only choices whose flag conditions pass are visible.

**View (`DialogueView`).** Reads the runner's state and draws the panel: speaker name, wrapped text, choices with the highlighted one marked. Replacing the look never touches the logic.

**The game side.** Entering dialogue switches `DemoApp` to `Mode::InDialogue`: the player freezes, the camera **turns smoothly toward the speaker** (eased yaw/pitch), E/Space continues, W/S or 1–4 choose. Nothing else (prompt, messages) is drawn over the panel.

**Code.** `Game/Dialogue/*`, `Assets/Dialogue/*.json`, `DemoApp::BeginDialogue`, `UpdateDialogue`.

---

## 31. Levels and level transitions (M12)

### A level is a file
**Concept.** Hard-coding a map in C++ couples content to code. **Data-driven levels** describe a place in a file; the engine turns it into live objects.

**AtomEngine.** `Assets/Levels/<name>.json` describes: the scene model and collision mesh, named **spawns** (position + yaw), lighting (sun, sky, ground, exposure), fog, ambience beds and positional emitters (with groups, e.g. `vending`), **footstep surfaces** (a default plus rectangular zones on the ground plane; first match wins), particles, unease settings, `outdoor`, and **entities** with capabilities and actions (§29). `ParseLevel` produces plain `LevelData` and validates it (needs a spawn, the default spawn exists, vectors have 3 numbers, action types are known, surface names are known — §33). The street itself moved from C++ into `street.json`.

Four levels ship with v0.0.3: **A** the street, **B** the shrine grounds (through the barred gate), **C** a machiya interior (through the house door at the east end) and **D** the windmill field (through the field path at the west end, §38). v0.0.4 adds **E** the night street (by bus from the stop at the east end, §43–§44) and **F** the pachinko hall (through its doors on the night street, §45), plus `night_test`, a testbed. A level file may leave spawn and entity positions to Blender markers (§39).

### Ownership: what lives with a level
**Concept.** Most "leaks" in games are really *ownership* bugs: something from the old level keeps running (a sound humming forever) or keeps a pointer into freed memory. The cure is to make ownership explicit and let **RAII** (destructors) clean up.

**AtomEngine.** `Level` owns everything that exists only while you're there: scene and entity models, the collision world, the entities, and the ambience voices it started. The game owns what survives: player, camera, `GameState`, renderer, audio system, sound library, fonts, dialogue. `~Level` stops its voices; then members die in **reverse declaration order** — declared so that entities (which point at models) die **before** the models. Unloading is just destroying the `Level`.

### Changing level behind a fade
`LevelManager` is a small state machine:
```
Idle --RequestChange--> FadingOut --(black)--> swap --> FadingIn --> Idle
```
- The swap happens while the screen is **black**, hiding the loading hitch. The fade is a uniform in the post pass (a multiply toward black).
- The new level is **loaded before the old one is released** — briefly more memory, but a failed load leaves you where you were.
- Callbacks: `onUnloading` (systems drop anything pointing into the old level — e.g. the target handle, the footstep surface callback) and `onLoaded` (place the player at the spawn, apply lighting/fog/atmosphere).
- While transitioning, `DemoApp` is in `Mode::Transitioning`: input is ignored and the player doesn't update.

**The spawn bug (fixed in v0.0.2).** `onLoaded` moved the player's feet but not the camera, and the camera is otherwise only moved by the player update — which doesn't run during the fade. So the whole fade-in showed the new level from the *old* level's coordinates, then snapped. The fix, `PlayerController::Teleport`, sets feet **and** camera and clears momentum and head-bob (keeping the step counter, so arriving doesn't play a footstep). The general lesson: **any state that is rendered must be set when it changes, not only when some update happens to run.** §33 describes the check that now guards it.

**Developer switch.** `ATOM_START_LEVEL=<level>[:<spawn>]` starts anywhere.

**Code.** `Game/Level/*`, `Assets/Levels/*.json`, `DemoApp::OnLevelLoaded`, `OnLevelUnloading`.

---

## 32. Testing: unit tests and the in-game harness (M12–M13)

**Concept.** Two kinds of automated test complement each other:
- **Unit tests** exercise pure logic in isolation — fast, no GPU, precise failures.
- **Integration (scenario) tests** drive the *real* game end to end, catching what only appears when the systems run together.

A design choice makes both possible: gameplay code builds as a library, **`AtomGameLib`**, linked by both the game and the tests. And the logic worth testing (slot map, parsers, runner, dialogue, interaction choice) is kept free of GPU and window dependencies.

**Unit tests (`Tests/`, doctest).** Slot-map handle safety, raycasts, level and dialogue parsing and validation (including broken files), the dialogue state machine and flag gating, interaction targeting, and the test-script runner itself. `AtomTests` runs in a fraction of a second.

**The scenario harness (`Game/Testing/TestScript`).**
- `ATOM_TEST_SCRIPT=<file>` makes the game run a script and **exit with 0 (pass) or 1 (fail)**; each `Tests/Scenarios/*.atomtest` is registered with ctest.
- Scripts are one command per line and address entities **by name**: `teleport_to`, `face`, `interact`, `choose`, `finish_dialogue`, `wait_for_level`, `expect_flag`, `expect_message`, `expect_voices_max` (looping voices only, §39), `expect_surface`, `expect_animating`, `wait_for_animation`, `reload_level`, `expect_near`, …
- The whole script is parsed up front — unknown commands and wrong argument counts fail before anything runs.
- The runner executes **one command per frame** (waits span frames), so the game really updates between actions.
- Actions go through **the same code as the player**: `interact` fails if targeting wouldn't pick that entity, just as pressing E wouldn't.
- The game exposes a narrow `TestHooks` interface; unit tests implement it with a fake to test the runner without the game.

**Acceptance test.** `levels_roundtrip` walks A → B → A → C → A: the gate is barred, then unlocked through dialogue; flags survive level changes; and `expect_voices_max` checks after each change that no ambience voice leaked from the previous level.

**Code.** `Game/Testing/*`, `Tests/*.cpp`, `Tests/Scenarios/*.atomtest`, `Tests/CMakeLists.txt`.

---

## 33. Validation: catching bugs where they start (M14)

Two bugs shipped past every test in v0.0.2: the fade-in drawn from the wrong place (§31), and **z-fighting** walls in the machiya interior. Neither was covered, because no test looked at the camera during a transition or at the geometry. Rather than comparing screenshots (expensive, and it needs a human to judge), each class of bug gets a cheap, exact **check at the stage where it's introduced**.

### Z-fighting and the export lint
**Concept.** Two surfaces in the **same plane**, facing the same way and overlapping, have identical depth. The depth test (§9) can't order them, so rounding picks a winner **per pixel, per frame** — the surface flickers between the two materials as the view moves. It's not a depth-precision problem you can fix with a better near plane: the depths are *exactly* equal. The fix is geometric: pieces that butt into a wall must stop just behind its face.

In the interior, the corridor walls, the tokonoma side boards and the doma walls each ended exactly in the plane of the wall they met. They were shortened by 10 cm.

**The lint (`MeshBuilder.lint_coplanar`, `Tools/Blender/atom_kit.py`).** Every mesh is checked as it's built:
- Each face remembers **which call made it** (one `box` or `cylinder` = one piece), so a piece is never compared with itself.
- Axis-aligned faces are grouped by (axis, facing direction, plane offset to 0.1 mm); within a group, faces of different pieces are tested for rectangle overlap above 1 cm².
- **Different materials → error**: the build prints the plane, materials and overlap region, and **refuses to export anything**, like a compile error.
- **Same material → warning**: identical textures, so it isn't visible as a material flicker.

On its first run it also found hidden faces pressed against others (bottoms of pieces resting on floors, tops under the haiden roof). Those are simply no longer emitted (`faces=NO_BOTTOM` / `SIDES`), which also saves triangles. Limitation: it checks one mesh at a time, so two separate kit pieces placed flush in the street aren't compared.

### The arrival check
- `DemoApp` records where each load *should* put the camera — computed from the **spawn**, never read back from the camera (reading it back would compare the camera with itself; the first version did exactly that and passed with the bug present).
- **Harness:** on the first frame in every new level, the runner asks the game for the distance and yaw error and fails if they exceed 1 cm / 0.5°. Every scenario gets this for free; with the old bug it reports `camera is 34.4 m ... from the spawn`.
- **Debug assert:** during the fade-in, `SDL_assert` checks the same invariant, so a regression is caught in normal Debug play too.

### Level data checks
- **Surface names** are validated against one list (`FootstepSurfaces`) shared with the audio code — a typo used to fall back to dirt silently.
- **Spawns** are checked on load against the collision world: a floor within step height under the feet, and the player's body spheres not pushed by any wall (§15).
- `expect_surface` checks the footstep surface under the player from a scenario.

**The general lesson.** Test **invariants at their source**: geometry at export, data at load, state right after it changes. Such checks are exact, cheap, and point straight at the cause. Image comparison only sees the symptom.

**Code.** `Tools/Blender/atom_kit.py` (`lint_coplanar`), `Tools/Blender/build_assets.py`, `Game/Testing/TestScript.cpp` (`CheckArrival`), `DemoApp::Arrival`, `Game/Level/Level.cpp` (`CheckSpawn`), `Game/Level/LevelData.cpp`.

---

## 34. Baked lighting I: vertex colours (M15)

**Concept.** Real-time lights are cheap to change but expensive to make *soft*. Light that doesn't move — the sky, bounce light off walls, the darkness in a corner — can be computed once, offline, and stored. This is **baked lighting**. Early-2000s games lived on it: consoles like the PS2 had little per-pixel lighting, so most of the mood was baked. The simplest store is one colour per **vertex**, interpolated across each triangle by the rasterizer.

**What gets baked.** Blender's path tracer, **Cycles**, computes how much light reaches each vertex:
- *Outdoors*, from a uniform white sky of strength 1 plus up to three bounces. An unoccluded, upward-facing point reads 1.0, a vertex under an eave reads less, and a vertical wall reads about half. The value is *sky visibility plus bounce*.
- *Indoors*, **ambient occlusion** within 1.5 m: how enclosed a point is. A closed room gets no sky at all, so a sky bake of the machiya interior came out almost black (median 0.004). Its real light comes from the lightmap (§35).

**How the shader uses it.** Without a bake, ambient light is the hemisphere estimate (§13): a blend between ground and sky colours by the normal's up-facing. With a bake, the ambient becomes `skyColor × vertexColor`, because the bake already contains the orientation and occlusion that the hemisphere term only approximates. The two are blended by a per-draw weight: 1 for baked meshes, 0 for everything else (e.g. the keeper's model when unbaked). The sun stays dynamic, with its shadow map (§23).

**Vertices to store it on.** A 10 m wall made of one quad has four vertices, so it can only hold one smooth gradient. `MeshBuilder` therefore **tessellates** large quads into a grid: 1 m cells by default (about PS2 density), 0.5 m in the interior. This happens *after* the z-fighting lint (§33), so the lint still compares the authored faces, and a face is never compared with its own cells. Cells share their grid vertices.

**Per-instance light.** The street places kit pieces as linked duplicates, all sharing one mesh. Each placement has different light, though, so every placement gets its own mesh copy before baking. That's why `street.glb` grew from 0.8 MB to 3.3 MB (7 MB at a 0.5 m grid).

**Storage.** The colour is baked per vertex (point domain, which keeps shared vertices shared), quantised to 8 bits, exported as glTF `COLOR_0`, and read as 16-bit normalised in the engine (`USHORT4_NORM`).

**Determinism.** CPU rendering, a fixed seed, fixed samples, no denoiser and 8-bit quantisation make the bake byte-identical across rebuilds. The first step of the milestone was a test: bake twice, compare hashes.

**Checks.** A mesh whose bake is all black (no light reached it) or all white (the bake didn't run) fails the asset build. A unit test checks that every shipped visual model carries a real bake, and that models without `COLOR_0` read as white and unbaked.

**Controls.** Per level, `lighting.bakedLight` (0..1). **F3** turns baked light off to compare.

**Code.** `Tools/Blender/atom_bake.py`, `MeshBuilder._tessellated` (`atom_kit.py`), `Engine/Renderer/Mesh.h` (`Vertex::color`, `HasBakedLight`), `Engine/Assets/Model.cpp` (`LoadModelGeometry`), `Shaders/Basic.frag.hlsl`.

---

## 35. Baked lighting II: lightmaps (M16)

**Concept.** Vertex colours can't resolve a pool of light in the middle of a floor or a soft shadow across a wall. A **lightmap** is a texture of baked light that gives lighting its own resolution. It needs a **second UV set** in which no two faces overlap, because every point on the surface must own its own texels. (The first UV set tiles textures and repeats freely.)

**Unwrapping.** Blender's *Smart UV Project* keeps flat, connected faces together in one **chart**, so a whole wall stays one piece with no seams inside it. Charts are packed with a margin of a few texels. Bilinear filtering, and the first mip levels, read neighbouring texels, so without the margin the light of one chart would **bleed** into the next.

**Baking.** Cycles bakes diffuse direct and indirect light into a 512² float image. The lights exist only for the bake: warm area lights just in front of the four shoji panels (daylight through paper) and a cooler one at the entrance. The sky is set black, so nothing leaks through the walls.

**Storage.** The result is scaled, clamped and **sRGB-encoded** (§12), which spends the 8 bits where the eye sees differences, in the darks. It is written by a minimal PNG writer of our own (header, one IDAT chunk, no metadata), so the file is byte-identical across rebuilds. Blender's writer isn't guaranteed to be.

**Engine.**
- Vertices gain `lightmapUv` (`TEXCOORD_1`).
- A material may reference a lightmap; the level file names it: `"lightmap": { "texture": ..., "intensity": ... }`.
- The shader samples it and **replaces** the ambient term with it.
- A dedicated sampler clamps at the edges and stops after two mip levels. Smaller mips would average neighbouring charts into each other.
- A level that names a lightmap that is missing **fails to load**, rather than silently falling back to flat light.

**Lint.** Lightmap UVs must lie inside 0..1, and no two faces may overlap. The check rasterises faces into a coarse grid and flags any pixel centre covered twice. Pixels within 1e-5 of an edge are skipped, so faces sharing an edge inside a chart never count as overlapping.

**Code.** `Tools/Blender/atom_lightmap.py`, `Renderer::LoadTexture`, `Model::SetLightmap`, `Level::Create`, `Shaders/Basic.*.hlsl`.

---

## 36. Alpha-tested materials (M17)

**Concept.** Leaves, grass, chain-link and ragged cloth are too detailed to model. Instead, a flat card is textured with the shape in the **alpha** channel. **Alpha testing** draws a pixel or discards it (`clip(alpha − cutoff)`): there is no partial transparency, so no sorting is needed, and depth is still written. glTF calls this `alphaMode: MASK`, with an `alphaCutoff`. Thin cards are **double-sided**: back faces aren't culled, and the normal is flipped on the back face (`SV_IsFrontFace`) so the side you see is lit correctly.

**Shadows must alpha-test too.** Otherwise leaves cast solid rectangles. The shadow pass therefore gets the UVs and the base texture, and discards the same pixels.

**Leaves lit from behind.** In reality thin leaves let light through. In the shader, a masked card lit from behind takes half the sun (`max(N·L, 0.5·max(−N·L, 0))`). Without that, backlit trees read as black silhouettes.

**Alpha-to-coverage (A2C).** With MSAA, the shader's alpha can decide *how many of a pixel's samples* are covered, which gives antialiased edges without sorting. The shader sharpens alpha to a ramp about one pixel wide around the cutoff (`(a − cutoff) / fwidth(a) + 0.5`). A2C needs an alpha channel in the render target, though, and the compact HDR format R11G11B10 (§18) has none. So the engine uses A2C only when the target is RGBA16F, and a plain alpha test otherwise. On the dev laptop that means a plain test. The Debug build's SDL assert is what caught this.

**Pipeline variants.** Scene pipelines exist for each combination of MSAA sample count, face culling and A2C. A draw rebinds only when its variant changes.

**Content lessons.**
- *Premultiplied alpha.* Blender stores images premultiplied by default, which blackens the colour of transparent texels. Alpha images are therefore **channel-packed** (colour and alpha independent).
- *Alpha dilation.* Transparent texels are filled with the mean colour of the opaque ones. Bilinear filtering and mipmaps blend transparent texels into the edges, so black ones would draw a **dark fringe**. They would also read as zero albedo in the bake, which divides out albedo, and therefore as "no light".
- *Baking cards.* A vertex sitting on a transparent texel of its own card reads no light. A second bake pass makes the cards opaque while hiding them from shadow and bounce rays (so a canopy of now-solid cards doesn't bury itself). Card vertices that read nothing in the first pass take the second pass's value.

**Checks.** The asset build fails if a masked texture's alpha never crosses its cutoff (it would draw a solid card, or nothing). A unit test checks that the shipped masked materials are double-sided masks.

**Code.** `Material::alphaMode/alphaCutoff/doubleSided`, `Renderer::GetScenePipeline`, `Shaders/Basic.frag.hlsl`, `Shaders/Shadow.*.hlsl`, `atom_kit.py` (`MASKED`), `atom_textures.py` (`_with_alpha`).

---

## 37. Decals (M18)

**Concept.** A **decal** is a layer of detail lying *on* another surface: a stain, grime, a sign, a road marking. Being coplanar is its whole point, so it must never z-fight (§33). The standard recipe:
- place it a hair above the surface (here 2 mm);
- draw it **after** all opaque geometry, **alpha-blended** (`src·a + dst·(1−a)`);
- **test depth but don't write it**, so the surface below keeps its depth and overlapping decals all show;
- add a **depth bias** toward the camera (constant plus slope-scaled), so precision never loses the 2 mm.

Decals cast no shadows.

**In AtomEngine.** glTF `alphaMode: BLEND` marks a decal. In Blender, alpha goes straight into the shader, which the exporter writes as BLEND. The draw loop runs in two phases: everything else, then decals (scene pass only). The road's white lines were already a quad 4 mm above the asphalt, i.e. an unflagged decal; they became a real one.

**The lint learns "near-coplanar".** Faces of different pieces that are parallel, overlap, and lie less than 5 mm apart are now an error, unless one of them is a decal. At a distance, depth precision can't separate them either. The search sorts faces by plane offset per axis and facing direction, then checks a sliding window. Its first run found the stone lanterns' paper windows only 2 mm off the stone; they moved out to 6 mm.

**Limit.** The lint compares faces within one mesh. Decals placed as a separate mesh (the street's wall stains) aren't checked against the walls.

**Content.** Water stains and grime on the machiya, the shrine wall and the interior; paper ofuda on the shrine gate; a faded shop sign; crossing diamonds on the road.

**Code.** `AlphaMode::Blend`, `Renderer::GetDecalPipeline`, `DrawQueue` (phases), `atom_kit.py` (`DECALS`, `DECAL_OFFSET`, `NEAR_COPLANAR`, `lint_coplanar`).

---

## 38. Animation and vertex sway (M19)

### Rigid node animation
**Concept.** glTF stores animation as **clips**. A clip is a set of **channels**, each of which drives one node's translation, rotation or scale through **keyframes**. Playing a clip means sampling every channel at time *t* and composing each node's local matrix `T·R·S` under its parent's. Interpolation is:
- **linear**: `lerp` for vectors, **slerp** for quaternions. Slerp follows the arc of the rotation sphere, so the angular speed is constant;
- **step**: hold the previous key;
- **cubic spline**: Hermite curves using the stored in and out tangents.

Times outside the keys clamp to the first or last key. This is *rigid* animation: whole parts move, nothing bends. Bending needs skinning (joints and weights), which came in v0.0.6 (§53).

**In AtomEngine.**
- `Model` keeps the node hierarchy and its clips.
- A part moves if a clip targets its node or any ancestor. Only those parts are posed at submit time; every other part keeps the transform baked at load, so static scenes cost nothing extra.
- `SampleChannel` is a pure function, and unit-tested.
- Entities gain an **`animation`** capability: clip, loop, autoplay, speed, and a sound fired N times per loop. The windmill creaks on every quarter turn, because a beat is `duration / N` and the sound plays whenever the time crosses one.
- A **`playAnimation`** action starts a clip. A finished one-shot stays finished, so an opened door stays open.
- In Blender, an animated kit piece is a parent object with a child part that carries a named action (`spin`, `open`, `swing`). The exporter writes the action as a glTF clip, and the kit export and bake include the children.

### Vertex sway
**Concept.** Foliage and cloth moving in the wind needs no simulation. The **vertex shader** offsets each vertex by a lean with the wind, plus a few sines of time and world position, so neighbouring cards don't move in lockstep. The offset is scaled by a per-vertex **weight**: 0 at the root, 1 at the free end. Tips move quadratically more than roots.

**In AtomEngine.**
- The weight lives in the baked colour's **alpha**, stored as `1 − weight`, so every unbaked mesh (alpha 1) stays rigid by default.
- The asset build writes a gradient per card face: from the bottom up for grass and leaves, and from the top down for the noren, which hangs.
- The wind comes from the atmosphere's gusts (§25) through a per-frame vertex uniform, and is zero indoors.
- The shadow pass sways too, so shadows move with the leaves.

**Harness.** `expect_animating <entity>` checks that a clip's time advances; `wait_for_animation <entity>` waits for a one-shot to finish.

**Level D, the windmill field.** Reached from the west end of the street, it is the testbed:
- a windmill whose sails turn and creak;
- a hanging sign that swings;
- a shed whose door slides open;
- long grass and shrubs swaying in the gusts.

**Code.** `Engine/Assets/Animation.*`, `Model::Submit(…, clip, time)`, `Game/World/GameWorld.h` (`Animated`), `Level::Update/PlayAnimation`, `Shaders/Sway.hlsli`, `atom_kit.py` (`SWAY`, `_animate`, `build_windmill`), `atom_bake.py` (`_apply_sway`).

---

## 39. Authoring iteration (M20)

**Concept.** Content work is a loop: edit, load, look, fix. Every second shaved off that loop, and every error that points straight at its cause, compounds. A full editor is a large project. These four tools cover most of the loop.

### Schemas
A **JSON Schema** describes a file's shape: keys, types, allowed values, and what is required. Editors such as VS Code read a file's `"$schema"` and then offer completion and underline mistakes while you type.
- `Assets/Schemas/level.schema.json` and `dialogue.schema.json` list every key the parsers read, with `additionalProperties: false`, so a misspelt key is flagged.
- The **C++ parser stays the authority**. The schema is a help for editing, not a second definition. A unit test keeps the two in step: every top-level key in a shipped file must appear in its schema.

### Errors that say where
- *Syntax* errors give a line and column (`street.json:line 20, column 19: … unexpected ','`). nlohmann's parse error carries the failing byte offset, and counting newlines up to it gives the line.
- *Content* errors give a **JSON Pointer** (RFC 6901), the path to the exact value: `/entities/1/interactable/action/type: unknown action type "explode"`. The same holds for dialogue (`/nodes/1/choices/1/next: …`).
- A value of the **wrong type** is now an error. It used to be silently replaced by the default, which is the worst outcome: a typo that looks like it worked.

### Hot reload
- `ATOM_ASSET_ROOT=<repo>` makes the game read the source tree instead of the build's copy.
- A `FileWatcher` polls **modification times** once a second. Polling is simple and portable, and cheap for a handful of files; a file appearing or disappearing also counts as a change.
- It watches the current level's files: level JSON, markers, models, collision, lightmap and entity models. It also watches the dialogue files.
- On a change, `LevelManager::Reload` builds the new level **before** releasing the old one, as for a level change (§31), but with no fade and no teleport, so the player stays put.
- A broken edit keeps the old level running and shows the error on screen. The watcher then waits for the next edit rather than retrying every second.
- The harness has `reload_level` and `expect_near`.

### Blender markers
Placement belongs in the 3D tool, behaviour in data.
- Level builders place empties named `spawn:<name>` and `entity:<name>`.
- The asset build writes them, converted to game axes (x, z, −y) and sorted for determinism, to `Assets/Levels/<level>.markers.json`.
- The level loader merges the markers: a level file may leave out a position (or yaw) and take it from the marker. Where both give a value, the file wins.
- A marker with no entity of its name is a load error, and so is an entity with neither a position nor a marker.
- Yaw conventions: a spawn marker looks along its local +Y (game yaw = −rotation); an entity marker turns the model like a kit piece (yaw = rotation).
- The windmill field is placed this way.

**Also in M20.**
- **Leak checks.** They count **looping** voices only: a leak is something that plays until it's stopped, while one-shots (a cicada call, a creak) end on their own. Counting one-shots made the checks flaky.
- **Build exit code.** Blender exits with 0 even when a `-P` script raises, so the build script now catches the exception and exits with 1.

**Code.** `Assets/Schemas/*`, `Game/Level/JsonText.h`, `Game/Level/LevelData.cpp` (paths, typed getters, markers), `Game/Level/FileWatcher.h`, `LevelManager::Reload`, `DemoApp::UpdateHotReload`, `build_assets.py` (`write_markers`), `atom_street.py` (`add_marker`).

---

## 40. Representation and performance foundations (M22)

**Concept.** A small playable area can imply a much bigger place if what's far away is drawn cheaply. Games do this with **representation layers**:
- **near** (0–40 m): full geometry, collision, baked light, decals, animation;
- **middle** (40–150 m): simple box shells, no collision, no shadows;
- **far** (150 m+): flat cards and the sky.

To make that affordable the engine must stop thinking of a level as one model. It needs pieces it can cull, skip in the shadow pass, and share.

### Chunks and layers
- A level lists **chunks** (`chunks`: name, model, optional collision, `layer` near/mid/far, `castsShadow`, optional `cell`). A chunk is about one building or half a block.
- Each chunk is drawn and culled as a whole: `Renderer::BeginChunk`/`EndChunk` wrap its draws with a `ChunkInfo` (bounds from the model, layer, shadow flag). One box test against the frustum decides for all of its draws.
- `castsShadow` defaults to true for near chunks and false for mid and far ones. A chunk that casts no shadow never enters the shadow pass: distant shells there would cost and show nothing.
- The rural street was split into a base (ground, wires, decals that span it) and three chunks along the road, with no visual change.

### Cells
**Concept.** Classic games (and "portals" in general) divide a place into connected rooms and draw only the room you're in and the rooms next to it. The **layout** hides the rest: a bend, an alley end wall, a footbridge.

**AtomEngine.** `cells` are rectangles on the ground plane with a list of neighbours. `VisibleCells(cells, position)` returns the player's cell and its neighbours; near chunks of any other cell are skipped. Mid and far chunks always draw. Outside every cell (a gap, a teleport) everything is drawn rather than risk a hole.

### A uniform grid for collision
**Concept.** A triangle soup tested triangle by triangle is O(n) per query. A **broad phase** first narrows down which triangles could be involved. The simplest is a **uniform grid**: each triangle is listed in every cell its bounds overlap, and a query only looks at the cells it touches.

**AtomEngine.**
- `CollisionWorld` builds a 4 m XZ grid lazily, on the first query after triangles change (`Append` adds a chunk's collision).
- A triangle spanning several cells would be found several times; a per-query **mark** skips repeats.
- Candidates are sorted back into triangle order before testing, so every query returns *exactly* what the brute-force scan returned. Unit tests prove it against the old scan; the grid is about 5× faster on the street.

### Draw sorting and the model cache
- **Draw sorting.** Before drawing, commands are stable-sorted by (decal, alpha-masked, double-sided, material, mesh). Draws that share a pipeline and a material follow each other, and the renderer skips rebinding what's already bound. The street's 238 draws need 2 pipeline binds and about 80 material binds.
- **`ModelCache`.** A model used by two levels, or by several entities, is loaded once. Entries are **weak pointers**: a model dies with the last level that uses it, and during a level change the next level picks up the models the old one still holds. A file changed on disk is loaded again (hot reload, §39). Materials are shared with the model, so runtime edits (a flickering sign, a lightmap) reach every user.
- **F1 per layer.** The overlay shows, per layer: chunks visible/submitted, draws, triangles and shadow draws.

**Code.** `Engine/Renderer/Renderer.*` (`ChunkInfo`, `RenderLayer`, `LayerStats`, `SortDrawCommands`), `Engine/Physics/CollisionWorld.*`, `Game/Level/ModelCache.h`, `Game/Level/LevelData.*` (`ChunkData`, `CellData`, `VisibleCells`), `Level::Submit`, `build_assets.py` (`split_into_chunks`), `Tests/CollisionGridTests.cpp`.

---

## 41. Night rendering (M23)

### Emissive masks
**Concept.** Until now a glowing material glowed with its base colour (the vending machine front). At night that's too coarse: a building's lit windows, a sign's tubes and its dark board all share one texture. An **emissive texture** (a mask) says exactly which pixels emit light and in what colour, independently of how the surface looks unlit.

**AtomEngine.** `Material::emissiveTexture` (glTF `emissiveTexture`, fragment slot t3). The shader's emitted term is `emissiveMask × emissiveFactor` when a mask exists, `baseColor × emissiveFactor` otherwise. Blender materials get their masks from `atom_kit.EMISSIVE` (mask generator, strength, fog amount).

### Fog amount
A light seen through fog still reads as a light: the glow reaches you even when the wall around it has faded. Each material carries a **fog amount** (glTF extras `atom_fog`, 0..1): the share of the runtime fog it receives. Signs and lamps take about a third; far cards (§42) get less than full fog so the skyline doesn't vanish.

### Glow
**Concept.** **Bloom** (here, "glow") imitates light scattering in the eye and the lens: bright things bleed into their surroundings. The standard recipe:
1. a **bright pass** keeps only what's brighter than a threshold;
2. a **blur** spreads it;
3. a **composite** adds it back over the image.

**AtomEngine** (`Engine/Renderer/Glow.*`):
- The bright pass runs at **¼ size** of the scene, with a **soft knee**: brightness above the threshold eases in over a band instead of switching on, so nothing pops.
- The blur is a **separable Gaussian**: one horizontal and one vertical pass do what a 2D kernel would at a fraction of the taps. 9 taps are read as 5 samples by placing each between two texels, so bilinear filtering blends them for free; the rounded weights are normalised so the total energy stays 1. At ¼ size the blur spans about 36 scene pixels.
- The post pass adds the glow in HDR, before tonemapping, with a slight shimmer (a period, analogue feel).
- Levels set `lighting.glow` (strength, threshold). It costs about 0.09 ms on the Iris Xe.

### Halos
Soft additive glows around lamps and signs, drawn as **billboards** through the particle path (§25) with an additive-blend pipeline variant. They're submitted separately (`SubmitHalos`) and drawn after the particles with a first-instance offset into the same instance buffer. A halo can **flicker** (§43).

### The night sky
**Concept.** An **equirectangular panorama** maps longitude to x and latitude to y, so one 2:1 image covers every direction.

**AtomEngine.** The sky is one fullscreen triangle on the far plane (`z = w`, depth 1, behind everything). Each pixel rebuilds its view direction from its clip position with the inverse view-projection of a camera at the origin (**rotation only**), so the panorama stays fixed in the world as you turn and never moves as you walk. The Blender build paints it (deep blue overhead, a city's orange glow low in the haze, stars, faint clouds) and writes it with the deterministic PNG writer.

**Code.** `Shaders/Basic.frag.hlsl` (emissive, fog amount), `Shaders/GlowBright/GlowBlur.frag.hlsl`, `Shaders/Post.frag.hlsl`, `Shaders/Sky.*.hlsl`, `Shaders/Halo.frag.hlsl`, `Renderer::SubmitHalos/DrawSky`, `atom_textures.py` (`night_sky`, `neon_sign`), `atom_kit.py` (`EMISSIVE`, `build_street_lamp`, `build_neon_sign`), `Assets/Levels/night_test.json`.

---

## 42. Middle and far layers: shells, impostors and skyline cards (M24)

### Facade shells (middle layer)
Box buildings with one **facade atlas**: four styles (office block, tiled flats, dark brick, grey tower) in the four quadrants of one texture, lit windows in its emissive mask. Each face maps a whole quadrant, about 3 m a floor. No collision, no shadows, no bake: lit windows come from the mask, the rest from the level's ambient. They ring the playable area 40–90 m out.

### Impostors
**Concept.** An **impostor** replaces a detailed object with a picture of it on a flat card that turns to face the camera. Far enough away, the eye can't tell. Rendering the object from several directions and showing the view closest to the current one keeps the picture right as you walk around it.

**AtomEngine.**
- **Baking** (`atom_city.render_impostor`): Cycles renders the building **orthographically** (like a card seen from far away) from **8 directions** into one RGBA atlas, 128×256 per view, on the CPU with a fixed seed (byte-identical rebuilds). Transparent texels are **dilated**: they take the colour of their opaque neighbours, so filtering never pulls a dark fringe into the edges. A small JSON descriptor records the atlas, the view count and the card size.
- **At runtime** (`Game/World/Impostors.*`): one quad per view, and a material whose base colour is black and whose emissive texture and alpha both come from the atlas (the render already holds the lighting). The card turns to face the viewer; `SelectImpostorView` picks the view rendered from the closest direction.
- **Hysteresis.** Near the boundary between two views the choice would flip back and forth every frame as the camera sways. The current view is kept until the camera is 7.5° past the boundary; only then does it switch. Unit tests swing the camera across a boundary and check that the view never flickers.
- Levels place impostors (`impostors`: descriptor, position, yaw, layer); each card is its own chunk in its layer, never in the shadow pass.

### Skyline cards (far layer)
Three rings of inward-facing, **alpha-tested** silhouette cards at 180, 260 and 380 m, taller the further out, so each shows above the one in front. Their texture repeats at different rates per ring, so the silhouettes don't line up, and parallax between the rings sells depth in front of the panorama. Lit windows glow from the mask with little fog.

**Code.** `Tools/Blender/atom_city.py`, `atom_textures.py` (`facade_atlas`, `skyline`), `Game/World/Impostors.*`, `Level::Submit`, `Assets/City/`, `Tests/ImpostorTests.cpp`.

---

## 43. The night street: cells, night lightmaps, live lights, wet road (M25)

**Level E**, the night street, is the first place built for the layered approach: four cells (the bus stop, the main street, a narrow alley and the pachinko front), with the mid shells, skyline and impostors of §42 around them.

### Layout does the culling
Each cell is one mesh and one near chunk. The alley's far end is a wall; the plaza opens off the alley to the side; an elevated railway crosses over the street. From the main street, no line of sight reaches the pachinko front, so it isn't drawn until you're in the alley. Buildings that form a neighbouring cell's walls belong to the cell they're seen from.

### A lightmap per chunk, baked in context
- A whole street at a useful density doesn't fit one texture, so **chunks carry their own lightmaps** (`chunks[].lightmap`): 1024² for the main street and the pachinko front, 512² for the others.
- Each cell is baked **with the other cells present** (`context`): they cast shadows onto it, bounce light onto it and, being emissive, light it. A lamp near a border lights both sides the same, with no seam.
- Bake-only lights: point lights in every lamp head, an area light outside each lit shop window, the pachinko front's floodlight, vending machines, and a weak **sun** for moonlight. Signs and windows add their own emission to the bake: neon spill for free.
- Small, very bright emitters make Cycles leave isolated bright texels (**fireflies**). The night bakes **clamp** indirect samples to remove them.

### Live lights
**Concept.** Baked light can't move or flicker. The few lights that must are computed per pixel at runtime.

**AtomEngine.**
- Up to 4 **point lights** per frame (`Renderer::SubmitLiveLight`, level `lights`) reach the shader in the per-frame uniforms. Each is Lambert with a falloff `(1 − d/radius)²` that reaches zero exactly at its radius, so its reach is exact and cheap to reason about.
- A light, a halo or an emissive material can **flicker**. `FlickerFactor(time, seed, amount)` multiplies three incommensurate sines: their product rarely peaks, so the drops are short and never rhythmic. The seed is the *position*, so a halo, a live light and a sign's material at the same place stutter together.
- A light or halo can ride on an entity (`entity`, position as an offset).
- **Movers** (`mover` on an entity) shuttle it between two points: wait, travel, wait, travel back. The elevated train uses one; its headlights (halos), its live light and its sound follow it across the street. A looping sound on a mover plays only while it moves and pans across the stereo field as it passes.

### The wet road
No reflections are computed. The look comes from:
- dark wet asphalt;
- **reflection decals**: soft coloured streaks under each sign and window, one colour per column of a single texture;
- puddle decals;
- a **wet** material setting (glTF extras `atom_wet`). On wet materials the shader breaks the emitted light (the reflections) into slow ripples of world-space value noise, stretched along the street, and lays a faint moving sheen that is brighter where the ground is lit.

### Ambience per cell
`audio.zones` give each cell its own beds (traffic at the bus stop, voices and bicycle bells on the main street, a drone in the alley, the pachinko hall leaking through its doors). All of them play from the start at zero; each frame the beds of the player's cell step toward full and the others toward silence (`StepTowards`, a linear crossfade over `zoneFadeSeconds`).

**Code.** `Tools/Blender/atom_night.py`, `atom_lightmap.py` (point and sun lights, `context`, `clamp`), `Game/World/LiveEffects.*`, `Level::Update/Submit` (movers, lights, zones), `Engine/Renderer/Lighting.h` (`LiveLight`), `Shaders/Basic.frag.hlsl` (`LiveLights`, wet), `Game/SoundSynth.cpp` (traffic, neon buzz, voices, bells, pachinko leak, train), `Tests/NightStreetTests.cpp`, `Tests/Scenarios/night_street.atomtest`.

---

## 44. Action sequences: the night bus (M26)

**Concept.** An interaction that does one thing (a message, a flag, a level change) isn't enough for a moment that plays out over time: wait, something approaches, a sound, a door, a fade. A **sequence** is a list of timed steps written as data and run by a small interpreter. It's the same idea as actions (§29), stretched over time.

**AtomEngine.**
- Levels define named `sequences`. Steps: `wait`, `message`, `setFlag`, `show`/`hide` an entity, `playSound` (optionally following an entity, optionally looping), `playAnimation`, `moveEntity` (eased to a stop, like a vehicle braking), `changeLevel`. A `sequence` action starts one.
- `SequenceRunner` executes instant steps until one takes time, and carries leftover time into the next step, so a sequence runs the same at any frame rate. It doesn't know the game: it calls **hooks** (message, move, play…) that `DemoApp` supplies, which keeps it unit-testable.
- While one runs the game is in `Mode::InSequence`: the player is frozen, nothing can be targeted, and a second sequence can't start.
- The parser checks that every entity a step names exists, that every sequence an action names exists, and that `changeLevel` is the last step (after it the level is gone).
- Entities can start **hidden**; a hidden entity isn't drawn, and neither is anything riding on it.

**The bus.** At the rural stop (east end of the street) and the city stop (by the railway), waiting starts the sequence. The bus, hidden far down the road, appears and drives in with its headlights and engine, then the doors fold open with a hiss (a `doors_open` clip). The fade then takes you to the other stop.

**Harness.** `wait_for_sequence`. The roundtrip scenario now rides street → city → street and checks the frozen player, the flags and the arrival spot.

**Code.** `Game/Interaction/Sequence.*`, `Game/Level/LevelData.cpp` (`ParseStep`), `Level::SetEntityVisible/SetEntityPosition/PlaySound`, `DemoApp::RunSequence/UpdateSequence`, `atom_kit.py` (`build_bus`, `build_bus_stop`), `Tests/SequenceTests.cpp`.

---

## 45. Render-to-texture and the pachinko hall (M27)

### Render-to-texture
**Concept.** A texture doesn't have to come from a file. The GPU can draw into it (a **render target**) and then sample it like any other texture. That's how in-world screens, mirrors and security cameras work.

**AtomEngine.**
- `Texture::CreateRenderTarget`: a one-mip RGBA8 sRGB texture usable both as a colour target and in a sampler.
- `RenderTexture` pairs one with a **canvas**: the same immediate-mode 2D batcher as the UI (§28), at the target's fixed virtual resolution (320×240). Whatever is drawn into the canvas during a frame is rendered into the texture before the scene pass, so the scene samples this frame's picture.
- Render targets are marked as **pixel art** and sampled with **nearest** filtering: up close, the screen shows square pixels instead of a blur.
- F1 counts render textures drawn and scene draws sampling one; the harness checks them with `expect_screens`.

### A fixed timestep
**Concept.** A simulation advanced by the frame's `dt` behaves differently at 30 fps and at 144 fps: collisions are missed, bounces change. A **fixed timestep** accumulates frame time and spends it in whole steps of a constant size. After a long hitch only a few steps run and the backlog is dropped (otherwise the catch-up takes longer than the frame, the next frame has even more to catch up: the "spiral of death").

**AtomEngine.** `FixedStep`: 1/60 s steps, at most 5 per frame. Unit tests show 60 steps per second from 144 fps and from 30 fps, and a capped two-second stall.

### The attract loop
`PachinkoAttract` is what a machine shows while nobody plays:
- balls fall through staggered pins and bounce off the reel window;
- one landing in the start pocket spins the three reels and rolls up a 7-segment score;
- a ring of chasing bulbs flashes on a win.

Everything is plain rectangles. It runs on the fixed step with a **seeded xorshift** generator, so it's **deterministic**: the same seed and step count give the same picture on any machine at any frame rate. Tests check that, and that balls stay on the field and score. Levels map it onto materials (`screens`: material name, seed). Two seeds alternate along the rows so neighbours don't play in sync. The playable game (v0.0.5) will draw into the same kind of texture.

### Room reverb
**Concept.** A room answers a sound with a dense tail of reflections. Schroeder's classic reverb imitates it with **comb filters** (a delay fed back into itself, each an echo that repeats and decays) in parallel, followed by an **all-pass** filter that smears those echoes into a wash without colouring the tone.

**AtomEngine.** `Reverb` (in `Engine/Audio`) runs four damped combs and an all-pass per channel on the master mix, with slightly different delays left and right for width. The damping makes highs die first, as in a real room. Levels set it with `audio.reverb` (mix, size, feedback); leaving the level turns it off. It is pure DSP, unit-tested for silence when off and for a tail that decays.

### Level F, the pachinko hall
Behind the city's pachinko doors:
- four double rows of machines under fluorescent panels, and a prize counter at the back;
- one 1024² lightmap baked from the panels, with the machines' faces and lamp boxes bleeding colour onto the carpet;
- the hall's loud bed with the reverb inside, and the low-passed leak outside.

**Code.** `Engine/Renderer/RenderTexture.*`, `Texture::CreateRenderTarget`, `Renderer::CreateRenderTexture/RenderTextures`, `Game/World/FixedStep.h`, `Game/World/PachinkoAttract.*`, `Engine/Audio/Reverb.h`, `Level::Create/Update` (screens, reverb), `Tools/Blender/atom_pachinko.py`, `Tests/PachinkoTests.cpp`.

---

## 46. The asset build at scale: bake cache and GPU baking

**Concept.** A build step that always redoes everything gets slower as content grows. The standard answer is an **incremental build**: remember a **fingerprint** (a hash) of everything a result depends on, and redo the work only when the fingerprint changes. This is what `make` does with timestamps and what content pipelines do with hashes.

**AtomEngine.**
- Lightmap bakes are the expensive step: about 13 of the ~16 minutes of a full build go to the night street's four lightmaps, CPU path tracing at 192 samples.
- `atom_lightmap.fingerprint` hashes everything a bake depends on:
  - the mesh and its lightmap UVs, and the meshes around it (their geometry and render UVs);
  - their materials, every node input and every image's pixels;
  - the lights and the settings (size, samples, sky, clamp);
  - the device, Blender's version and the baker's own source code.
- Records live in `build/bake_cache/` (not committed; a fresh clone bakes once), one per lightmap and device. A bake is skipped only if the fingerprint matches **and** the PNG on disk is exactly the file the last bake wrote.
- A build that doesn't touch lit levels takes about 2 minutes instead of 16; changing one level's lights re-bakes only that level. `--no-cache` bakes everything.

### GPU baking and determinism
- `--gpu` bakes on the NVIDIA GPU (OptiX, else CUDA): all six lightmaps in under 3 minutes instead of about 16.
- The price is **reproducibility**. A GPU path tracer and a CPU one give the same lighting with a different noise pattern, and a GPU bake isn't byte-repeatable even on the same machine. In a measured comparison on the main street, 71 % of texels differed between CPU and GPU, and two GPU runs differed by one level in a few texels.
- So GPU bakes are for tuning light only. Their PNGs carry a text chunk (`atom-gpu-bake`), and a unit test refuses any such lightmap in `Assets/`: rebuild on the CPU before committing.

**Code.** `Tools/Blender/atom_lightmap.py` (`fingerprint`, `CACHE_DIR`, `USE_GPU`), `build_assets.py` (`--no-cache`, `--gpu`), `Tests/AuthoringTests.cpp`.

---

## 47. Documentation captures: the engine photographs itself

**Concept.** Grabbing the desktop to illustrate a program is fragile: another window gets in the way, the picture is cropped wrong, or a frame is caught half-drawn. It's better for the program to save its own frames. Scripting those shots also makes them repeatable, and so useful as tests.

**AtomEngine.**
- **`Renderer::RequestCapture(path, includeUi)`:** on the next frame, the post pass (and the UI, if asked) is drawn a second time into an offscreen texture of the swapchain's format.
  - A copy pass downloads it into a transfer buffer (`SDL_DownloadFromGPUTexture`).
  - After the frame's GPU fence, the pixels are swizzled from BGRA to RGBA and written with `stb_image_write`.
  - Frames without a request cost nothing extra.
- **Harness commands:**
  - `screenshot <stem>`, and `capture <stem> <count> <every>` for numbered sequences;
  - `pan`: an eased camera move (smoothstep, so it starts and stops without a jolt) that can film itself frame by frame;
  - `set <what> <value>`: every render switch (MSAA, fog, shadows, post, the field of view, the HUD…).
- **`fixed_dt`:** every frame advances exactly the same step, so sequences are evenly spaced and a rerun gives the same frames.
- **GIFs:** `Tools/Docs/make_gif.py` turns sequences into GIFs with no extra dependencies.
  - It reads the PNGs back itself; the engine writes unfiltered rows so that's fast.
  - It builds one 256-colour palette per GIF by **median cut**: split the colour box with the widest range until there are 256.
  - It maps pixels through a 32³ lookup cube and encodes with **LZW**.
- **The first-render scene** (`first_render`: a grid plane and spinning cubes) is the subject. `Tools/Docs/first_render.atomtest` shoots one topic per manual section, each as before/after pairs.

**Found on the way.**
- Object and action names are *global* in Blender. Markers now keep their real name in custom properties, and a bad marker fails the build.
- The bake fingerprint normalises line endings, so a checkout that converts them no longer forces a full re-bake.

**Code.** `Renderer::RequestCapture/RenderCapture/FinishCapture`, `Game/Testing/TestScript.cpp` (`screenshot`, `capture`, `pan`, `set`), `DemoApp::Set/Capture`, `Tools/Docs/`.

---

## 48. Input contexts and the machine mode (M29)

### Input contexts
**Concept.** Reading keys directly ("is W down?") ties gameplay to a keyboard layout and to one meaning per key. Games read **actions** ("move forward", "launch") instead. An **input context** says which keys give which actions in the current mode: Space confirms a line in a dialogue and fires a ball at a pachinko machine. One table holds every binding, so rebinding, gamepads or on-screen prompts would change only that table.

**AtomEngine.**
- `InputAction` names the actions; `InputContextId` names the modes (exploring, dialogue, machine); `InputMap::Default()` binds keys per mode.
- Each frame, `ActionInput::Update` computes *held* and *pressed* for the active context. Gameplay (the player controller, interaction, dialogue, the machine) reads only that.
- **Injection:** the harness holds and presses actions (`hold_action`, `press_action`) through the same path as keys, so scripts exercise the real input code.
- The mapping is a template over "is this key down?", so it's unit-tested without SDL.

### The machine mode
**Concept.** Moving from walking about in 3D to a 2D game in front of you is a **mode change**. It works best as an explicit state machine with timed transitions:

`Inactive → Entering → Playing → Leaving → Inactive`

**AtomEngine** (`MachineMode`):
- **Entering:** eases the camera (smoothstep) from the player's eye to a pose in front of the machine's screen. Yaw turns the *short way round*, since a turn from −170° to 170° is 20°, not 340°. Then it fades the 2D view in.
- **Leaving:** fades out first, then moves back.
- **Pure:** no rendering or input inside; `DemoApp` reads the camera pose and the fade, and unit tests check the timings.

**Integer scaling.** Pixel art stays crisp only at whole-number scales: every source pixel becomes an equal square. `FitIntegerScale` picks the largest scale that fits (320×240 is 3× in 1280×720) and centres it; the rest is border. `UIRenderer::DrawImage` draws the machine's render texture, sampled with *nearest* filtering for pixel-art textures.

**The screen.** The playable machine has its own screen material. `Level::TakeOverScreen` pauses that screen's attract loop and hands its render texture to the game. The in-world screen and the fullscreen view are the same texture, so zooming in and out is seamless.

**Code.** `Game/Input/InputContext.*`, `Game/Pachinko/MachineMode.*`, `DemoApp::BeginMachine/UpdateMachine/DrawMachineView`, `Level::TakeOverScreen`, `UIRenderer::DrawImage`, `Tests/MachineModeTests.cpp`.

---

## 49. 2D physics (M30)

**Concept.** A pachinko field is rigid-body physics in two dimensions.
- **Shapes:** dynamic circles (balls) against static circles (nails) and segments (walls, rails).
- **Each step:** integrate velocity and position, then detect overlaps and resolve them. Push the ball out along the contact **normal**, remove the velocity going into the surface, and give back a share of it: the **restitution**, 1 for a perfect bounce, 0 for none. **Friction** takes away a share of the sliding speed.
- **Balls against balls:** equal masses exchange the part of their velocities along the line between their centres, so momentum is kept.

**Pitfalls, and how they're handled.**
- **Tunnelling:** a fast ball can jump over a thin nail between two steps. Speeds are capped (480 px/s), and each substep is 1/480 s, so a ball never moves more than a third of its radius per substep.
- **Jitter at rest:** gravity makes a resting ball approach its rail slightly every substep, and bouncing that back makes it shiver. Below a **rest threshold** (about 3 substeps of gravity) there's no bounce.
- **Friction per contact, not per second:** a ball riding a rail touches it every substep, 480 times a second. The first friction value (2 % per contact) stopped balls dead halfway up the launch lane; it's now 0.05 % per contact, about 20 % per second.
- **Many shapes:** a **uniform grid** broad phase, as for the level's collision (§40). Each shape is listed in every cell it overlaps, and a per-query mark tests each shape once.

**Determinism.** The same inputs give the same bits, because of:
- a fixed iteration order;
- fixed substeps;
- no dependence on frame time (the game steps it from `FixedStep`, §45);
- `float` maths in one build.

A unit test runs 30 balls through 96 nails for 10 seconds twice and compares memory byte for byte.

**Impacts** faster than a threshold are reported with their speed and position, for the sounds (§51).

**Code.** `Game/Pachinko/Physics2D.*`, `PhysicsDebugDraw.*`, `Tests/Physics2DTests.cpp`.

---

## 50. Playfields as data, and tuning by simulation (M31)

**Concept.** A machine's layout is content, not code. `Assets/Machines/<name>.json`, with a schema, lists:
- walls, curved rails and nail rows;
- the launcher: position, direction, speed range, rate and jitter;
- pockets (start, side, attacker, out, foul);
- the attacker's gate;
- the rules (§51).

The loader validates it: every shape on the board, no touching nails, a start pocket and an out hole, and errors given as JSON Pointers as for levels. `Tools/Machines/night_fever_layout.py` computes the shipped layout, for example nail rows as chords of the board's circle with gaps for the reels and the pockets.

**The machine** (`PachinkoGame`, one 1/60 s tick at a time):
- **Launching:** holding the launcher fires about 1.7 balls a second. The knob (0..1) sets the speed between the minimum and maximum, with a little seeded jitter.
- **Pockets:** they catch balls and pay into the tray. A ball too weak to clear the lane falls back into the **foul** pocket and is returned, as on real machines.
- **The gate** is a wall that can be switched off (`World2D::SetSegmentEnabled`).

**Tuning by simulation.** Watching is slow and anecdotal. A unit test instead fires 200 balls at five knob settings and counts where each ends up; that it *ends up somewhere* is itself the assertion. The numbers drove the design:
- **No ball may come to rest.** The reel frame got a pitched roof, since its flat top held balls.
- **Road nails,** a slanted row that steers balls toward the start pocket, must be a little *wider* apart than a ball. Closer, they cradled balls between them, and every ball that landed on them reached the pocket.
- **The start pocket** catches roughly 5–12 % of balls, and outside a fever the machine pays back about what it takes.

**Code.** `Game/Pachinko/Playfield.*`, `PachinkoGame.*`, `PixelDraw.h`, `Assets/Machines/`, `Assets/Schemas/machine.schema.json`, `Tools/Machines/`, `Tests/PlayfieldTests.cpp`.

---

## 51. Rules as a state machine: the lottery and the fever (M32)

**Concept.** A classic pachinko machine's rules are a small state machine:

`Idle → Spinning (maybe a reach) → Result → Idle`, or on a hit, `→ Fever: Round (gate open) → Interval → Round … → Idle`

- Balls into the start pocket **hold** spins, up to 4; past that the ball still pays, but its spin is lost.
- **A round** ends after N balls into the attacker, or after T seconds.

**Randomness you can replay.** Each spin's outcome is drawn from a **seeded** generator (xorshift) *when the spin starts*; the reels only show what was drawn.
- A **hit** shows three of a kind, always through a **reach**: the first two match and the last reel hangs.
- Some misses tease a reach too, stopping one short.
- **Replays:** with the same seed and the same inputs, a whole session replays exactly, which is how its unit test works. Games use the same idea for replays, networked lockstep and bug reports.

**Checked statistically.** Over 100,000 draws at 1 in 99, the hit count must lie within about four standard deviations of 1010. An exact number would be wrong, and so would no check at all.

**Pure and event-driven.** `PachinkoRules` takes pocket events in and gives back the gate state and its own events (spin, reel stop, reach, hit, round start and end). The game drives the gate from it, `DemoApp` turns its events into sounds, and its tests need no physics.

**Around it.**
- **Sounds, synthesised:** ball clicks (only the loudest few impacts per tick), the start chime, the payout rattle, reel stops, the reach and the fanfare. The hall's ambience **ducks** while you play (a gain on the level's `bed` voices).
- **Attract screens:** the hall's other screens run the *real* game with a demo input, a player who never tires.

**Code.** `Game/Pachinko/PachinkoRules.*`, `PachinkoGame::Step/DrawReels`, `DemoApp::PlayMachineSounds`, `SoundSynth.cpp` (the machine's sounds), `Tests/PachinkoRulesTests.cpp`.

---

## 52. Counters and the economy (M33)

**Concept.** Flags answer yes or no ("was the letter read?"). An economy needs **counters**: how many tokens, how many balls. Both are **game state** that outlives any level, owned by the game rather than by a level (§31).

**AtomEngine.**
- **`GameState` counters:** missing ones read 0; they never go negative; `Spend` takes an amount only if all of it is there.
- **Two data-driven actions:**
  - `addCounter`, optionally only once: a flag remembers, and a second line is shown next time;
  - `exchange`: spend a counter for a flag, or say what's missing.
- **The loop:**
  - the attendant's welcome tokens;
  - at the machine, 10 tokens buy 50 balls;
  - balls stay in your tray between sessions;
  - 300 balls buy the ofuda from the prize shelf, a flag tied to the city's thread.
- **Harness:** `expect_counter` and `set_counter`. The `pachinko_session` scenario plays the whole loop: tokens, buying, playing, standing up, the exchange refused, then made.

**Code.** `Game/World/GameState.h`, `Interaction/Actions.h` (`AddCounter`, `Exchange`), `ActionExecutor.cpp`, `Assets/Levels/pachinko_hall.json`, `Tests/Scenarios/pachinko_session.atomtest`.

---

## 53. Skeletal skinning (M35)

**Concept.** Rigid animation (§38) moves whole parts. A character has to *bend*: an elbow, a knee, the cloth over a shoulder. **Skinning** does it with a **skeleton**, a hierarchy of **joints** (glTF nodes), and per-vertex **weights**: each vertex follows up to four joints, blended by how much it belongs to each.

- **Bind pose:** the pose the mesh was modelled and attached to the skeleton in.
- **Inverse bind matrix:** one per joint. It takes a vertex from model space into that joint's own space as it stood in the bind pose. Pose the joint, multiply back out, and the vertex moves with it.
- **Palette matrix:** `jointWorld × inverseBind`. In the bind pose it is the identity, and the mesh stands as modelled.
- **Linear blend skinning:** `skinned = Σ weightᵢ × paletteᵢ × position`. The matrices are blended, then applied once. Its known flaw: blending rotations as matrices shrinks volume at sharp twists (the "candy wrapper"). Fixing that takes dual quaternions; it's not needed here.

**AtomEngine.**
- **Loading:** cgltf gives the skins (joint lists, inverse binds) and the attributes `JOINTS_0` (u8 or u16) and `WEIGHTS_0`. Weights are **renormalised**: exporters store them as floats or normalised integers, and a sum drifting from 1 shrinks the vertex toward the origin. A skin over 64 joints is refused.
- **A second vertex stream:** `SkinVertex` (4 joint indices, 4 weights) lives in its own buffer next to `Vertex`. Static meshes are untouched; skinned draws bind both streams.
- **Pipelines:** skinned variants of the scene, decal and shadow pipelines use `Skinned.vert` / `ShadowSkinned.vert` (`Skinning.hlsli`) with the same fragment shaders. The palette is a vertex uniform (slot 2), pushed per draw.
- **Per frame:** `Model::Submit` with a **pose** (one local `T·R·S` per node) composes world matrices (`ComputeWorldMatrices`, resolving parents on demand), computes one palette per skin (`ComputePalette`), stores it once (`Renderer::AddPalette`) and submits every part with its handle (`SubmitSkinned`). A skinned mesh ignores its node's transform: the joints place it.
- **Culling:** a running character leaves its bind-pose box. At load, `ComputeSkinnedBounds` skins the mesh through every clip (16 samples each, plus a 5 % margin) and widens the mesh's box to fit.
- **Entities** get a uniform `scale` (Rudy at 0.8, about 1.7 m).

**Tests.** The skin loads with 22 joints and weights summing to 1; the bind-pose palette is the identity; one vertex skinned on the CPU (`SkinPoint`) matches a hand computation; the bounds hold every vertex at times off the sampling grid.

**Code.** `Engine/Assets/Skin.*`, `Model::Load/Submit/SamplePose`, `Mesh` (skin stream), `Renderer::AddPalette/SubmitSkinned`, `Shaders/Skinned.vert.hlsl`, `ShadowSkinned.vert.hlsl`, `Skinning.hlsli`, `Tests/SkinningTests.cpp`.

---

## 54. The character lab: a viewer and debug views (M36)

**Concept.** Animation is hard to judge in a game: the camera moves, the character moves, things happen fast. Tools of the 2000s (and engines' animation editors since) give it a **viewer**: the model alone on a turntable, an orbit camera, clips to pick and step through, and **debug views** that show the machinery instead of the result.

**AtomEngine.**
- **The level:** `character_lab` (`Tools/Blender/atom_lab.py`): a grid floor, a **cyclorama** (a backdrop curving smoothly up from the floor) whose colour matches the fog, so the open sides dissolve, and a checkered turntable. A level with a `lab` section (subject entity, orbit) opens in the viewer.
- **`LabViewer`, pure state:**
  - an orbit camera: yaw, pitch (clamped), distance (zoom by a fixed share per notch, so it feels the same near and far);
  - clips, speed steps, pause and a one-frame step (which pauses: stepping is for looking closely);
  - toggles for the bind pose, the skeleton and the weights.

  Keys come in as a `ViewerInput`; the camera and the pose come out. Unit-tested without a window.
- **The viewer owns the clock:** the subject's clip and time are written each frame, and the level draws what it's told.
- **Skeleton overlay:** each joint's origin is projected to the screen and drawn in the UI layer, joined to its nearest ancestor that is also a joint. It's drawn over everything, so the bones show through the body.
- **Weights view:** each joint gets a colour (golden-ratio hues), and each vertex the blend of its joints' colours by weight. Gradients mark joints sharing vertices (shoulders, neck); hard edges mark parts following one joint. Skinned meshes carry no baked light, so `Skinned.vert` writes this colour into the vertex-colour channel, and a material flag makes `Basic.frag` show it.
- **Input is by position:** bindings are scancodes, physical key positions. `[` `]` on a US keyboard are `` ` `` `+` on a Spanish one, so the blend slider also sits on Z/X, which are in the same place on every layout.

**Code.** `Game/Character/LabViewer.*`, `Game/DemoAppLab.cpp`, `Level::EntityModelTransform`, `LevelLab` in `LevelData`, `Tools/Blender/atom_lab.py`, `Tests/LabViewerTests.cpp`.

---

## 55. Pose blending and the animation state machine (M37)

### Blending poses
**Concept.** A character rarely plays one clip: it eases from standing into walking, and walks faster into a run. Both are **blends** of poses, joint by joint:
- translation and scale: a straight `lerp`;
- rotation: **slerp** (§38). A quaternion `q` and `-q` are the same rotation, so slerp first takes whichever is nearer. Otherwise a 20° blend can swing 200° the long way.

More than two poses blend as a running weighted average: each new pose comes in by its share of the weight seen so far.

**Crossfade:** when a clip changes, the old one keeps playing while its weight falls to 0 over a blend time (0.3 s in the viewer). No pop.

### Walk and run in phase
**Concept.** Walk (1.1 s per stride) and Run (0.7 s) blended *by time* tangle their legs: at 0.5 s one has the left foot forward, the other the right. Blend them by **phase** instead, the fraction of a stride:

```
walk time = phase × 1.1      run time = phase × 0.7
cycle     = 1.1 + (0.7 − 1.1) × weight      phase += dt / cycle
```

Both clips are always at the same point of the stride, so the feet agree; the blended stride lasts in between (0.9 s at 50 %). Games call these **sync groups**.

### The animation state machine
**Concept.** What plays when is gameplay logic, best written as data: **states** (a clip, or a blend driven by a parameter) and **transitions** with conditions on **parameters** and a crossfade time. Gameplay only sets parameters (`speed`, `grounded`); the animator decides.

```json
"move": { "blend": ["Walk", "Run"], "param": "speed", "range": [1.4, 4.0] },
{ "from": "*", "to": "jump", "when": ["grounded == 0"], "blend": 0.1 }
```

- Transitions are checked in order, and the first that matches wins. `"*"` means from any state but the target.
- A **one-shot** (`"loop": false`) returns to its `next` state by itself.
- Unset parameters read 0, so `grounded` must be set before anything moves. A test learned that the hard way: a forgotten `grounded` fired the jump.

**AtomEngine.**
- `BlendPoses` and `Model::SamplePose(samples)` blend any weighted clips.
- `Animator` binds a model's clips by name once (errors name what's missing) and outputs `ClipSample`s (clip, time, weight) for the level to draw.
- **Viewer:** 5 shows the walk/run blend on a slider; 6 runs the state machine from a 14 s demo script (stand, speed up, run, jump, slow down).
- **Harness:** `set_param`, `expect_state`.

**Tests.** Blend endpoints are the sources; slerp takes the short way; the state machine follows scripted parameters, and its weights always sum to 1; both blended clips stay at the same phase.

**Code.** `Game/Character/Animator.*`, `BlendPoses` in `Skin.*`, `Model::SamplePose(std::span<const ClipSample>)`, `Tests/AnimatorTests.cpp`.

---

## 56. A third-person character: drive mode (M38)

**Concept.** Third-person control has three parts:
- **Movement relative to the camera:** W means "away from the camera", whatever way the character faces. It turns to face its movement, smoothly and the short way round.
- **A body with physics:** walls, steps, gravity, a jump.
- **A camera on a spring arm:** behind and above the shoulders, swung by the player. A ray from the pivot toward the camera finds walls in between. The arm **pulls in at once** (never show the inside of a wall) and **eases back out** (no jump when the wall is gone).

**Animation follows the body, not the keys.** The animator reads the body's actual speed and whether it stands. Walking into a wall plays idle, and walking off a ledge plays the fall.

**Root motion vs in place.** The Jump clip lifts the hips about 0.55 m by itself (its **root motion**). If physics also lifts the body, the jump is doubled. The choice is who moves the character: the animation, or physics. Here physics jumps (it lands on steps and platforms), and the clip plays **in place**: an `inPlace` state pins the hips to their rest position, so the clip keeps only the arms' and legs' motion. Landing ends the jump early.

**Animation events.** Moments of a clip, written as data, such as a foot touching down. The foot-down times were measured from the clips by sampling the foot joints' height (Walk at 41 % and 89 % of a stride, Run at 47 % and 94 %, close thanks to the phase sync). An event fires when the playback crosses it, wraps included, and only from the state playing, not one fading out. Each "foot" bumps the step counter the footstep audio already listens to.

**AtomEngine.**
- `PlayerController::Move`: the first-person body split from its camera, now with a jump (and no snapping back to the floor while rising). Rudy uses a second one with his own size and speeds; the player's code path is unchanged.
- `SpringArm`: a pure class with a raycast callback, unit-tested.
- Tab switches viewer ↔ drive in the mode switch, once per press. Handling it inside the entered mode's update made one press switch twice in the same frame.
- **The lab:** steps (0.15 m each) to a platform, a ramp (a rotated collider; colliders may now carry a rotation), crates, and a wall to back the camera into. The asset lint caught the ramp sharing a plane with the platform's sides, and a 2 cm inset fixed it.
- **Scenario `character_lab`:** drive, walk, run, jump, climb the steps and the ramp, Tab back.

**Code.** `Game/Character/SpringArm.*`, `PlayerController::Move/Place`, `DemoApp::BeginDrive/UpdateDrive/EndDrive`, `Animator` (events, `inPlace`), `Tests/DriveTests.cpp`, `Tests/Scenarios/character_lab.atomtest`.

---

## 57. Releasing the lab: captures, and a regression found by measuring (M39–M40)

**Captures.** `Tools/Docs/character_lab.atomtest` directs the lab like first-render (§47), at a fixed 1/30 s step:
- bind pose vs posed;
- skeleton and weights, as stills and turntable orbits;
- every clip;
- a crossfade at quarter speed;
- the blend swept from walk to run;
- the state machine's demo;
- driving up the steps;
- the spring arm at the wall.

`capture_character_lab.ps1` makes the GIFs into `out/img/character_lab/`.

**A regression, found by comparing.** Release frame times looked a little high, but this laptop drifts ±10 % between runs, so one number proves nothing. The fair test is **A/B**:
- build the previous release (a git worktree at `master`);
- add the same timing to both;
- alternate the runs.

The lightmapped levels were 10 % slower (the interior 2.30 against 2.06 ms; the night street +0.5 ms); the plain first-render scene was not.

**Bisecting by hypothesis.** The cost grew with lit, lightmapped scenes, which points at the GPU and per-pixel work. The only per-pixel change was the weights view: an `if` at the top of `Basic.frag` replacing the base colour. Removing it alone gave the time back (2.10 ms). Rewritten as a branch-free `lerp` driven by the same flag, the interior measured 2.07 ms.

Why would a uniform branch cost anything? On paper it shouldn't: every pixel takes the same way. In practice the compiler and the driver may lay the shader out differently around it (registers, scheduling), and this GPU paid for it. The lesson isn't "branches are slow". It's: **measure against the last release, change one thing, measure again.**

**Code.** `Tools/Docs/character_lab.atomtest`, `capture_character_lab.ps1`, `Shaders/Basic.frag.hlsl` (the debug blend).

---

## 58. Developer tools: Dear ImGui (M41)

**Concept.** Tuning a light by editing JSON, restarting and looking again is slow. A **developer UI** lets you drag a slider and see the result at once. **Dear ImGui** is the standard choice. It's an **immediate-mode** UI: every frame the code says "a window, a slider bound to this float". ImGui keeps no widget tree of yours, so there is nothing to synchronise with the game's state. The widget *is* the variable, for one frame.

```cpp
if (ImGui::Begin("Lighting"))
{
    ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 50.0f);
}
ImGui::End(); // always, whatever Begin() returned
```

**The one rule that bit us.** `Begin()` returns false when the window is collapsed, but `End()` must still be called. Putting `End()` inside the `if` crashed as soon as a panel was collapsed ("Missing End()"). Tables are the opposite: `EndTable()` only if `BeginTable()` returned true. A scenario (`set devtools_collapsed`) now collapses every panel.

**AtomEngine.**
- **Pinned and plain:** Dear ImGui 1.92.9 is a git submodule, built as a static library with its SDL3 platform backend and SDL_GPU renderer backend. ImGui ships no CMake of its own.
- **F10** toggles the panels:
  - **Frame:** a frame-time graph, draws, binds, layers, lit draws;
  - **Render:** the same switches as the harness's `set`;
  - **Lighting:** the level's sun, ambient and fog, edited live;
  - **Spot light:** the flashlight's cone, range, intensity and lag;
  - **Level:** entities, flags and counters.
- **Copy as JSON:** a panel puts its values on the clipboard in exactly the shape the data file expects. You tune live, then paste the result into the file.
- **A separate overlay pass:** ImGui is drawn after everything else, into the swapchain, and screenshots and captures are taken before it. Dev tools never show up in documentation images.
- **Input:** while ImGui wants the mouse or keyboard, the game doesn't receive them.

**Code.** `Engine/Debug/DevTools.*`, `Game/DemoAppDevTools.cpp`, `Renderer::SetOverlayPass`, `Tests/Scenarios/devtools.atomtest`.

---

## 59. Spot lights and specular highlights (M42)

**Concept.** A **spot light** is a point light restricted to a cone. Four things decide how much it lights a point:
- **The cone:** the angle from the spot's axis. Inside the **inner** angle the light is full, outside the **outer** angle it is zero, and between them it fades with a smoothstep. A soft edge looks like a real lamp; a hard one looks like a stencil.
  ```
  cone = smoothstep(cos(outer), cos(inner), dot(-L, spotDir))
  ```
- **Distance falloff:** physically, light falls as 1/d². Pure 1/d² never reaches zero, so a light would touch the whole level. The **windowed** version reaches exactly zero at the range and lets the engine skip everything beyond it:
  ```
  falloff = saturate(1 - (d/r)^4)^2 / (d^2 + 1)
  ```
  The `+1` stops it exploding at d = 0.
- **Diffuse (Lambert):** `max(dot(N, L), 0)`, as for the sun.
- **Specular (Blinn-Phong):** the shiny highlight. Take the **half vector** H = normalize(L + V), halfway between the light and the eye. The highlight is `pow(dot(N, H), shininess)`; a higher exponent gives a smaller, sharper spot. The factor `(n + 8) / 8π`, used here without the π and folded into the strength, keeps the energy roughly constant: a sharp highlight is also brighter.

**Roughness → shininess.** glTF materials carry a **roughness** (0 = mirror, 1 = chalk), which the loader now reads. It maps to an exponent: rough stone barely gleams, a wet jar or a metal hinge catches a crisp spot. An `atom_specular` extra in Blender scales the strength per material, for things that should stay matte whatever their roughness says.

**The same maths twice.** `SpotMath` in C++ implements the cone, the falloff and the shininess exactly as the shader does. The unit tests check the edges (inner, outer, range), and gameplay uses it (§61) to ask "is this lit?" without reading the GPU.

**AtomEngine.** One spot light per frame (`Renderer::SubmitSpotLight`): position, direction, range (15 m), inner and outer angles (12° and 24°), colour, intensity and specular. The baked world remains almost all of the lighting; the spot is added on top in `Basic.frag`.

**Code.** `Engine/Renderer/SpotLight.h` (`SpotLight`, `SpotMath`), `Shaders/Basic.frag.hlsl` (`SpotLighting`), `Shaders/Common.hlsli`, `Tests/SpotLightTests.cpp`.

---

## 60. The spot shadow map (M43)

**Concept.** The sun's shadow map (§23) is **orthographic**: the sun is so far away that its rays are parallel. A spot light is a point, and its rays spread out, so its shadow map is a **perspective** render from the lamp. Its field of view is the cone's outer angle and its far plane is the range. Everything else is the same idea: render depth from the light, and later ask each pixel "was something closer to the light than me?"

**What changes with perspective.**
- **Texel size grows with distance.** One shadow texel covers a few millimetres at 1 m and several centimetres at 10 m. A fixed bias that's right near the lamp is too little far away (acne), and one right far away is too much near (peter-panning). The **normal offset therefore grows with distance**, per metre.
- **Depth isn't linear.** Perspective depth crowds its precision near the lamp. A near plane that isn't too small (0.1 m) keeps that tolerable.
- **No texel snapping.** The lamp moves with the player's hand, so there is nothing to keep stable. The lag and sway hide its motion instead (§61).

**AtomEngine.**
- **Shadow map:** 1024², 32-bit depth.
- **Drawing:** `SpotMath::ViewProjection` builds the light's matrix. The sun's shadow pipelines (plain, alpha-tested, skinned) are reused, and draws are culled to the spot's frustum.
- **Filtering:** 3×3 PCF with a comparison sampler.
- **Skipping:** the taps run only where the spot reaches at all, behind a `[branch]`. Pixels outside the cone don't read the map.
- **Opt out:** `castsShadows` lets a light skip the whole pass.

**Code.** `Renderer::RenderSpotShadowPass`, `m_spotShadowMap`, `Basic.frag.hlsl` (`ComputeSpotShadow`, t4/s4), `FrameStats::spotShadowDrawn`.

---

## 61. The flashlight: light as gameplay (M44)

**Concept.** A dynamic light becomes **gameplay** when the player controls it and the world answers to it: where you point the light decides what you can see and what you can do.

**Holding it.** A flashlight fixed to the camera looks fake: the beam's centre never moves on screen. This one is held low and to the right, and follows the view **a moment late**: its direction eases toward the camera's with a short lag, plus a slight sway while walking. The beam drifts across the scene as you turn, which also hides the shadow map's motion (§60).

**What only the light shows.**
- **Reveal materials** (`atom_reveal`): decals such as chalk marks whose alpha is multiplied by how much the spot reaches that pixel (`SpotReach`). They are invisible in baked light and appear only in the beam.
- **Lit-only interactables** (`requiresLight`): the interaction system asks `isLit` before offering a prompt. The answer comes from the CPU copy of the spot's maths (§59) at the entity's focus point, so the GPU is never read back.
- **Things that stay gone** (`goneWithFlag`): once a flag is set, the entity is no longer spawned, in any visit. The flashlight itself is gone once picked up.
- **Locked prompts** (`lockedPrompt`): what an interactable says while its condition isn't met ("The bolt is on the other side."), resolved by `ResolvePrompt`.

**AtomEngine.** The flashlight is found on the machiya's entry step (`SetOwned`). F toggles it in any level. `Flashlight::Update` applies the lag and sway, and `Lights` produces the `SpotLight` for the frame. Its settings live in `Assets/Data/flashlight.json` with a schema (§63).

**Code.** `Game/Flashlight.*`, `InteractionSystem::Settings::isLit`, `Tests/Scenarios/flashlight.atomtest`.

---

## 62. The passage: dust, a beam, and darkness that isn't black (M45)

**Concept: baked darkness.** A dark level isn't black. Pure black reads as "nothing is drawn" and hides the level's shape. The passage is **baked** from its own small lights:
- a candle in the cellar;
- a battery lantern;
- an old green exit lamp in the ladder chamber.

These give pools of dim light and a readable silhouette. The flashlight adds detail and colour on top. Emissive materials (the exit sign, §41) glow without lighting anything.

**Dust in the beam.** Air is full of dust you only see in a beam. The passage's particles are marked `beamLit`, and the particle shader multiplies their alpha by `SpotReach` at their position. Outside the cone they vanish; inside, they drift through the light. `SpotReach` lives in `Common.hlsli`, so the scene, the decals and the particles share one definition.

**A faked visible beam.** Real volumetric light means marching rays through fog per pixel. That is expensive, and overkill for one lamp. The beam is instead **three crossed planes** along the spot's axis, textured with a soft gradient and drawn with **additive** blending: it only adds light, so the order doesn't matter. Two corrections make it hold up:
- the planes **fade near the camera**, so you never see their edges from inside the beam;
- each pixel is scaled by the spot's reach, so walls cut the beam where the light stops.

Each level chooses whether its air shows it (`particles.beam`).

**The level.** The passage is a cellar under the machiya and a tunnel up to the windmill field's shed:
- **The cellar:** jars and the lantern.
- **The tunnel:** timber-shored, with a fork.
- **The fork:** chalk arrows that only the beam reveals.
- **The ladder chamber:** the trapdoor's bolt, found only in the beam.

Unbolting the trapdoor sets `hatch_unbolted`. Until then, the shed in the windmill field is locked from above (`lockedPrompt`); afterwards it is the way down.

**Code.** `Tools/Blender/atom_passage.py`, `Assets/Levels/passage.json`, `Shaders/Beam.*`, `Shaders/Particle.*` (`beamLit`), `Renderer::DrawBeam`, `Tests/Scenarios/passage.atomtest`.

---

## 63. Light culling and measuring performance honestly (M46)

### Light culling

**Concept.** Every lit draw used to loop over all the live point lights and evaluate the spot, even when they were nowhere near it. **Light culling** decides per draw which lights can touch it, and the shader skips the rest.
- **Point lights:** a sphere (position and range) against the draw's box (`SphereTouchesBox`). The answer is a **bitmask** per draw, one bit per light, in `u_lights.x`.
- **The spot:** a flag (`u_lights.y`) set when the draw's box is in the spot's frustum.

The shader branches on these values. They are the same for every pixel of a draw, so all threads take the same path and the branch costs almost nothing (compare §57, where a branch did cost something: always measure).

### Settings as data

The flashlight's cone, intensity, lag and sway moved into `Assets/Data/flashlight.json`. It has a schema, reloads while running (§39), and the dev tools' Copy as JSON writes exactly that file.

### Measuring performance honestly

**The problem.** A laptop's speed isn't constant. Two things happened during this release:
- **Unplugged:** on battery, everything ran about 3× slower.
- **Thermal throttling:** after a while under load, frame times climbed within one run, whatever was switched on.

With drift that size, comparing two runs measured minutes apart answers nothing about a 0.3 ms change. Other things distort the numbers too: a 60 Hz monitor capped frame times until the measurements moved to the laptop's 144 Hz screen.

**The method.**
- **Medians, not means:** one hitch (a shader compile, a file read) moves the mean, not the median. The **p95**, the 95th percentile, shows how bad the slow frames get.
- **Compare close together, in pairs:** run A, then B, then B, then A (**ABBA**), repeating. Each round's **paired difference** (B − A) is one sample, and the result is their median. Drift that slows the machine over a round hits both halves almost equally. Alternating the order cancels what's left: whichever half runs second looks slower, once one way and once the other.
- **Settle before measuring:** after switching a setting, wait (0.5 s by default) before timing. Changing MSAA rebuilds render targets, and that transition is not the steady-state cost.
- **Engine warm-up isn't thermal warm-up:** the first 300 frames, which compile pipelines and fill caches, are dropped. Heat is handled by the interleaving, not by waiting.
- **Trust the tool first:** measure a build against itself. The difference should be ~0 within its spread. That spread (~0.15 ms here) is the smallest difference the tool can resolve.

**The tools.**

| Question | Tool |
|---|---|
| What does this feature cost? | the harness's `bench <setting> <a> <b> <rounds> <seconds> [settle]`, in-process |
| Did this build get slower? | `Tools/Perf/ab.ps1 -A <exe> -B <exe> -Level <name>`, alternating executables |
| How fast is this level now? | `ATOM_PERF_LOG=1`: median, p95 and mean every 240 frames (`ATOM_PERF_CSV` writes them to a file) |
| *Why* is it slow? | a GPU profiler (PIX, Intel GPA); SDL_GPU has no GPU timers |

The in-process `bench` is the tighter tool: no restart and no reload, with the halves seconds apart. `expect_bench_under` exists for local checks on known hardware but is never part of ctest. **Correctness belongs in CI; performance belongs in controlled benchmarks**, because shared CI machines time things too noisily to gate on.

**What it found on the night street:**
- **Particles:** +0.28 ms, consistent across runs. That's real, but small for 200 billboards, so optimising them wasn't justified.
- **The flashlight:** +0.23 to +0.77 ms. It lights little of the street and the view moves, so it was not stable.
- **The spot shadow pass:** indistinguishable from zero.
- **MSAA 4× vs 1×:** about +1.1 ms, used as a known cost to check that the tool works.

**A bug found on the way.** Since v0.0.4, the particle pipeline cache had stored every particle pipeline in the halo pipeline's slot. Two consequences:
- leaves, ash and fog banks were blended like halos;
- a new pipeline was created, and leaked, every frame.

Looking at what a draw actually binds, not just how long it takes, found it.

**Code.** `Engine/Core/FrameStatsWindow.*`, `Game/Testing/PairedBench.*`, `TestScript` (`bench`, `expect_bench_under`), `DemoApp` (`PerfLog`), `Tools/Perf/ab.ps1`, `Tools/Perf/lights_and_particles.atomtest`, `Tests/FrameStatsTests.cpp`, `Tests/PairedBenchTests.cpp`.

---

## 64. A day sky and the environment state (M47)

**Concept: authored colour, not a simulated atmosphere.** A physically based sky (Rayleigh and Mie scattering) computes colour from sun angle and air. Early-2000s fantasy games painted it instead: a colour overhead (**zenith**), a colour at eye level (**horizon**), and a blend between them. It's cheap, it's art-directable, and changing two colours changes the whole mood.

**The gradient.**
```hlsl
color = lerp(horizon, zenith, smoothstep(0.0, 0.65, max(dir.y, 0)));
```
`smoothstep` starts slowly, so the horizon colour holds a wide band low down and the zenith colour arrives higher up. Below the horizon the colour stays the horizon's. The fog uses the same colour, so distant ground dissolves into the sky with no visible edge.

**The sun** is two terms added on top:
- **The disc:** where the view direction is within the sun's radius, `smoothstep(cos(radius), …, dot(dir, sunDir))` gives a soft-edged disc. Its colour is about 12× the sun's, so in the HDR target it exceeds the glow threshold and blooms (§41).
- **The halo:** `pow(dot(dir, sunDir), n)` for a broad glow (n = 6) and a tighter one (n = 64). These are the same "raise a cosine to a power" falloffs as a specular highlight (§59).

**Environment state.** The part of a level's light that weather and time of day change became one plain struct, `EnvironmentState`: sun direction and colour, ambient sky and ground, fog colour and density, the sky gradient, and later water, rain and wind. `LevelLighting` now *inherits* from it and adds what stays the level's own (shadows, bake weight, glow). Inheritance means all existing code that reads `lighting.sunColor` kept working, and passing a `LevelLighting` where an `EnvironmentState` is expected copies just that part (**slicing**, which here is exactly what's wanted).

**Blending.** `Blend(a, b, t)` mixes two states:
- **Numbers and colours:** linear, `mix(a, b, t)`.
- **The sun's direction:** mixed, then normalized back to length 1. Halfway between noon and the horizon is still a valid direction. Opposite directions have no middle, so they switch.
- **Optional parts** (no sky on one side, or no fog density): switch at t = 0.5.

**Fog setting "level".** F5's fog settings gained a default, "level", which uses the level's own `fogDensity`. Existing levels have none, so they look exactly as before.

**Code.** `Game/Level/Environment.*`, `Shaders/SkyGradient.hlsli`, `Shaders/Sky.frag.hlsl` (gradient or panorama, a branch per draw), `Tests/EnvironmentTests.cpp`.

---

## 65. Stylized water (M48)

**Concept.** Water that reads through **colour, movement, silhouette and highlights** rather than optics. There's no refraction, no rendered reflection (that came as an option in §68) and no simulation. The plan was a short recipe:

```
body colour (shallow → deep)   lit by sky and sun
+ sky colour, by Fresnel       along the rippled reflection
+ the sun's glint              on the ripples
+ foam                         where it meets the shore
```

**Depth without a depth buffer.** Water usually fades with how much water is under each pixel, which means reading the scene's depth. With MSAA that depth is multisampled and awkward to sample. Instead the depth is **baked into the mesh**: Blender computes the lake bed's height under each water vertex from the same height function that builds the ground, and stores `depth / maxDepth` in the vertex's first UV. The shader reads it as an ordinary interpolated value. It's free at runtime and exact for a static lake.

**Ripples from noise.** The surface height is two layers of value noise drifting in different directions (a slow swell and smaller wind ripples), plus a fine layer for sparkle. The shader never moves vertices: it only needs the **slopes** to tilt the normal. They come from **central differences**, sampling the height a little to each side, `dh/dx ≈ (h(x+e) − h(x−e)) / 2e`. Noise is computed from world position, so neighbouring water meshes ripple as one sheet.

**Fresnel.** Water, like glass, reflects more at grazing angles: about 2% looking straight down, nearly all of it skimming the surface. **Schlick's approximation**:
```hlsl
fresnel = 0.02 + 0.98 * pow(1 - dot(n, toEye), 5);
```
The reflected colour is the **sky gradient along the reflected ray**, the same function as §64's sky, shared in `SkyGradient.hlsli`. So the water shows the real sky, ripples included, with no extra render. The reflected sun is capped so the separate glint draws the highlight.

**Glint, foam and alpha.**
- **Glint:** Blinn-Phong with a very high exponent (700) on the rippled normal, a hot sparkle, plus a broad sheen (90).
- **Foam:** a band where the baked depth is small, broken and drifting with noise.
- **Alpha:** clear in the shallows (you see the sand), nearly opaque in the deep, and zero exactly at the waterline, so the shore has no hard edge. Fresnel and foam raise it: what water reflects isn't see-through.

**Drawing it.** A material tagged `atom_water` in Blender gets its own pipeline:
- blended like a decal, with depth testing but no depth writes;
- no decal depth bias;
- `Water.frag` as the fragment shader.

It's sorted after the decals, which may lie on what the water covers.

**The lakeshore lab.** A lab level (`ATOM_START_LEVEL=lakeshore`), built like the character lab and outside the demo:
- a round lake, a sandy shelf, reeds, rocks and trees;
- low hills;
- a jetty to walk out on.

It is lit **live**, with the sun and its shadow map; its bake stores only sky occlusion. A lightmap can't follow a sun that a weather preset moves, but occlusion (darker under the jetty, in corners) is true in any weather.

**Code.** `Shaders/Water.frag.hlsl`, `Renderer::GetWaterPipeline`, `Material::water`, `Tools/Blender/atom_lakeshore.py`, `Tests/Scenarios/lakeshore.atomtest`.

---

## 66. Weather as data: presets and blending (M49)

**Concept.** Weather here is **authored state**, not meteorology. A preset is a named set of environment values, and switching presets changes the whole mood at once:
- **Rain:** dimmer sun, grey sky, denser fog, darker and calmer water.
- **Sunset:** a low orange sun, a purple-to-orange sky, warm reflections.
- **Night:** a cold, weak "sun" (a moon whose disc still blooms), a near-black blue sky.

The key idea is that **the water's look is part of the preset**: a rainy lake isn't the sunny lake with rain on top.

**Presets as partial data.** `Assets/Environments/<name>.json` uses the same keys as a level's `lighting` environment, and every key is optional. What a preset leaves out comes from **the level**. `clear_day.json` is empty, meaning "the level as authored". A preset can therefore be used in any level, and the level's own light stays the reference.

**One reader, all or nothing.** The code that reads a level's lighting environment was turned into `ReadEnvironment(json, path, state)`. Levels and presets call the same function. `ApplyEnvironmentPreset` parses into a *copy* and assigns it only if the whole preset is valid. A typo then changes nothing and the error names the field (`/water/skyReflection: must be 0..1`), so the game is never left half-switched.

**The controller.** `EnvironmentController` holds a *from*, a *to* and a timer:
- **Eased:** `t² (3 − 2t)` (smoothstep), so weather drifts in rather than starting and stopping on a step.
- **Deterministic:** the state depends only on the elapsed time, so the same switches give the same result at any frame rate. The unit tests compare 12 small steps with one large one.
- **Change of mind:** a new switch mid-blend starts from *what's showing*, not from the old preset, so nothing jumps.

**Tools.** The F10 Environment panel has a button per preset, a blend time and live sliders for everything showing. Copy as JSON writes a complete preset file. Presets hot reload. The harness has `environment <name> [seconds]` and `expect_environment <name>`, which answers `(blending)` mid-transition. A level offers presets with `"environment": { "default", "presets" }`.

**Code.** `Game/Environment/EnvironmentController.*`, `ApplyEnvironmentPreset` (`LevelData.cpp`), `Assets/Environments/`, `environment.schema.json`, `Tests/EnvironmentControllerTests.cpp`, `Tests/Scenarios/environment.atomtest`.

---

## 67. Rain and wind (M50)

**Rain as streaks.** A raindrop falling at 8 m/s is a short line to the eye, not a dot, because it moves during the eye's exposure. The drops are particles with a `stretch`. The vertex shader builds a thin quad *along the fall direction* instead of a camera-facing square, then turns it about that axis to face the camera as well as it can:
```hlsl
side  = normalize(cross(fallDirection, cameraForward));
world = position + side * corner.x * width + fallDirection * corner.y * length / 2;
```
Like the leaves (§25), up to 1,400 drops live in a box that follows the player and wrap around its edges. The rain amount (0..1) decides how many are active. The wind tilts the fall direction, so the streaks slant.

**Rings on the water.** A grid of cells covers the lake. Each cell has a drop at a jittered spot, on its own clock: a ring grows from radius 0 and fades as it does. Each ring pushes the water's normal outward around its radius, which shows as a bright and dark circle. Heavier rain enables more cells, chosen by a hash compared with the rain amount. Two offset grids hide the pattern.

**Wet ground.** Surfaces facing up (`normal.y`) get darker when wet, as wet sand and wood do, and a faint drifting sheen of the sky appears. How this code is compiled turned out to matter; see §69.

**Sound and wind.**
- **Rain sound:** a new synthesized loop, a wash of filtered noise with occasional close "ticks". It plays only while it rains: a looping silent voice tripped the scenarios' voice-leak checks.
- **Wind** is now part of the environment and drives the existing gusts, the vertex sway of foliage (§38) and the leaves. The default equals the old fixed wind.

**Code.** `Game/Atmosphere.*` (drops), `Shaders/Particle.vert.hlsl` (streaks), `Water.frag` (`RainRings`), `BasicRain.frag`, `SoundSynth::Rain`, `AudioScape::SetRainLevel`.

---

## 68. A planar reflection (M51)

**Concept.** A **planar reflection** renders the scene a second time from a camera mirrored below the water's plane, into a texture. The water then samples that texture where its Fresnel showed only the sky. It's exact for flat water and costs a second render of whatever it draws, so it was built as an experiment and kept only after measuring it.

**Mirroring the camera.** Reflecting the world in the plane y = h is the matrix *translate(h) · scale(1, −1, 1) · translate(−h)*, applied before the view: `view' = view · mirror`. Two problems follow.

- **Triangles turn inside out.** A mirror reverses winding order: front faces become back faces, and back-face culling would draw the wrong side of everything. Instead of building extra pipelines with the opposite cull mode, the reflected image is also **flipped left-right** (x negated after projection). Two flips restore the winding. The water reads the texture with `u → 1 − u`; because the water lies on the mirror plane, its own pixel maps exactly there.
- **What's under the water rises into the sky.** Mirrored, the lake bed would appear above the water. It must be clipped at the plane, and there are no clip planes in SDL_GPU. The answer is an **oblique near plane** (Eric Lengyel's technique): rewrite the projection's depth row so its near plane *is* the water plane. The hardware then clips everything below it, with no shader test:
  ```
  c  = water plane in view space
  q  = inverse(P) · (sign(c.x), sign(c.y), 1, 1)   // the far corner opposite the plane
  P.row2 = c / dot(c, q)                           // for 0..1 depth
  ```
  The cost is depth precision far away, which a half-resolution reflection doesn't need.

**What it draws.** At half resolution:
- the sky;
- the opaque draws of the near layer, culled to the *mirrored* frustum with the same chunk culling as the main view.

It draws no decals, water, particles or mid and far layers. It runs only when a water surface is in view, and it's lit from the mirrored camera's position so fog and highlights match.

**The decision.** It mirrors the jetty, its posts and the reeds into the water, grounding them. It cost **0.12–0.22 ms** on the Iris Xe. It's kept as an option of the water look (`lighting.water.reflection`, presets can set it) and is on for the lakeshore.

**Code.** `Renderer::RenderReflectionPass`, `EnsureReflectionTargets`, `DrawQueue(..., reflection)`, `Water.frag` (t5), `Tools/Perf/water_and_weather.atomtest`.

---

## 69. Releasing 0.0.8: a branch that cost when skipped (M52)

**What the lake costs.** These are in-process paired benches (§63), two runs, from the beach and from the jetty's end:

| | beach | jetty's end |
|---|---|---|
| water | +0.34 ms | +0.68 ms |
| rain | +0.23 ms | +0.18 ms |
| both | +0.63 ms | +0.91 ms |
| reflection | +0.21 ms | +0.14 ms |

- **Water** costs per pixel (noise, Fresnel, sky), so it grows with how much screen it covers. From the jetty it fills the view.
- **Rain** is mostly its drops: updating 1,400 of them on the CPU, sorting and uploading them, and their overdraw.

**The regression.** The release check compares against the previous release with interleaved builds (`ab.ps1`). The demo levels don't use water, presets or rain, yet the street came out **0.26 ms slower** than v0.0.7.

- **The suspect:** the one per-pixel change every scene draw shared was the rain's wet-ground code in `Basic.frag`. It sat behind `[branch] if (rain > 0)`, a branch that's the same for every pixel and false in every dry level.
- **The test:** remove that code and nothing else. The street got 0.23 ms back.

This is the same thing §57 found with the weights view. Code that never runs can still cost: its presence changes how the compiler allocates registers and schedules the *whole* shader, and on this GPU that slowed every pixel.

**The fix: a shader variant.** Instead of branching at run time, choose at **compile time**. `BasicRain.frag` is three lines: `#define ATOM_RAIN` and `#include "Basic.frag.hlsl"`. The rain code sits inside `#ifdef ATOM_RAIN`. The renderer keeps a second set of scene and decal pipelines built from the variant and picks them only while it rains. Dry frames run the original shader exactly. Afterwards the street measured −0.035 ms against v0.0.7 and the windmill field −0.007 ms, both within the method's resolution.

**The general lesson** is the trade-off every engine makes between **uber-shaders** (one shader, runtime branches) and **shader permutations** (many compiled variants). Branches keep the pipeline count down; variants keep each shader as lean as what it actually does. Here two variants cost almost nothing to manage and gave the time back. Engines with hundreds of features do the same thing at scale and pay for it in compile time and pipeline caches.

**Code.** `Shaders/BasicRain.frag.hlsl`, `Renderer::GetScenePipeline` (the rain bit in its cache index), `Tools/Perf/ab.ps1`.

---

## 70. An architecture audit, and checked GPU lifetimes (M53)

**Concept: an audit before a rewrite.** After fifty-two milestones the engine was reviewed as a whole (2026-10-03): target dependencies, ownership, lifecycles, the renderer, assets, tests. The verdict was **no correctness failure and no case for a rewrite**. Its value was naming *where growth strains*, and just as importantly *what to protect*:
- the one-way game → engine dependency;
- immediate renderer submission;
- the slot-map world;
- committed deterministic assets;
- the paired measurements.

v0.0.9 is that audit's findings, done in small commits. The audit also rejected the things a growing engine is tempted by: an ECS, a render graph, an RHI, plugins, scripting, an asset database. Each needs a measured trigger first (§75).

**A contract in comments is not a contract.** `Mesh` and `Texture` free their GPU memory through a raw `SDL_GPUDevice*` they don't own. If one outlives the renderer, it calls SDL on a destroyed device: a use-after-free during teardown, silent until it crashes. The rule "destroy before the device" existed only in comments.

**Making it executable.**
- **Counting:** `GpuResources` counts live wrappers per device and kind. Each `Mesh`/`Texture` constructor counts itself in, and each destructor counts itself out.
- **Checking:** `Renderer::Shutdown` asks for the report *before* destroying the device: "GPU resources still alive at shutdown: 2 textures, 1 mesh; 0 render textures registered". It always logs, and asserts in Debug.
- **In the tests:** the scenarios' ctest definition fails on that line (`FAIL_REGULAR_EXPRESSION`), so a leak introduced anywhere fails the scenarios at once.
- **Ownership is unchanged:** the same `unique_ptr`s and the same raw pointers. This is the cheapest enforcement, not a redesign (renderer-owned handles would be the heavy alternative).

**A measurement that measured nothing.** The audit found the reflection costing ~0 ms where M51 had measured ~0.15 ms. The pass wasn't broken; the *benchmark* was:
- `bench` alternates AB BA, so after an even number of rounds it ended on A;
- `bench weather off on`, run just before, left the water **off**;
- the reflection only runs over water, so `bench reflection` compared nothing with nothing.

`bench` now always ends on its second value. The lesson: when two measurements disagree, check the *measuring* before the *measured*.

**Code.** `Engine/Renderer/GpuResources.*`, `Renderer::Shutdown`, `Tests/GpuResourcesTests.cpp`, `TestScript` (`bench` ends on B, `expect_reflection`).

---

## 71. Honest CMake: PUBLIC, PRIVATE and build options (M54)

**Concept: usage requirements.** A CMake target declares what it needs with a scope:

| Scope | Meaning | Example here |
|---|---|---|
| `PRIVATE` | my `.cpp` files need it | ImGui inside `DevTools.cpp` |
| `PUBLIC` | my headers need it, so anyone including them does too | SDL types, GLM maths |
| `INTERFACE` | only my users need it | (header-only libraries) |

Over-using `PUBLIC` makes everything compile. It also hides who depends on what. The game's developer panels used ImGui only because the engine leaked it through a `PUBLIC` link, and every consumer rebuilt for headers it never used. M54 declared each dependency where it's used:
- **ImGui:** `PRIVATE` in the engine and in the game library.
- **JSON:** `PRIVATE` in the game library; the tests link it themselves.
- **The version string:** `PRIVATE` in each target that prints it.

SDL and GLM, plus the GLM defines that change what GLM's headers mean, stay `PUBLIC`.

**A build option that removes a tool.** `ATOM_BUILD_GAME` (default ON) gates the executable, the shader compilation and the in-game scenarios. Off, the engine and game libraries and the unit tests build **without `dxc`**. That's what lets a plain CI machine build and test the engine. All three combinations (game+tests, tests only, game only) were built from clean directories.

**Code.** root, `Engine/`, `Game/` and `Tests/` `CMakeLists.txt`.

---

## 72. Continuous integration (M55)

**Concept.** **CI** runs the build and the tests on a clean machine for every change pushed. It catches the "works on my machine" failures a developer can't see locally:
- a file not added;
- a submodule not pinned;
- a dependency installed only on the laptop.

Engines of every size do this. Large ones run every platform, shader compilation and packaging; small ones run the cheap part.

**What runs where.** GitHub Actions (`.github/workflows/ci.yml`), on every push and pull request:
1. a fresh Windows runner clones with submodules;
2. it configures with `ATOM_BUILD_GAME=OFF` (no shader compiler, §71);
3. it builds `AtomTests` in Release and runs `ctest`.

**What doesn't run there, and why:**
- **The in-game scenarios:** hosted runners have no real GPU.
- **Performance:** timings on shared machines are too noisy to gate on.

Both stay local. CI is a confirmation *after* pushing, not a replacement for testing while working (§75).

**Code.** `.github/workflows/ci.yml`; README "Testing".

---

## 73. The runtime asset payload, from evidence (M56)

**Concept: source and product.** The vocabulary engines use for content (O3DE uses it too):
- **Source:** what people edit (the Blender scripts, hand-written JSON).
- **Products:** what tools generate from it (GLB, PNG, markers, DXIL).
- **Runtime payload:** the products the game needs at run time.

The build used to copy all of `Assets/` next to the game, so nothing could answer "what does the game need to run?".

**Evidence first.**
- **A log:** `ATOM_ASSET_LOG=<file>` makes every loader report each path it opens, once: models, textures, collision, fonts, WAV, levels, dialogue, playfields, impostors, presets, data.
- **The run:** every scenario plus a visit to every level opened **86 files from 20 folders**. `Schemas/` was never opened; editors and the authoring tests read it from the source tree.

**The list.** `Game/CMakeLists.txt` now names those 20 folders and copies them with `copy_directory_if_different`, which requires CMake 3.26. It works **per folder, not per file**: only 14 of the 31 kit models are used today, and a file list would break the day a level uses another. To validate it, the copied `Assets` folders were deleted, rebuilt from the list alone, and every scenario passed against them. Hot reload (`ATOM_ASSET_ROOT`) still reads the full source tree.

**Code.** `Engine/Core/AssetLog.*` (and its calls in each loader), `Game/CMakeLists.txt`.

---

## 74. Diagnostics out of the coordinator, and load timings (M57)

**Concept: extract what changes for its own reasons.** `DemoApp` had become the place where everything meets. Gameplay coordination belongs there; measurement and test control do not. `GameDiagnostics` took the frame-time log, the scripted test runner (and the exit code it produces) and the fixed time step. `DemoApp` keeps the frame's explicit order and calls it.

This is one owned collaborator, not a framework: no event bus, no service interfaces.

**Characterization tests first.** Before moving code, tests pinned what it *did*:
- the PERF line byte for byte, since `ab.ps1` parses it;
- the warm-up and blocks, and the CSV;
- the fixed step;
- the test exit code: 0 or 1, reported once.

Moving code under such tests turns "I think it's the same" into "the tests say it's the same". The perf output now goes to any stream, which is what made it testable.

**Measure before optimizing loading.** Each model records four stage times:
- **parse:** the glTF;
- **decode:** the images;
- **upload:** creating GPU textures and meshes;
- **build:** everything else.

Every level load logs the total, models loaded versus reused, and where the time went. The night street: **173 ms, 11 models, 82 ms of it GPU upload**, against 2 ms of parsing. A faster parser (fastgltf) would change almost nothing here. If loading ever needs to be faster, upload is where to look. That's the kind of evidence the audit asked for before any loader refactor.

**Code.** `Game/Testing/GameDiagnostics.*`, `Tests/GameDiagnosticsTests.cpp`, `Tests/FakeGame.h`, `Model::LoadTimes`, `ModelCache`, `Level::Create`.

---

## 75. Writing the architecture down, and tooling that doesn't hang (M58)

**An architecture document.** `docs/Architecture.md` is the *structure* counterpart to this manual's concepts. Its key correction concerns the layering. It isn't "Application → Engine → Game":
- **`Atom::Application`** is the engine's runtime and **composition root**: it owns and orders every subsystem.
- **`DemoApp`** is the concrete game deriving from it.
- There is **no `Engine` object**, and adding one would duplicate `Application`.

The document also covers diagrams of targets, lifecycles and passes, the GPU lifetime rule, the asset vocabulary, and **architecture decision records**. An ADR is a short note of a decision, its reasons and when to revisit it. Its table "when to add what" names the trigger for each tempting addition: an ECS when entity queries are measured as painful; fastgltf when parsing fails a requirement; Jolt when dynamic 3D bodies enter scope.

**A watchdog for external runs.** `ab.ps1` waited on each game run forever. The game's script timeout counts *game* time, which stops if frames stop (a covered window, a driver stall, a dialog box). Now:
- each run has a **wall-clock** limit;
- a hung run is killed and retried once, and a second hang stops the comparison while keeping the measured rounds;
- the environment variables it sets are restored in a `finally` block. They used to linger in the caller's shell, so a later manual launch started in the benchmark level and quit.

The general rule: a timeout inside the process can't catch the process hanging; something outside must watch the clock.

**A validation ladder.** Check a change with the cheapest step that can catch its mistakes, and climb only when that passes:

| Level (`Tools/Dev/check.ps1`) | Runs | For |
|---|---|---|
| `docs` | nothing | `.md`, CHANGELOG, `.gitignore`, the CI workflow |
| `quick` | one incremental build + unit tests | the inner loop; content JSON |
| `feature` | quick + the named scenarios | a feature, with a change-to-scenario map |
| `full` | Debug and Release, everything | before a milestone or release commit |

Use Debug for asserts and lifetime checks, Release for anything timed. Rebuild assets only when content changes. The full matrix is a **gate**, not the inner loop. A `CLAUDE.md` gives agents the same rules. It also bans editing escaped source (`\n`) through inline scripts, whose extra quoting layers broke C++ string literals three times in this project.

**Releasing 0.0.9.** No feature, so no runtime cost: against v0.0.8 with interleaved builds, the street +0.027 ms and the lakeshore +0.024 ms, within resolution.

**Code.** `docs/Architecture.md`, `Tools/Perf/ab.ps1`, `Tools/Dev/check.ps1`, `CLAUDE.md`.

---

## 76. A settings model: two decisions, kept apart (M59)

**Concept: separate what changes for different reasons.** Two settings look alike but aren't:
- **The GPU preference** (`low-power` or `high-performance`) decides *which adapter* the game asks for. It's a **stability** choice: on the development laptop, the low-power adapter was hardcoded from the very first demo because the other one lost its swapchain when windows moved between monitors.
- **The graphics quality** decides *how much is drawn*. It trades looks for speed.

"Fast GPU" and "high quality" sound like one dial, which is the trap. On a hybrid laptop the fast GPU can be the unstable one, and a slow GPU can still afford High.

**Quality presets.** A tier is a bundle of switches the renderer already had:

| Tier | Render scale | MSAA | Shadows, particles | Reflection |
|---|---|---|---|---|
| High (default) | 1.0 | 4× | on | the level's choice |
| Balanced | 0.75 | 2× | on | off |
| Low | 0.5 | 1× | off | off |

Tiers **cap, they don't force**. The lakeshore's reflection is authored per level (§68), so High *allows* it and lower tiers turn it off. No renderer feature exists for one tier only. If an F-key changes a single switch, the tier becomes **Custom**: the label tells the truth about what's drawn.

**Precedence, as a pure function.** `ResolveSettings(cli, env, saved)` merges four layers, strongest first:

```text
command line  >  ATOM_* environment  >  settings.json  >  defaults
```

It's a **pure function**: values in, a value out, no files, no SDL. So it's unit-tested exhaustively, which matters because precedence bugs are the confusing kind ("I set it but nothing changed").

**Never fail on a settings file.** The file is user-owned and may be damaged, hand-edited or from a future version. `ParseSettings` never fails: a bad field gets its default, an unknown schema gets all defaults. A settings file must never stop a game from starting.

**Code.** `Game/Settings/GameSettings.*` (`QualityPreset`, `PresetFor`, `TierOf`, `ResolveSettings`), `Tests/GameSettingsTests.cpp`.

---

## 77. The command line and choosing the GPU before it exists (M60)

**`main` had ignored its arguments.** Now `argv` reaches `DemoApp`, and a pure `ParseCommandLine` turns it into options: `--gpu`, `--quality`, `--calibrate`, `--diagnostics <file>`, `--no-settings`, `--reset-settings`. Unknown arguments are reported, not fatal.

**Concept: some decisions must come before construction.** SDL picks the adapter when the GPU device is *created*, and a device can't change adapters afterwards. So the choice has to exist before the renderer does. `Application` gained a hook that runs first:

```cpp
const StartupConfig startup = OnConfigure(); // the game resolves its settings here
SDL_Init(SDL_INIT_VIDEO);                    // ... then window, then the renderer
```

This is the **template method** pattern again (§2): the base class owns the order, and the game fills in a step. Changing the GPU later therefore says **"restart required"**: rebuilding every GPU resource in-process was rejected as too much machinery for a rare choice.

**Concept: tests must not depend on whoever played last.** If a saved Low preset applied during scenarios or benchmarks, results would change depending on the last key someone pressed. The rule: with `ATOM_TEST_SCRIPT` set, or `--no-settings`, the saved file is neither read nor written. `ab.ps1` and every scenario get exactly the old behaviour.

**Code.** `Game/Main.cpp`, `Engine/Core/Application.*` (`StartupConfig`, `OnConfigure`), `ParseCommandLine` in `GameSettings.*`.

---

## 78. Saved settings and the F10 Settings panel (M61)

**Where per-user files go.** `SDL_GetPrefPath("AtomEngine", "AtomGame")` returns the platform's per-user, writable folder: on Windows `%APPDATA%\AtomEngine\AtomGame\`. The install folder may be read-only, and it's shared between users, so it's the wrong place.

**Show what is, not what was asked.** A preference is a *request*. The Settings panel shows both sides:
- the adapter SDL **actually** picked, next to the preference;
- the quality mode *and* the tier being drawn (Custom included);
- the calibration result;
- *Calibrate now*, *Calibrate next launch*, *Reset*.

Quality applies at once; the GPU radio button is marked "restart required" (§77).

**A smoke test per tier.** Running every scenario at three tiers would triple the test matrix. Instead, one scenario (`quality_tiers`) sets each tier and checks that its switches took effect (`expect_quality`, `expect_reflection`). The full multi-tier run happens once, at release (§82).

A bug worth remembering: the new panel first opened exactly **under** the Environment panel, so it seemed missing. ImGui windows need distinct default positions.

**Code.** `DemoApp::LoadSavedSettings` / `SaveSettings`, `Game/DemoAppDevTools.cpp`, `Tests/Scenarios/quality_tiers.atomtest`.

---

## 79. Diagnostics and a doctor (M62)

**Concept: a bug report needs a machine report.** "It's slow on my PC" is useless without context. `--diagnostics <file>` starts the engine, writes what it found and quits:
- SDL version, backend, adapter, the requested preference and **why** it was or wasn't honoured;
- present modes, maximum MSAA, scene format, frames in flight;
- display size, scale and refresh rate, power source;
- the quality tier and the effective settings.

**Context on every measurement.** A timing without its conditions misleads (a battery run is ~3× slower, §63). Every PERF log now starts with a `PERF context` line: adapter, power source, tier.

**A doctor script.** `Tools/Dev/doctor.ps1` answers "can this machine build it?": Windows version, CMake ≥ 3.26, Visual Studio's C++ tools, `dxc`, submodules, optionally a trial configure (in a throwaway folder) and the game's own diagnostics. Each check prints PASS, WARN or FAIL **with what to do**. It **only reads**: no drivers, power plans or files are changed. A diagnostic tool that modifies the system is no longer a diagnostic tool. The tool lookup it shares with `check.ps1` moved into `Tools/Dev/common.ps1`.

**Code.** `DemoApp::WriteDiagnostics`, `Renderer::GetDeviceReport`, `GameDiagnostics::SetPerfContext`, `Tools/Dev/doctor.ps1`, `Tools/Dev/common.ps1`.

---

## 80. Hybrid laptops and the high-performance fallback (M63)

**Concept: how a hybrid ("Optimus") laptop shows a picture.** It has two GPUs:
- an **integrated** GPU (here an Intel Iris Xe), inside the CPU, low power;
- a **discrete** GPU (here an RTX 4060 Laptop), fast and power-hungry.

The built-in panel's cable usually goes to the **integrated** GPU only. When a game renders on the discrete GPU, Windows copies each finished frame across to the integrated GPU, which scans it out. External ports (often HDMI) may be wired straight to the discrete GPU. So "which GPU" and "which screen" interact: moving a window between screens can mean changing who presents.

**What was measured on this laptop:**
- With the Iris enabled, Direct3D 12 **refuses** an RTX swapchain for the built-in panel (`DXGI_ERROR_DEVICE_REMOVED`), even in a raw D3D12 probe with no engine code. On the external monitor, wired to the RTX, it works.
- With the Iris *disabled* in Device Manager, the panel falls back to the "Microsoft Basic Display Driver" (a software driver, 64 Hz): every frame took ~73 ms whatever was drawn. Disabling the integrated GPU doesn't give the panel to the discrete one; it removes the part that drives it.
- Windows' per-app *Graphics* setting, forced to "High performance", overrides **both** SDL preferences, so no fallback can reach the Iris. The game now explains this and suggests *Let Windows decide*.

**The fallback, in two places:**
- **At creation:** if a high-performance device or its swapchain fails, retry low-power *before* any resource exists, and show why.
- **Mid-run:** a lost swapchain calls `OnRenderFailure`; the game saves `pendingFallback = low-power` and exits with code 3. The next launch uses low-power and explains.

**Concept: sometimes the safe cleanup is no cleanup.** After a lost swapchain, releasing the window from the device, or destroying the device, corrupted the heap inside SDL (`0xC0000374`). On that one path the device is **abandoned**: the process is exiting, and the OS reclaims everything a process owns. Leaking deliberately at exit beats crashing during cleanup. `ATOM_SIMULATE_SWAPCHAIN_LOSS` and `ATOM_WINDOW_POSITION` reproduce the failure without unplugging anything.

**Code.** `GPUDevice::Initialize` (retry, `ExplainNoDevice`; in `Renderer::Initialize` until §83), `Renderer::Failure`, `Application` (exit 3), `DemoApp::OnRenderFailure`, `FallbackAfterFailure`, `Tools/PresentationProbe`.

---

## 81. Calibration, frames in flight and latency (M64)

**Calibration (opt-in)** measures instead of guessing from GPU names. It plays the two heaviest views (the night street; the lakeshore in rain), each tier twice in the order **H B L L B H**, and picks the highest tier whose **worst p95** is ≤ **13.3 ms** (75 fps):
- **Why p95, not the average:** stutter is felt in the slow frames. A 5 ms average with 20 ms spikes feels worse than a steady 8 ms.
- **Why twice, keeping the better pass:** background stalls only ever *slow* a pass; the better of two is closer to the machine's real speed. The ABBA order (§63) spreads drift over all tiers.
- **Refusals:** on battery it refuses. When the display caps presentation (vsync, or a 60 Hz monitor holding frames), frame times measure the *display*, not the GPU, so it reports no result instead of a wrong one. During the run, presentation is uncapped.
- **Measured as the player plays:** at the player's frames in flight (below). Calibrating with a deeper queue than play would overrate the machine.
- **The decision is pure** (`DecideCalibration`) and tested with synthetic measurements. The record stores the adapter and resolution, and is ignored when either changes.

**Concept: frames in flight and GPU clocks.** "Frames in flight" is how many frames the CPU may queue before waiting for the GPU. SDL's default is 2. Calibrating with 2, the result flipped between High and Balanced, and 3-second medians swung **3.5 ↔ 9.7 ms**. The cause: with only 2 queued, the Iris finished early and **idled between frames**; its power management saw idle time and **lowered its clock**, so the next frames got slower, then busy again, and so on. With **3** frames queued, the GPU always has work, the clock stays up, and calibration chose High every time (worst p95 7.8–8.0 ms). (A later day, two runs at 2 also chose High: the clock drop comes and goes with the machine's state, which is exactly why it's worth removing.)

**Concept: queue depth is latency.** Each frame waiting in the queue was built from older input. When the player moves the mouse, the result reaches the screen only after the frames already queued ahead of it. How much that costs depends on whether the queue is full:
- **GPU-bound or vsync:** the CPU runs ahead until the limit, so the queue is full, and each extra slot adds one whole frame. With vsync that's one **refresh**: **+6.9 ms at 144 Hz, +16.7 ms at 60 Hz**. And vsync is the default present mode.
- **CPU-bound:** the GPU waits on the CPU and the queue is empty, so depth costs nothing.

That argument nearly put the default back to 2: AtomEngine is an engine, and the games it runs may need fast, precise controls. The measurement that settled it was vsync on the 144 Hz panel, 2 vs 3, interleaved:

| Frames in flight | Frame interval | Frame rate |
|---|---|---|
| 2 | 13.9 ms | **72 fps** |
| 3 | 6.95 ms | 144 fps |

**Concept: double vs triple buffering.** In SDL's D3D12 backend, "frames in flight" also sets the **swapchain's buffer count** (2 or 3). With **two buffers** and vsync, one is on screen and the other waits for the next refresh. The game has nowhere to draw until a refresh frees one, so it misses every other refresh even with 3.4 ms of work: the classic double-buffering halving. With **three**, there's always a free buffer. Every release before 0.0.10 ran at 72 fps on this panel.

So the latency reasoning cut the other way. Latency is roughly *queued frames × frame interval*: about 2 × 13.9 ≈ 28 ms with two, against 3 × 6.9 ≈ 21 ms with three. One more queued frame costs less than every frame lasting twice as long. The default is **3**; `ATOM_FRAMES_IN_FLIGHT=1..3` overrides. Calibration measures at the player's setting. Input-to-screen latency itself wasn't measured (that needs timestamps from input to present, or a capture tool), so those 28 and 21 ms are estimates. *v0.0.12 measured them, and they were wrong (§90–§92): 3 frames gave 36 ms, and the default is now 2 frames with the wait before input, at 24 ms.*

The lessons:
- A GPU is not a constant-speed machine: power management reacts to the workload's *shape*.
- An API setting can mean more than its name: "frames in flight" here also meant "swapchain buffers".
- A sound general rule ("deeper queues add latency") still has to meet the measurement on the actual machine.

**Code.** `Game/Settings/Calibration.*`, `Game/DemoAppCalibration.cpp`, `Tests/CalibrationTests.cpp`, `Renderer::SetUncappedPresentation`, `SDL_SetGPUAllowedFramesInFlight` in `Renderer.cpp`.

---

## 82. Releasing 0.0.10: what the RTX taught (M65)

**Documenting a decision with its evidence.** `docs/Architecture.md` gained §8 "Hardware and settings" and **ADR-006**: low-power stays the default, and the GPU choice is not a quality setting. It records the context (the measurements of §80), the rejected options (scoring GPUs by name, a hardware database, continuous adaptive quality, changing the power plan, an in-process device rebuild, a launcher) and when to revisit.

**Assumptions vs measurements.** Three claims made during this release turned out wrong once measured:
- "The RTX can only present to the external monitor" was a generalization from a test that had the monitor connected. The panel test had to be done separately.
- "Disable the Iris and the RTX will drive the panel" was disproved by Windows' own `dxdiag`: the panel went to a software driver.
- "Two frames in flight is lower latency" holds only if the frame rate stays the same. With vsync it halved (§81).

All three were settled by Windows' reports and paired measurements, not by argument.

**The release measurements:**
- **Full matrix:** Debug and Release, 17/17 each.
- **Multi-tier run:** every scenario at Low and Balanced. The only failures were checks for what those tiers turn off on purpose (rain particles at Low, the reflection, the default-tier check). The tests caught the tiers *working*.
- **Against v0.0.9** (`ab.ps1`, 8 rounds): the night street **−0.51 ms**, the lakeshore **−0.55 ms**, every round negative. To attribute it, both levels were repeated with v0.0.10 forced to 2 frames in flight: **−0.006 ms and −0.002 ms**, identical. So the speed-up is entirely the third frame in flight (§81), and the settings machinery costs nothing. With vsync the difference is 72 → 144 fps.

An A/B that finds a difference isn't finished until a second A/B explains it.

**Code.** `docs/Architecture.md` §8 and ADR-006, `CHANGELOG.md`, `README.md` "Settings and hardware".

---

## 83. Device vs presentation: GPUDevice

**Concept: two steps that can fail separately.** Getting a picture from a GPU onto a window takes two steps:
1. **Creating the device:** SDL asks DXGI for an adapter (by preference) and creates a Direct3D 12 device on it. This only needs the GPU.
2. **Claiming the window:** SDL creates the window's **swapchain** for that device (`CreateSwapChainForHwnd`). This needs a path from that GPU to the screen the window is on.

On a hybrid laptop these really do come apart. The RTX 4060 is a perfectly good device, but showing its frames on the built-in panel means **cross-adapter presentation**: the RTX renders, and Windows and the drivers copy each frame to the Iris Xe, which drives the panel. That second path is what fails on the development laptop.

**The flaw it exposed.** `Renderer::CreateAndClaimGPUDevice` ran both steps and returned one boolean, and it logged the adapter only when both succeeded. So the log said the high-performance GPU was "unavailable", and the RTX never appeared in it at all. One boolean hid which step failed.

**The fix is a boundary, not just a message.** Device bootstrap and presentation (adapter preference, the hybrid fallback, the window claim, the swapchain's composition, present mode and frames in flight) form their own subsystem. Scene rendering doesn't need to know any of it. So it moved into **`GPUDevice`**, which the renderer owns:

```text
Application → Renderer (frames, passes, resources) → GPUDevice (device + presentation) → SDL GPU → D3D12
```

- `Renderer::Render()` stays the frame coordinator, pass by pass.
- **No `IGraphicsBackend`, no `D3D12Device`/`VulkanDevice` classes.** SDL already is the backend abstraction (ADR-001); a layer on top would map one to one. This is a responsibility split, not an RHI.
- The pure decisions moved out as testable functions: `ChoosePresentMode` (vsync, mailbox, immediate, and the M46 `ATOM_PRESENT` rule) and `DescribeFallback` (the wording).
- Future fullscreen, resize or device-recovery work touches `GPUDevice`, not the pass code.

**Honest diagnostics.** The adapter is logged as soon as the device exists, *before* the claim, and a failed claim says which adapter it failed on:

```text
GPU device created: backend=direct3d12 adapter="NVIDIA GeForce RTX 4060 Laptop GPU" preference=high_performance
GPU presentation failed on "NVIDIA GeForce RTX 4060 Laptop GPU": could not claim the window: Could not create swapchain! Error Code: ... (0x00000000)
GPU device created: backend=direct3d12 adapter="Intel(R) Iris(R) Xe Graphics" preference=low_power
```

`StartupFallback` records the **stage** (`device` or `presentation`), the adapter and the error; `--diagnostics` prints them as `gpu.fallback.*`. SDL's versions now print first, so the log reads like a bug report.

**Don't trust an error code that says success.** SDL quotes `0x00000000` ("the operation completed successfully") for a swapchain that wasn't created: the real code got lost on the way. The raw D3D12 probe, without SDL, reports `0x887A0005` (`DXGI_ERROR_DEVICE_REMOVED`) for the same call. The failure is therefore **below SDL**, in DXGI or the drivers' cross-adapter path. Updating SDL to 3.4.18 changed nothing, as expected.

**Muxless laptops.** NVIDIA calls the arrangement here **classic Optimus**: no MUX, so the panel is wired to the Iris only, and it can't be switched to the RTX ("Advanced Optimus: No"). On such a machine the Iris must stay enabled (§80 showed what happens without it), and RTX rendering has to work *through* it.

**Concept: an investigation by elimination.** "The RTX can't present" has many possible causes, and each one was turned into a test that rules it in or out:

| Suspect | Test | Result |
|---|---|---|
| AtomEngine's code | raw D3D12, no SDL, no engine | fails the same way |
| SDL | 3.4.16 vs 3.4.18 | no change |
| The swapchain kind | flip discard and sequential, bitblt, 2 or 3 buffers, BGRA or RGBA | all fail on the RTX, all work on the Iris |
| Direct3D 12 itself | the same matrix in D3D11, RTX chosen explicitly | fails too |
| NVIDIA's opt-ins | the `NvOptimusEnablement` export; a per-program NVIDIA profile | no effect |
| The launching session | run from the user's own terminal | same result |
| The Intel driver | 32.0.101.6790 → 7092 | no change |

The driver update had a lesson of its own. Intel's newest main driver (32.0.101.9034) no longer lists this chip (device `A7A0`, 13th gen); 11th–14th gen Iris Xe now get a separate, slower driver branch. The installer's "No supported devices" was the chip missing from the INF's device list, not a broken install.

**Concept: two roads to the dGPU.** One clue didn't fit: *The Evil Within* renders on the RTX and shows on the same panel. That's because muxless laptops have two paths:
- **Windows' hybrid path:** a program picks the high-performance adapter itself (as SDL does), and Windows copies its frames to the iGPU. This is the path that fails on this laptop.
- **NVIDIA's older Optimus path:** a driver profile recognizes the game, the game sees the Intel adapter, and the driver quietly renders on the RTX. It serves **D3D9–D3D11 and OpenGL** only. **D3D12 has no such path**, so a D3D12 program depends entirely on Windows'.

Windows' own counters showed which GPU each process used (`\GPU Engine(pid_*_engtype_3D)`, matched to the adapters' LUIDs). AtomEngine, even with an NVIDIA profile, rendered on the Iris.

**The conclusion is ADR-006's addendum.** On this machine no D3D12 program can show an RTX image on the built-in panel. It's the laptop's hybrid-display stack, beyond any engine's reach. Low-power stays the default, high-performance stays an opt-in that falls back and says why, and the RTX is tested on an external monitor wired to it. `SwapchainMatrix` checks any machine in seconds.

**Code.** `Engine/Renderer/GPUDevice.*`, `Renderer::Initialize` / `GetDeviceReport` / `Shutdown`, `DemoApp::WriteDiagnostics`, `Tests/GPUDeviceTests.cpp`, `Scenario.gpu_fallback`, `Tools/PresentationProbe` (`PresentationProbe`, `SwapchainMatrix`), ADR-006 addendum in `docs/Architecture.md`.

---

## 84. The C++ runtime: from a DLL to linked in (M66)

**Concept: a program's invisible dependencies.** C++ code calls a runtime library for things like `new`, `std::string`, exceptions and `printf`. MSVC offers it two ways:
- **as DLLs** (`/MD`, the default): `VCRUNTIME140.dll`, `MSVCP140.dll` and the `api-ms-win-crt-*` set, which Microsoft ships as the *Visual C++ Redistributable*;
- **statically** (`/MT`): the parts the program uses are copied into the `.exe` at link time.

Visual Studio installs the Redistributable, so on a development machine the DLL version always works. On a fresh PC it may not be there at all, and the game then fails with "VCRUNTIME140.dll was not found" before a single line of AtomEngine runs. `dumpbin /dependents` showed both `AtomGame.exe` and `SDL3.dll` depending on it.

**The fix is one CMake line**, set before the first target:

```cmake
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
```

**Why every target, SDL included:** each static runtime has its own heap. Memory allocated by one module and freed by another, using a different runtime, corrupts memory. Using one choice everywhere rules that out.

**The cost:** the exe grew from 1.74 MB to 2.08 MB, and SDL3.dll from 2.75 MB to 3.00 MB. After the change, the binaries load only Windows' own DLLs and SDL3.dll.

**Concept: classifying dependencies.** A git submodule isn't a runtime dependency. GLM, nlohmann/json, cgltf and stb are headers or compiled-in sources, and Dear ImGui is a static library; all of them disappear into the exe. Only SDL3 remains a DLL. Blender, Python and dxc are *authoring* tools whose products are committed or built. CMake, doctest, the probes and the perf tools are *development* tools. Architecture §7 keeps the table.

**Code.** The top `CMakeLists.txt`; Architecture §7.

---

## 85. Console or window, and a log (M67)

**Concept: the subsystem.** Every Windows executable declares a *subsystem* in its header:
- **console** (`Windows CUI`): Windows gives it a console window, and `stdout`/`stderr` write there;
- **windows** (`Windows GUI`): no console; started by double-click, its output goes nowhere.

A game for players should be GUI: a black terminal behind the game looks broken. A developer wants the console. So the subsystem is a **build choice**: `ATOM_DISTRIBUTION=ON` sets CMake's `WIN32_EXECUTABLE`, and development builds stay console.

The first version made every build windowed, and development lost its console. The lesson: a distribution concern belongs to the distribution build.

**The entry point.** A GUI program starts at `WinMain`, not `main`. Including `SDL3/SDL_main.h` lets SDL supply `WinMain` and call our `main`, so one source serves both subsystems.

**Concept: standard handles.** A process inherits three handles (input, output, error) from whoever started it:
- **ctest and `ab.ps1`** redirect them into pipes, so even a GUI program's output reaches them;
- **a terminal** gives a GUI program none, but it can `AttachConsole(ATTACH_PARENT_PROCESS)` and reopen `CONOUT$`;
- **a double-click** leaves nothing to attach to.

`RunLog` checks which case applies.

**A tee for the log.** `TeeBuffer` is a small `std::streambuf` that forwards every write to two buffers: the console's and the log file's. Installed into `std::cout` and `std::cerr`, it makes every existing message reach `%APPDATA%\AtomEngine\AtomGame\logs\AtomGame.log` without touching a single call site.
- **Crash-safe:** each finished line is flushed, so a crash keeps everything before it.
- **Rotation:** the previous run is kept as `AtomGame.previous.log`.
- **Scripted runs skip the file,** so tests never overwrite a player's last real session.

**Failing visibly.** If start-up fails with no console (a real double-click), `main` shows `SDL_ShowSimpleMessageBox` with the last error line and the log's path. It's never shown with a console, so it can't block ctest.

**Code.** `Game/Platform/RunLog.*`, `Game/Main.cpp`, `Application::StartFailed`, `Tests/RunLogTests.cpp`, `ATOM_DISTRIBUTION`.

---

## 86. One package command, from one definition (M68)

**Concept: install rules.** CMake can describe not just how to build, but what to *install*:
- `install(TARGETS …)`, `install(FILES …)` and `install(DIRECTORY …)` list what goes where;
- `cmake --install <build> --prefix <folder>` copies exactly that into a folder;
- so they're the natural single definition of a package.

The rules in `Game/CMakeLists.txt` install:
- the exe, `SDL3.dll` and the shaders;
- the **shipped** asset folders. The asset list was split into `ATOM_SHIPPED_ASSETS` and `ATOM_DEV_ASSETS`: the character lab stays in development builds (its scenario) and out of the package, because its character's licence is unknown;
- `LICENSE.txt`, a generated `THIRD_PARTY_NOTICES.txt` (the MIT, zlib and OFL licences require their text to travel with binaries), and a players' `README.txt`.

**Staging, then verifying.** `Tools/Dist/package.ps1` runs: build `build-dist/` → install into `Dist/AtomGame/` (deleted first, so nothing stale survives) → verify → start it from `%TEMP%` with another working directory → zip.

`verify.ps1` takes its expectations from a `.payload` file the same install rules write, not from a second, hand-kept list. A package fails when:
- something is **missing** (a folder, the 21 shaders, a licence);
- something is **extra** (`.blend`, `.py`, `.pdb`, test scripts, schemas, the lab);
- a binary **needs a DLL** Windows doesn't have (`dumpbin`);
- a shader is **unsigned**.

**Concept: DXIL signing.** A compiled shader container starts with `DXBC` and a 16-byte digest. dxc fills the digest only when it finds `dxil.dll` (the validator) beside it. Unsigned shaders run on a developer's machine, but release drivers refuse them, so the check reads the digest.

**Concept: reproducibility.** The same revision must give the same package. Every file is identical between runs. The ZIP container isn't, because the writer stores its own metadata, so the guarantee is stated as "same contents". Stamping entries with the commit's time (`--mtime`) at least removes the build time.

Two practical bugs taught their own lessons:
- **`dumpbin`'s header** "Dump of file …SDL3.dll" looked like a dependency.
- **`.gitignore` is case-insensitive on Windows,** so `Dist/` also hid `Tools/Dist/`. Anchored patterns (`/Dist/`) fixed it.

**Code.** `Game/CMakeLists.txt` (install rules), `Shaders/CMakeLists.txt` (exports the shader folder and count), `Tools/Dist/package.ps1`, `verify.ps1`, `README.txt.in`.

---

## 87. CI as a check, not a release channel (M69)

A second CI job builds the game and its shaders on a fresh runner (dxc from the Windows SDK), then runs `package.ps1 -NoSmoke`. There's no GPU on hosted runners, so it doesn't start the game. Its value is the **red or green**: a change that breaks the package (a missing file, a new DLL dependency, an unsigned shader) fails a pull request.

**Concept: artifacts vs releases.**
- **An artifact** is a file a CI run keeps. It's downloadable only when signed in to GitHub, found on that run's page, wrapped in another ZIP, and gone after 90 days. That's useful to a developer, not to a player.
- **A release** (a GitHub Release page, an itch.io upload) is for players.

The first version uploaded the 16 MB ZIP on every run, and nobody would ever download it, so the upload was removed. Packages for players are made with the package command and published by hand, which at this pace is a minute a few times a year. Automate (a manual "publish" job, itch.io's butler) only when publishing becomes frequent.

The workflow runs on **pull requests, pushes to master, and on demand**, not on every push. The docs had said otherwise since 0.0.9.

**Code.** `.github/workflows/ci.yml`.

---

## 88. What ships, and the clean-machine test (M70)

**Concept: keep diagnostics, don't strip.** It's tempting to remove developer features from a release. But the shipped executable should be the *tested* one, and on a stranger's PC a hidden diagnostic is priceless. Architecture §9's policy:
- **Always available:** the log, `--diagnostics`, the start-failure box.
- **Shipped as diagnostics:** F10, F1, F2–F8, behind keys a player never needs.
- **Development-enabled:** `ATOM_*` switches, the harness, `bench`, `screenshot`. They ship but stay inert unless set.
- **Debug only:** asserts, the D3D12 debug layer.
- **Not packaged:** the lab, the tests, the probes, the tools, the schemas.

**Reversed in v0.0.13 (§101):** packages now carry *no* developer tools. F10, F1 and the `ATOM_*` switches are compiled out (`ATOM_DEV_TOOLS=0`); the log, `--diagnostics` and the start-failure box stay. What was learned in between: with two games, "inert unless used" left the facilities uneven and within a player's reach.

**Two builds, one source:**

| | `build/` | `build-dist/` |
|---|---|---|
| For | development | players |
| Program | console | windowed (`ATOM_DISTRIBUTION=ON`) |
| Configurations | Debug, Release | Release |
| Tests | yes | no |

**Concept: a clean machine.** "Works on my machine" hides dependencies the developer installed long ago. The decisive test is a Windows with nothing on it: a **VirtualBox VM** restored from a "clean" snapshot each time. Windows containers can't do this: they need Windows Pro or Server, have no desktop and no real GPU. VirtualBox has no Direct3D 12 either, so the VM proves everything up to the GPU (DLLs, paths, log, settings, the failure message), but not a play-through. The procedure and its expected results are kept as a local working note, outside the repository.

**Code.** `docs/Architecture.md` §9, the README's Distribution and troubleshooting sections.

---

## 89. Releasing 0.0.11 (M71)

**The numbers:**
- **Exe:** 2.0 MB.
- **Package:** 25.9 MB in 122 files, one DLL; the ZIP is 16.1 MB.
- **Packaging:** 6 s when up to date, 132 s from a fresh clone.
- **Speed vs v0.0.10:** night street +0.07 ms, lakeshore +0.004 ms. The static runtime and the subsystem cost nothing.
- **Unexpected runtime dependencies:** none, beyond the C++ runtime.

**What distribution pressure revealed:**
- **The runtime was mostly ready.** Paths from the executable's folder, settings in `%APPDATA%` and an explicit asset list were all earlier decisions that paid off here.
- **The blockers were outside the code:** a borrowed C++ runtime, a console, no log, and an asset whose **rights** weren't recorded. Licences are part of engineering once something ships.
- **SmartScreen.** Windows warns about unsigned executables downloaded from the internet ("Windows protected your PC" → *More info → Run anyway*). Removing the warning takes **code signing**, a certificate bought and a signing step added. That's a cost and a process, deferred until publishing makes it worth it.

**Next, from the evidence:** a play-through on a second real PC, a lab character with a clear licence, and input latency measured.

**Code.** `CHANGELOG.md` 0.0.11.

---

## 90. Measuring input latency with PresentMon (M72)

**Concept: input latency.** The time from an action (a click, a key) to its result on screen. Frame time says how *often* frames come; latency says how *late* each one is. Two settings can have the same 144 fps and very different latency, which is exactly what this version found.

**Concept: measuring it in software.** Without a high-speed camera, the standard tool is **Intel PresentMon**. It reads Windows' own event tracing (ETW), which records when Windows received each input and when each frame was actually displayed, and pairs them: `MsAllInputToPhotonLatency`. It doesn't include the mouse's own delay or the panel's response time; it's from "Windows has it" to "on screen".

**Concept: synthetic input.** A human can't click 190 times identically. `latency.ps1` injects left clicks with Windows' `SendInput`, every 120–250 ms at random, so clicks don't line up with the 144 Hz refresh and bias the result.

**Concept: always do a spike first.** Before building the tool, a 15-minute experiment checked the assumption it rests on. It held, with a twist: injected clicks appear in PresentMon's *all-input* column but never in its *click* column (that one apparently needs a hardware mouse button). Had the assumption failed, the plan said to stop. Building a tool around an unchecked assumption is how measurements end up meaningless.

**Concept: a visible effect.** `ATOM_LATENCY_FLASH=1` turns the frame black on each click, so the input has an unmistakable consequence.

**Concept: trust the tool first.** Measuring a configuration against itself gave −0.06 ms, so the method resolves differences far below the effects being studied.

**The first table** (144 Hz, Iris Xe): 3 frames in flight with vsync, the then-default, took **36 ms**; 2 frames 30 ms; uncapped 23–26 ms.

**And a surprise:** 2 frames now held 144 fps, where v0.0.10 measured 72. The engine's frame loop hadn't changed in that respect; the Intel driver had. The reason for v0.0.10's default was gone, which only a measurement could show.

**Code.** `Tools/Perf/latency.ps1`, `Input::WasLeftClicked`, `ATOM_LATENCY_FLASH` in `DemoApp`.

---

## 91. Where a click's time goes, inside the engine (M73)

**Concept: total vs breakdown.** PresentMon gives one number from outside. The engine can time the stages it controls, per click:
1. click → the start of the frame that reads it;
2. that frame's wait for a swapchain image;
3. → submit (the CPU's work done);
4. → GPU done.

The click's time comes from SDL's event timestamp, which on Windows is the OS's own receive time, on the same clock as `SDL_GetTicksNS`.

**Concept: fences.** To know when the GPU finished a frame, the engine submits it with a **fence**: a flag the GPU sets when it's done. Fencing every frame would cost something, so `LatencyProbe` fences only frames carrying a click, and only with `ATOM_LATENCY_LOG=1`. It polls them twice a frame, which makes "GPU done" an upper bound accurate to within one frame. The code says so.

**Concept: the observer effect.** Instrumentation can change what it measures. Checked both ways:
- with the log on, PresentMon's numbers were unchanged;
- with it off, the game was as fast as v0.0.11.

**Concept: don't subtract unrelated measurements.** PresentMon's total minus the engine's "GPU done" is *not* the display stage. Their clocks differ, and PresentMon credits an input to the next frame shown *after* Windows saw it, not to the frame that read it. The difference of two medians isn't the median of the differences either. So the two are compared as trends only.

**What it showed:**
- **With 3 frames,** the acquire hardly waited (0.04 ms). The latency sat in the GPU and presentation queue *after* submit, behind earlier frames.
- **With 2 frames,** the CPU waited ~6 ms at the acquire, after having read the input.

The same symptom ("too much latency") turned out to have two different mechanisms, and only the second could be fixed by moving the wait.

**Code.** `Engine/Core/LatencyProbe.*` (owned by the renderer), `Renderer::Render` (timing, the fenced submit), `Application::Run`, `Tests/LatencyProbeTests.cpp`.

---

## 92. Wait first, then read input (M74)

**The idea.** Every frame read input, updated the game, *then* waited inside rendering for a free swapchain image. An input arriving during that wait was read only by the *next* frame. Moving the wait to the top of the frame, before input, means input is read as late as possible, right before the work that uses it.

**Concept: check the API's real semantics.** SDL documents `SDL_WaitForGPUSwapchain` as waiting until "all presenting command buffers are finished", which sounds like a full drain that would destroy the pipeline's overlap. SDL 3.4.18's D3D12 source shows it waits on one fence: the next swapchain slot's, the same one the blocking acquire uses. A review of the plan raised exactly this, so it was treated as an experiment with a throughput check, not an assumed fix. The wait itself didn't change; only what came after it did.

**The result** (click to display, 144 Hz):

| | 3 frames | 2 frames |
|---|---|---|
| wait late | 36.1 ms | 30.2 ms |
| wait first | 36.0 ms | **24.1 ms** |

As M73 predicted, with 3 frames there was nothing to move. With 2 frames the 6 ms wait vanished from the input's path. All configurations stayed at 144 fps, and uncapped frame times didn't depend on the wait's place.

**The decision: 2 frames in flight + wait first** became the default, 12 ms (a third) quicker than v0.0.11.

**Concept: a safeguard for the old failure.** On v0.0.10's driver, 2 frames halved the frame rate. A machine like that would make the new default worse. So with vsync and the default 2, the renderer checks the frame interval once, after a warm-up. If the median exceeds 1.6 refresh periods (`NeedsThirdFrame`, unit-tested), it switches to 3 frames (`SDL_SetGPUAllowedFramesInFlight` at runtime: one stall) and logs why. A default chosen from one machine's data should defend itself on others.

**Code.** `GPUDevice::WaitForPresentSlot` and `SetFramesInFlight`, `NeedsThirdFrame`, `Renderer::WatchFramesInFlight`, the top of `Application::Run`'s loop; `ATOM_LATENCY_WAIT`, `ATOM_FRAMES_IN_FLIGHT`.

---

## 93. Measuring without administrator rights

**The problem.** PresentMon's event tracing needs elevated rights, so every run stopped at a Windows UAC prompt, which makes unattended measurement impossible.

**Concept: least privilege.** Grant only what's needed, and only where it's needed:
- **`-Mode Engine` (the default)** measures the engine's stages with no rights at all. It's the everyday and automation path. It doesn't see the display stage, which is fine for most changes.
- **`-Mode PresentMon`** is an explicit developer operation (before a milestone's PR, after presentation changes). It runs directly for an administrator or a member of Windows' **Performance Log Users** group, which exists precisely to allow tracing without being an administrator. Otherwise it explains, then asks for elevation *for that run only*. `-NoElevate` makes it exit instead, so automation never meets a dialog.

**Concept: well-known SIDs.** Group names are translated ("Usuarios del registro de rendimiento" on a Spanish Windows), but the group's security ID, `S-1-5-32-559`, is the same everywhere. Check identities by SID, not name.

**Concept: what a public tool must never do.** A script in a public repository shouldn't change a machine's security configuration, so it never adds anyone to a group (the README documents it as an optional manual step). It shouldn't fetch and run software with elevation either (a supply-chain risk), so it only *finds* an installed PresentMon. And it shouldn't over-diagnose: an empty latency column could mean injection, targeting, permissions or a metric change, so it lists those instead of claiming "you need administrator".

**Code.** `Tools/Perf/latency.ps1` (modes, `-NoElevate`, `-PresentMonPath`); README, Measuring performance.

---

## 94. Releasing 0.0.12 (M75)

**The release check, end to end:** with PresentMon, the new default **23.7 ms** against v0.0.11's **36.1 ms** (−12.4 ms paired), both 144 fps.

**Concept: attributing an A/B difference.** Uncapped, v0.0.12 measured +0.32 ms slower than v0.0.11 on the lakeshore (a tight range, so real). Repeated with both builds forced to 3 frames and the old wait order, the difference was −0.004 ms. So the new *code* costs nothing, and the +0.3 ms is the frames-in-flight default trading a little uncapped throughput for 12 ms of responsiveness at vsync. As in §82, an A/B that finds a difference isn't finished until a second one explains it.

**Correcting the record.** v0.0.10's estimate ("about 28 ms with 2 frames vs 21 ms with 3") was reasoning, and measured it was backwards: 3 frames was the slowest configuration. The CHANGELOG says so, and the old "not measured" note now points to 0.0.12. Reasoning about queues is a good way to find what to measure, not a substitute for measuring.

**Limits:** software measurement only, one machine, and PresentMon's input association. All of them are written down.

**Code.** `CHANGELOG.md` 0.0.12, Architecture §3 (the frame now starts with the wait) and §8.

---

## 95. A framework both games share (M76)

**The problem.** v0.0.13 adds a second game. Everything game-side that isn't rendering had grown inside the demo: the run log, the settings model and command line, the calibration decision. A second game would have had to copy it, and two copies drift apart.

**Concept: a framework layer.** Between an engine (rendering, audio, input, the frame loop) and a game (its rules and content) there is a middle layer: what *every game* built on the engine needs, but the engine itself shouldn't own. AtomEngine now has three layers:

| Layer | Namespace | Holds | Knows about |
|---|---|---|---|
| `AtomEngine` | `Atom` | renderer, audio, input, the frame loop | nothing above it |
| `AtomFramework` | `AtomFramework` | `RunLog`, `GameSettings` (model, command line, precedence), calibration, and since M82 the `--diagnostics` report | the engine |
| a game | `AtomGame`, `Drift` | its rules, content, presentation | the engine and the framework, never another game |

**Concept: move first, then change.** The files moved with `git mv` and their include paths stayed the same, so the move itself changed no behaviour and the existing tests proved it. Only then did the code start serving two games. `GameDiagnostics` and the test-script runner stayed in the demo: their vocabulary (levels, entities, dialogue) is the demo's, and moving them would have put one game's policy into the shared layer.

**Concept: the engine names no game.** A message like "AtomGame.exe needs a GPU" was a leak of the game into the engine. It now says "this game's executable". `RunLog` is named per game: `%APPDATA%\AtomEngine\<game>\logs\<game>.log`.

**Concept: packaging per game.** Every install rule belongs to a CMake **component** named like the game, so `cmake --install --component Drift` installs DRIFT's files and nothing of the demo's. The payload file names the game and its executable; `package.ps1 -Game <name>` and `verify.ps1` serve any game.

The decision is ADR-007 in Architecture: games share a framework, not each other's code.

**Code.** `Framework/` (`Platform/RunLog`, `Settings/GameSettings`, `Settings/Calibration`, `Diagnostics/DiagnosticsReport`), `Game/CMakeLists.txt` and `Games/Drift/CMakeLists.txt` (install components).

---

## 96. Porting a web game: DRIFT flies (M77)

**The brief: faithful first.** DRIFT started as a three.js web game. A port can be faithful (the same rules, look and sound) or a reinterpretation. Faithful first means: reproduce the original exactly, check it against the original, and only then change anything. Without that, "better" can't be told apart from "different".

**Concept: port the rules line by line.** `ship.js` and the path in `world.js` became `DriftLib` (`Flight.h`): the path from two sines and a cosine, the ship's input easing, the 11 m leash to the path, banking, squash and stretch, speed `38 + flow × 34`, boost ×1.7, and the spring camera (look-ahead, roll, FOV `70 + boost × 18 + flow × 6`, a seeded shake). **Every constant kept.** Unit tests pin the formulas, so a later "cleanup" that changes the feel fails a test.

**Concept: pure game logic.** `DriftLib` contains no rendering, audio or window code: it takes time and input and returns positions. That's what makes it unit-testable without a GPU, and it's why the executable (`DriftApp`) is thin: it reads input, steps the library, and draws what it says.

**Concept: content from the same source.** The original's models came from a Blender script, ported to `Tools/Blender/drift_models.py`. Three of the four models came out **byte-identical** to the web version's; the ship matched in size, structure and bounds (the web one had been made in a live session, not by the script).

**Concept: when the two engines differ.** Some things can't be copied, only approximated, and each approximation is written down:
- **Light units:** three.js and AtomEngine scale light intensity differently, so intensities were matched by eye.
- **Fog:** three.js's linear fog became the engine's exponential fog (0.0035/m), chosen to look the same over the distances that matter.
- **The engine's glow** stands in for the original's point light (M79 tunes it).

**Concept: an autopilot for testing.** A game you have to play by hand can't run in CI. `ATOM_DRIFT_SECONDS=N` flies the path automatically for N seconds and prints a summary; `Scenario.drift_fly` runs it. Since M78 `ATOM_DRIFT_SEED` fixes the course, so a run is reproducible.

**Code.** `Games/Drift/Flight.*`, `DriftApp.*`, `Tools/Blender/drift_models.py`, `Tests/DriftFlightTests.cpp`.

---

## 97. Rules as pure, seeded code (M78)

**The world.** `World` lays a segment every 80 m, up to 720 m ahead, and recycles what's 20 m behind. Each segment holds a ring on the path, a trail of 7 weaving orbs, and `2 + min(4, segment / 4)` rocks off the racing line, sized 0.8–2.6.

**Concept: flow, one value everything follows.** `Flow` is a number from 0 to 1. Passing a ring within 3.1 m adds 0.10 and missing it subtracts 0.08. An orb adds 0.025 and grows the **chain**; a rock subtracts 0.25, resets the chain and shakes the camera. Flow decays by 0.012 per second. Speed, FOV, bloom, music and the sky all read it.

**Concept: order is part of the rules.** The original checks rings, then orbs, then rocks. In a frame where the ship takes an orb *and* hits a rock, that order decides the outcome: the chain grows, then resets, and ends at 0. The port checks in the same order, and a test pins it. Details like this are where faithful ports quietly diverge.

**Concept: seeded randomness.** The course comes from a seeded generator, so the same seed gives the same course on every machine and every run. That makes the rules testable ("seed 7: rings pass") and the scenario deterministic in what it flies through.

**The HUD and the title.** The original's layout and font (Space Grotesk, under the SIL Open Font License, shipped with its licence): controls top left, the chain top right (popping ×1.25 when it changes), the flow bar at the bottom, and a "CLICK TO LAUNCH" title screen over an idle glide.

**Code.** `Games/Drift/World.*`, `Tests/DriftWorldTests.cpp`, `DriftApp::DrawHud`; `Scenario.drift_fly` in `Tests/CMakeLists.txt`.

---

## 98. Toon shading and inverted-hull outlines (M79)

**Concept: toon (cel) shading.** Ordinary diffuse light (Lambert, §13) varies smoothly with the angle between the surface and the light. Toon shading **quantises** it into a few flat bands, like a cartoon. three.js does this with a gradient map: a tiny texture read at `facing × 0.5 + 0.5` with nearest filtering. DRIFT's map has three texels, so three steps: 70, 160 and 255 out of 255.

**How AtomEngine does it: a shader variant.** `BasicToon.frag` defines `ATOM_TOON` and includes `Basic.frag` (the same pattern as rain in §69), replacing the sun's Lambert with the three steps. A **variant** rather than a branch: materials that aren't toon run the plain shader exactly, at no cost. The pipeline table became plain | rain | toon (72 entries), chosen per material (`Material::toon`).

**Concept: inverted-hull outlines.** The classic cheap outline:
1. Draw the mesh normally.
2. Draw it again, every vertex pushed out along its normal by the outline width, with **front faces culled**, in a flat dark colour.

Only the back faces of the slightly bigger copy survive, and they peek out around the silhouette: a rim. It costs one extra draw per outlined mesh and needs no post-processing. Its limits are known: it can show gaps at hard edges and doesn't outline internal creases. AtomEngine pushes vertices in model space (`Outline.vert`), fogs the colour like the scene so distant outlines fade, and skips shadow and reflection passes. DRIFT outlines the ship, rings and rocks (0.045, `#07060f`) but not the orbs, as the original does.

**Concept: compensating for a pipeline difference.** The original draws its stars and streaks unfogged; the engine's particles are always fogged. Rather than add a "no fog" particle mode, the port pre-compensates the colour on the CPU: if fog will blend a fraction *f* of the fog colour in, the particle is given `(target − fog·f) / (1 − f)`, so it arrives on screen at the target colour.

**Stars and streaks.** `SpeedField`: 1500 stars in a box that rides at 0.9 of the ship's lateral position, and 120 streaks, stretched over 0.08 s of travel, visible above 45 m/s. Both are particles with a 1×1 white atlas, matching the original's square points and thin lines.

**Bloom.** The engine's glow (§41) with the original's `UnrealBloomPass` threshold of 0.85 and strength `0.45 + flow × 0.7`. A different blur kernel, so close rather than identical.

**Measured:** the demo against v0.0.12 showed no cost (night street −0.26 ms, lakeshore −0.004 ms, both within noise).

**Code.** `Shaders/BasicToon.frag.hlsl`, the `ATOM_TOON` block in `Basic.frag.hlsl`, `Outline.vert/.frag.hlsl`, `Material::toon/outline/outlineColor`, `Games/Drift/SpeedField.*`.

---

## 99. A live synth on the audio thread (M80)

**The difference from §26.** The demo's sounds are synthesised *once* at start-up into buffers, then played back by the mixer. DRIFT's soundtrack is **generative**: it is synthesised *live*, note by note, and follows the flow (the filter opens, the hats come in, the arpeggio gets denser). That means synthesis runs on the audio thread, under a hard deadline.

**Concept: modelling WebAudio.** The original builds a WebAudio graph: oscillators, gain envelopes, a biquad filter, a delay. `Atom::Synth` reproduces that graph as plain DSP:
- **Voices:** a fixed pool of 160 (sine, triangle, saw, noise through its own filter, and a pitch sweep for kicks).
- **PolyBLEP saw:** a naive sawtooth has an instant jump that aliases (high harmonics fold back as audible junk). PolyBLEP smooths the sample or two around each jump with a small polynomial, removing most of the aliasing cheaply.
- **Envelopes:** a linear attack to the peak, then an exponential decay to 0.0001 at the note's duration, as WebAudio's `exponentialRampToValueAtTime`. Computing `pow()` per sample was too slow, so the ramp is a constant per-sample **ratio**: multiplying by *r* each sample is an exponential curve.
- **The filtered bus:** an RBJ low-pass (§26's biquad) whose cutoff approaches its target like WebAudio's `setTargetAtTime`. A trap found here: WebAudio specifies a low-pass's Q **in decibels**, so the original's 2 dB becomes a linear 1.26.
- **A feedback delay and a master gain** with its own time constant (mute fades instead of clicking).

**Concept: real-time rules on the audio thread.** The device asks for audio every few milliseconds; miss that and the sound breaks up. So on the audio thread there are **no locks and no allocation**: nothing that can wait on another thread or on the memory allocator. Everything is allocated when the synth is built.

**Concept: a lock-free command queue.** The game must still tell the music things (a pickup, a flow change, mute). `SynthStream` uses a **single-producer, single-consumer (SPSC) queue**: one thread only writes, the other only reads, and atomic indices make that safe without a lock. Contrast §26, where the mixer shares its voice list under the stream lock; that works for short edits, but a synth running every block can't risk waiting.

**Concept: sample-accurate scheduling.** The music is a `SynthSequencer` (the port of `audio.js`). Each 256-frame block, it's asked which notes start inside that block and at which sample, so notes land exactly on the beat whatever the block boundaries. 96 BPM eighth notes, D minor pads, a kick on the quarters, hats above flow 0.15, an arpeggio whose probability grows with flow, the filter opening toward `500 + flow² × 6000` Hz, and pickups climbing a pentatonic scale with the chain.

**Concept: testing sound without listening.** `RenderOffline` runs the same code without a device. Tests check a sine's pitch from its zero crossings, the envelope's rise and decay, the low-pass's attenuation two octaves above the cutoff, the delay's repeat time, and that four seconds of the soundtrack are finite, unclipped and audible. Whether it *sounds like the original* is a listening check, which only a person can do.

**Speed:** more than 10× real time in Release.

**Code.** `Engine/Audio/Synth.*`, `SynthStream.*` (`SynthSequencer`), `Games/Drift/Music.*`, `Tests/SynthTests.cpp`.

---

## 100. Validating what changed (M81)

**The cost that grew.** Each milestone ran the full matrix: every scenario in Debug and Release, plus a CI-configuration build. About 20 minutes, even for a change to one folder. Validation that slow gets skipped or resented.

**Concept: test selection by change.** Map each changed file to the checks that can catch its mistakes, and run only those. Large codebases do this with dependency graphs; AtomEngine does it with a reviewable table.

**How AtomEngine does it.** `check.ps1 -Level changed` collects the files the branch touched since it left the base (committed, uncommitted and untracked) and maps each through `Tools/Dev/changed.psd1`:
- **First matching rule wins,** and each file is printed with the rule that matched, so the choice is visible.
- **Docs only:** nothing to build. DRIFT's sources: `drift_fly` and `drift_diagnostics`. Shaders and the renderer: the rendering set. A CMake or packaging change also stages and verifies a package.
- **Never silently nothing:** a file no rule matches is flagged and gets a broad fallback set.
- **No stale entries:** every scenario named in the table must exist (`ctest -N`), so a renamed scenario fails the check instead of quietly running less.
- `-DryRun` prints the plan without building.

**The rules that came with it:** per milestone, `-Level changed`; `-Level full` **once per version**, before the release commit; no local CI-configuration build (CI runs it on the pull request); `ab.ps1` only when a milestone touches performance.

**Concept: a table is data, not logic.** The mapping lives in a `.psd1` data file, not in the script, so reviewing or changing which tests guard which folder is a one-line diff anyone can read.

**Code.** `Tools/Dev/check.ps1`, `Tools/Dev/changed.psd1`, `CLAUDE.md`, the README's Development workflow.

---

## 101. Lean packages, one overlay, one report (M82)

**What changed since §88.** v0.0.11 shipped every developer facility, inert unless used ("nothing stripped"). With two games the facilities were uneven (the demo had a debug overlay, a performance log and a test harness; DRIFT had almost none), and a player could reach tools meant for developers. M82 reverses the policy.

**Concept: a compile-time switch.** A distribution build (`-DATOM_DISTRIBUTION=ON`) now also defines `ATOM_DEV_TOOLS=0`. Two entry points are gated:
- **`Atom::DevSwitch(name)`** replaces every direct read of an `ATOM_*` environment variable. In a package it returns nothing, so no switch can change behaviour.
- **The developer tools** (ImGui: the F10 panels and the F1 overlay) are never initialised.

The trade-off is written down (ADR-008): the shipped executable is no longer the exact tested one. The switch gates only *entry points*, not game code paths, which keeps that difference small. The run log's first lines say which build it is: "Developer tools: on" or "off (distribution build)".

**Players keep** the log, the start-failure message box, `--diagnostics`, and the settings flags (`--gpu`, `--quality`, `--reset-settings`…).

**Concept: a guard against the wrong build.** `package.ps1`'s smoke run checks the packaged game's output for "Developer tools: off" and refuses the package otherwise. A development build can't be shipped by mistake.

**One F1 overlay for every game.** The engine draws a corner window with ImGui that never takes the mouse or keyboard. The engine adds its lines first each frame (frame time, scene size and MSAA, draws and triangles, the adapter, present mode and frames in flight, the latency readout), then the game appends its own through `AddOverlayLine`. The demo's old overlay (drawn with the game's UI renderer) folded into it; DRIFT adds speed, flow, chain and totals.

**One `--diagnostics` report.** The report (§79) moved to the framework: `DiagnosticsReport` writes the shared facts (SDL, the adapter and any fallback, presentation, display, power), and each game appends its own lines. The demo's output is unchanged (`Scenario.diagnostics` passes as before). DRIFT gained the framework's command line, so `--gpu` and `--diagnostics` work there too (`Scenario.drift_diagnostics`).

**Code.** `Engine/Core/DevSwitch.h`, `Engine/Debug/DevTools.*` (`AddOverlayLine`), `Application::AddEngineOverlayLines`, `Framework/Diagnostics/DiagnosticsReport.*`, `Tools/Dist/package.ps1`, Architecture §9 and ADR-008.

---

## 102. Two old bugs a second game exposed

A second game uses the engine differently, and that shakes out bugs the first one never triggered or that everyone had stopped seeing.

### Double precision in a Debug shader

**The symptom.** DRIFT's Debug build looked broken, and the log repeated `D3D12 ERROR: CreatePixelShader: Shader uses double precision float ops which are not supported on the current device`, then "Failed to create scene pipeline". Release was fine.

**Concept: literal types in HLSL.** A number like `1.0` with no suffix is a *literal*, whose type is decided by context. The toon ramp chose between bare literals with nested `? :`, so the whole expression stayed a literal, and dxc typed it **double**. With optimisation (`-O3`) the doubles were folded away; Debug builds compile with `-Od`, so they stayed in the shader. The Iris Xe has **no 64-bit floating point in shaders** (`FP64` is optional in D3D12), so the device refused the pipeline.

**The fix:** `f` suffixes (`1.0f / 3.0f`) make every value a 32-bit float. Every shader was recompiled both ways and scanned: none uses doubles now.

**Why the tests passed anyway.** `drift_fly` checked only the autopilot's summary, which prints even with the pipeline missing. Scenarios now also fail on "Failed to create … pipeline". **Lesson:** a test that checks only the happy output can pass while the program is visibly broken; check for the failure signatures too.

### Blurry text since the first font

**The symptom.** UI text had always looked soft, "not clean", in every game.

**The cause: three reasonable decisions combined.**
1. The font atlas was created through the general `CreateTexture`, which **always builds mipmaps** (§11).
2. The UI sampler is trilinear.
3. Glyphs are baked with **2× horizontal oversampling** (§28), so each screen pixel spans two atlas texels across.

The GPU picks the mip level from the texel-to-pixel ratio: two texels per pixel means **mip level 1**, the half-resolution, box-filtered copy. Text had been drawn from an atlas half its baked size since v0.0.2.

**The fix:** `CreateTexture` takes a `mipmaps` flag, and fonts pass `false`. An atlas drawn at its own size gains nothing from smaller levels; they only blur it and mix neighbouring glyphs.

**Concept: measure the fix, and check the measurement.** Two identical scripted runs of the demo differ by at most 3 levels per pixel in the hint line; old against new differed by 31 on average, and enlarged crops showed the halos gone and the letters' counters open. The first comparison had shown *no* change at all, because the "new" build had never been built (a missing `cmake` on the shell's path, hidden by an output filter). An executable's timestamp gave it away. **Lesson:** a comparison that shows exactly zero difference is as suspicious as one that shows a huge one; confirm that both sides really are what you think.

**Concept: DPI awareness, ruled out.** At 125 % Windows display scaling, a program that isn't DPI-aware is drawn at 100 % and stretched by Windows (blurring everything). The window was checked from outside with the Win32 API: per-monitor DPI-aware, 1280×720 physical pixels, matching the swapchain. A query from a DPI-*unaware* process returned scaled numbers (1024×576) at first, a trap of its own.

**Code.** `Shaders/Basic.frag.hlsl` (the `ATOM_TOON` block), `Tests/CMakeLists.txt` (fail expressions), `Engine/UI/Font.cpp`, `Texture::Create` (`mipmaps`).

---

## 103. Releasing 0.0.13 (M83)

**DRIFT's package.** Its own install component: `Drift.exe`, `SDL3.dll`, the engine's shaders, `Assets/Drift/`, the licences (Space Grotesk's OFL among the third-party notices) and a players' README. `package.ps1 -Game Drift` makes `Drift-v0.0.13-win64.zip`: **1.9 MB, 35 files**, against the demo's 16 MB. CI now packages both games.

**The numbers** (Release, plugged in, 144 Hz panel):
- **DRIFT:** median frame 6.95 ms (144 fps), p95 7.83 ms. Click to GPU done (`latency.ps1 -Mode Engine`): median 20.7 ms, p95 27.5 ms.
- **The demo against v0.0.12** (`ab.ps1`, 8 rounds): night street −0.22 ms (−1.05..+0.38), lakeshore −0.002 ms (−0.06..+0.23). Both ranges straddle zero: the engine work for DRIFT costs the demo nothing measurable.
- `check.ps1 -Level full`: 20/20 in Debug and in Release, once, before the release commit.

**Concept: a baseline is not a comparison.** DRIFT's latency came from only 8 blocks of samples, and its rounds ranged 18.6–27.5 ms. The demo measured 17.5 ms in the same session, but with that much noise the difference can't be called real. DRIFT's figure is recorded as a baseline for its next version, not as "DRIFT is slower". DRIFT also had no frame-rate report; its autopilot summary now includes one, which ruled out the first suspicion (that DRIFT ran at 72 fps).

**Concept: a fidelity sheet.** Every part of the original (rules, look, sound) against the port, with *how* each was checked: a unit test pinning the constant, the scenario, by eye side by side, or by ear. The deliberate differences are listed too: text 4 px larger than the original's CSS (a 1280×720 window at 125 % scaling made 12 px small), a window instead of a browser page, a hidden cursor. The one check a test can't do, whether the music sounds like the original, is the player's listening check.

**A correction made at release: hidden is not captured.** Asked to hide DRIFT's cursor "like the demo", the first change copied the demo's mouse capture (relative mouse mode). But the demo uses the mouse for *look* (deltas), while DRIFT steers partly by where the pointer *is* in the window (`mouse.x × 0.35`, as in `ship.js`). Relative mode replaces that position with a virtual, clamped one. DRIFT now only hides the cursor (`SDL_HideCursor`). **Lesson:** copy a behaviour's purpose, not its implementation.

**Code.** `Games/Drift/CMakeLists.txt` (install rules), `Games/Drift/README.txt.in`, `.github/workflows/ci.yml`, `Tools/Perf/latency.ps1` (`-Game .../Drift.exe`), `CHANGELOG.md` 0.0.13, Architecture §1 and ADR-007/008.

---

## 104. Anatomy of a frame and what it costs

Measured in Release, vsync off, looking down the street, 1280×720, Iris Xe (laptop numbers — expect ±10 % noise):

| Stage | What it does | ≈ Cost |
|---|---|---|
| Base scene | 177 draws, textures, lighting, 4× MSAA | ~1.4 ms |
| 4× MSAA (vs 1×) | multisampled colour/depth + resolve | ~0.2–0.3 ms |
| Fog | math in the scene shader | ~0.04 ms |
| Shadow pass | 2048² depth, ~100 draws | ~1.0 ms |
| Post (tonemap, grade, grain, vignette) | one fullscreen pass | ~0.1 ms |
| Particles | ~200 instanced billboards, sort + upload | ~0.15 ms |
| **Total** | | **~2.6 ms ≈ 385 fps** |

Takeaways: the shadow pass is the biggest single cost; screen-space math (fog, post) is almost free; a 144 Hz budget (6.9 ms) leaves plenty of room.

**v0.0.2, per level** (same machine and settings): street ≈ 2.6 ms, shrine grounds ≈ 1.8 ms, interior ≈ 1.5 ms. The UI overlay (a few batched quads) is negligible. The interior is cheapest because the level turns shadows off and has few draws.

**v0.0.3** adds more vertices (tessellation for the bake, §34), a lightmap sample in the interior, alpha-tested cards in the scene and shadow passes, decals, and sway in the vertex shader: street ≈ 3.5 ms, shrine grounds ≈ 2.9 ms, interior ≈ 1.5 ms, windmill field ≈ 2.2 ms.

**v0.0.4**, per level (Release, IMMEDIATE present mode, 1280×720, Iris Xe, default spawn, averaged over 4000+ frames):

| Level | Frame | Draws (near / mid / far) | Shadow draws |
|---|---|---|---|
| street | ≈ 3.9 ms | 238 / 0 / 0 | 114 |
| shrine grounds | ≈ 3.4 ms | 104 / 0 / 0 | 137 |
| machiya interior | ≈ 2.0 ms | 11 / 0 / 0 | 0 |
| windmill field | ≈ 2.5 ms | 88 / 0 / 0 | 109 |
| night street | ≈ 3.5 ms | 52 / 4 / 4 | 0 |
| pachinko hall | ≈ 2.6 ms | 13 / 0 / 0 | 0 |

- **Per layer**: on the night street, without the mid and far layers the frame is ≈ 2.8 ms, so shells, skyline and seven impostors cost ≈ 0.65 ms for 8 draws, none in the shadow pass.
- **The older levels** are 0.3–0.5 ms slower than in v0.0.3. The likeliest cause is the glow pass (§41), which now runs in every level; it wasn't measured separately.
- **v0.0.5**: the pachinko hall ≈ 2.8 ms walking around (its three screens now simulate the real game), ≈ 3.5 ms seated and playing (the 2D game fullscreen with the hall still drawn underneath); the night street ≈ 3.3 ms, within noise of v0.0.4.
- **v0.0.6** (measured side by side with a 0.0.5 build, §57): the character lab ≈ 2.35 ms in the viewer, ≈ 2.4 ms with the skeleton, weights and state machine on, ≈ 2.2 ms driving (8 draws, 7 in the shadow pass; the skinned character is 3 of them). The other levels are unchanged: the interior 2.07 ms (0.0.5: 2.06), the pachinko hall 2.85 (2.99 that day), the night street ≈ 3.1.
- **v0.0.7**, the first measured the M46 way (`ATOM_PERF_LOG` medians of 240-frame blocks, p95 in brackets, plugged in, 144 Hz laptop screen, uncapped): the passage 1.89 ms (4.16) with the flashlight off, 2.32 ms (4.47) on; the machiya interior 2.04 (3.57); the character lab 2.21 (3.38); the pachinko hall 3.04 (9.06); the night street 3.94 (10.38); the street 4.23 (8.88). These are the baseline for later versions; earlier numbers above were averages and aren't directly comparable.
- **v0.0.8** (same method; Balanced power plan): the lakeshore from the beach per preset - clear day, overcast and fog 3.53 ms, rain 3.90, sunset 3.32, night 3.58. Against v0.0.7, interleaved builds (§69): the street −0.035 ms, the windmill field −0.007, the night street +0.15 (noisy), all within resolution. What the lake's features cost is in §69.
- **v0.0.9** adds no feature: against v0.0.8 (interleaved builds) the street +0.027 ms and the lakeshore +0.024 ms, within resolution. Level loads now log their own timing (§74), e.g. the night street 173 ms, mostly GPU upload.
- **v0.0.10**, against v0.0.9 (interleaved builds, §82): the night street −0.51 ms, the lakeshore −0.55 ms, from the third frame in flight (§81); with vsync on the 144 Hz panel, 72 → 144 fps. Baselines measured with two frames in flight don't compare with v0.0.10's.
- **v0.0.11** adds no rendering work: against v0.0.10 (interleaved builds, §89) the night street +0.07 ms and the lakeshore +0.004 ms, within noise. The static C++ runtime and the new log cost nothing measurable.
- **v0.0.12** (§94): click to display 36 → 24 ms at 144 fps. Uncapped, the 2-frame default is ~0.3 ms slower per frame than 0.0.11's 3 (the code alone: −0.004 ms). Baselines across 0.0.10–0.0.11 and 0.0.12 differ in frames in flight; set `ATOM_FRAMES_IN_FLIGHT` alike to compare code.
- **v0.0.13** (§103): against v0.0.12 (interleaved builds) the night street −0.22 ms and the lakeshore −0.002 ms, within noise: toon shading, outlines and the synth cost the demo nothing. DRIFT itself, with vsync on the 144 Hz panel: median frame 6.95 ms, p95 7.83 ms (its autopilot's summary).
- **The night levels** draw nothing in the shadow pass (night lighting turns sun shadows off); their extra work is glow, live lights and, in the hall, the render-texture screens.

---

## 105. Build system and project layout

- **CMake** (≥ 3.25), C++20. Targets: `AtomEngine` (static lib), `AtomFramework` (what every game shares, §95), `AtomGameLib` (the demo's gameplay as a static lib), `AtomGame` (exe), `DriftLib` and `Drift` (the second game, §96), `AtomTests` (doctest unit tests, linking both game libraries), `AtomShaders` (custom target compiling HLSL). Each `Tests/Scenarios/*.atomtest` is a ctest test that runs `AtomGame` with `ATOM_TEST_SCRIPT` (label `scenario`); `drift_fly` and `drift_diagnostics` run `Drift`.
- **Dependencies as git submodules, pinned**: SDL 3.4.18, GLM 1.0.1, cgltf v1.15, stb, nlohmann/json 3.12.0, doctest 2.5.3, Dear ImGui 1.92.9 (built as the `imgui` static library with its SDL3 and SDL_GPU backends).
- **Build options** (§71): `ATOM_BUILD_GAME` (the executable, shaders and scenarios; off, no `dxc` needed), `ATOM_BUILD_TESTS`, `ATOM_BUILD_PRESENTATION_PROBE` (`PresentationProbe` and `SwapchainMatrix`, §83), `ATOM_DISTRIBUTION` (the windowed player build, §85). The runtime asset payload is the folder list in `Game/CMakeLists.txt` (§73), split into shipped and development-only folders (§86). The C++ runtime is linked statically (§84). CMake 3.26 or later.
- **Distribution** (§86): `pwsh Tools/Dist/package.ps1 [-Game AtomGame|Drift]` builds `build-dist/`, stages `Dist/<Game>/` with `cmake --install --component <Game>`, verifies it, smoke-runs it (refusing a build with developer tools, §101) and zips `Dist/<Game>-v<version>-win64.zip`. A distribution build defines `ATOM_DEV_TOOLS=0`.
- **CI** (§72, §87): `.github/workflows/ci.yml`, on pull requests and pushes to master: the unit tests, and the package check for both games (no artifact kept). **Validation** (§75, §100): `Tools/Dev/check.ps1 -Level changed` per milestone (the table in `Tools/Dev/changed.psd1`), `-Level full` once per version.
- **Shader variants** (§69, §98): `BasicRain.frag` and `BasicToon.frag` include `Basic.frag` with a define; a change to `Basic.frag` rebuilds all three. Write float literals with `f` (§102).
- **Version**: `project(VERSION …)` in CMake becomes `ATOM_VERSION`, shown in the log and the window title.
- Post-build step copies `Assets/` next to the executable; shaders are compiled into `bin/<Config>/shaders/`.
- Visual Studio's built-in HLSL (FXC) is disabled on `.hlsl`/`.hlsli` files (`VS_TOOL_OVERRIDE None`) so only dxc compiles them; `Common.hlsli` is a dependency of every shader.
- `NoTrack/` and `build/` are git-ignored; this manual lives in `docs/`.
- **Machines**: `Assets/Machines/*.json` (schema `machine.schema.json`), laid out by `Tools/Machines/*_layout.py`; plain JSON, read at runtime, no Blender needed.
- **Documentation captures**: `pwsh Tools/Docs/capture_first_render.ps1` renders the first-render shots and GIFs into `out/img/` (§47); `capture_character_lab.ps1` does the lab's (§57).
- **Harness commands** added since v0.0.4: `screenshot`, `capture`, `pan`, `set`, `hold_action`, `press_action`, `expect_counter`, `set_counter`; in v0.0.6 `clip`, `expect_clip`, `set_param`, `expect_state`, and `set skeleton|weights|bind|pause|mode|blend`; in v0.0.7 `set devtools|flashlight|spot|particles|msaa…`, `expect_lit`, `bench`, `expect_bench_under`; in v0.0.8 `expect_water`, `environment`, `expect_environment`, `expect_particles`, `timeout`, and `set water|rain|weather|reflection`; in v0.0.9 `expect_reflection`, and `bench` always ends on its second value; in v0.0.10 `expect_quality`, `move_window`, `set quality`.
- **Game command line** (v0.0.10, §77): `--gpu low-power|high-performance`, `--quality auto|low|balanced|high`, `--calibrate`, `--diagnostics <file>`, `--no-settings`, `--reset-settings`. Settings are saved in `%APPDATA%\AtomEngine\AtomGame\settings.json` (§78); exit code 3 means the renderer failed mid-run (§80). After a start-up fallback, the diagnostics report adds `gpu.fallback.adapter|stage|error` (§83); `Scenario.gpu_fallback` runs a high-performance request on any machine.
- **Doctor** (§79): `pwsh Tools/Dev/doctor.ps1 [-Configure] [-GamePath <exe>]`.
- **Asset build options** (after `--`): `--no-cache` re-bakes every lightmap, `--gpu` bakes on the NVIDIA GPU for light tuning (§46), `--no-export` stops after the lint.
- **Environment switches** for development: `ATOM_VSYNC=0` (uncapped frame rate), `ATOM_AUDIO_CAPTURE=file.wav` (record the mix), `ATOM_START_LEVEL=<level>[:<spawn>]` (start anywhere), `ATOM_TEST_SCRIPT=<file>` (run a scenario, exit 0/1), `ATOM_ASSET_ROOT=<repo>` (read the source tree and hot-reload, §39); since v0.0.7 `ATOM_PRESENT=immediate`, `ATOM_PERF_LOG=1`, `ATOM_PERF_BLOCK=<frames>`, `ATOM_PERF_CSV=<file>` (§63); since v0.0.9 `ATOM_ASSET_LOG=<file>` (§73); since v0.0.10 `ATOM_GPU`, `ATOM_QUALITY` (§76), `ATOM_FRAMES_IN_FLIGHT`, `ATOM_CALIBRATE_SECONDS` (§81), `ATOM_WINDOW_POSITION=x,y`, `ATOM_SIMULATE_SWAPCHAIN_LOSS=<seconds>` (§80); since v0.0.12 `ATOM_LATENCY_LOG=1`, `ATOM_LATENCY_FLASH=1` (§90–§91), `ATOM_LATENCY_WAIT=late` (§92), `ATOM_PRESENTMON=<exe>` (§93); since v0.0.13, for DRIFT, `ATOM_DRIFT_SECONDS=<n>` (autopilot, then a summary with the frame rate), `ATOM_DRIFT_SEED`, `ATOM_DRIFT_TITLE=1`, `ATOM_DRIFT_CAPTURE=<png>` (§96–§97). All are read through `Atom::DevSwitch` and ignored in a package (§101).
- **Assets**: `blender -b --factory-startup -P Tools/Blender/build_assets.py` rebuilds every glb, the lightmaps (skipping unchanged ones, §46) and the markers; the game build copies `Assets/` next to the executable, so rebuild the game (or use `ATOM_ASSET_ROOT`) to see new assets.
- **Running tests**: `ctest --test-dir build -C Release` (all), `-LE scenario` (unit tests only, no GPU), `-L scenario` (in-game).

```
Engine/  Assets/ Audio/ Core/ Debug/ (ImGui) Physics/ Platform/ Renderer/ Scene/ UI/
         Renderer/GPUDevice (device, window claim, presentation, fallback)
         Audio/Synth, SynthStream (the live synth); Core/DevSwitch (development switches)
Framework/ Platform/RunLog, Settings/ (GameSettings, Calibration),
         Diagnostics/DiagnosticsReport (--diagnostics, shared)
Games/Drift/ DriftLib: Flight, World, SpeedField, Music; DriftApp, Main;
         Assets/ (ship, ring, orb, rock .glb; Fonts/SpaceGrotesk + OFL); README.txt.in
Game/    DemoApp, PlayerController, AudioScape, SoundSynth,
         Atmosphere, UneaseDirector, Main
         World/ Interaction/ Dialogue/ Level/ Testing/
         Input/ (contexts) Pachinko/ (physics, playfield, rules, game, machine mode)
         Character/ (LabViewer, Animator, SpringArm); DemoAppLab (the lab's modes)
         Flashlight; DemoAppDevTools (the ImGui panels)
         Environment/ (EnvironmentController); Level/Environment (state, Blend)
         DemoAppCalibration (settings and the log moved to Framework/ in v0.0.13)
Shaders/ Basic, Shadow, Particle, Fullscreen, Post, UI, Sky, Halo,
         GlowBright, GlowBlur, Skinned, ShadowSkinned, Beam, Water, BasicRain,
         BasicToon, Outline (.hlsl)
         + Common.hlsli, Sway.hlsli, Skinning.hlsli, SkyGradient.hlsli
Tools/Machines/  playfield layout scripts; Tools/Docs/  captures, GIF maker
Tools/Perf/  ab.ps1 (build A/B, hang guard), latency.ps1 (input latency: Engine / PresentMon),
             benchmark scenarios (lights, water and weather)
Tools/Dev/  check.ps1 (validation: changed, quick, feature, full), changed.psd1 (its table), doctor.ps1, common.ps1
Tools/Dist/  package.ps1 (the package), verify.ps1 (its check), README.txt.in (players' README)
Tools/PresentationProbe/  PresentationProbe (SDL or raw D3D12, window moves),
                          SwapchainMatrix (every swapchain kind, D3D11/D3D12, every adapter)
Tools/Blender/  kit + street + levels + city + night + pachinko + lab + lint
                + lakeshore + bakes (vertex, lightmap, cached) + impostors + markers
                + export
Assets/  Kit/ Street/ Shrine/ Interior/ Fields/ City/ Night/ Pachinko/ Lab/ Passage/ Lakeshore/
         Data/ (flashlight.json), Environments/ (weather presets)
         ThirdParty/ (used as they came: the lab's character, credits README)
         Sky/ (.glb, lightmap .png, impostor atlas),
         Levels/*.json (+ *.markers.json), Dialogue/*.json,
         Schemas/*.schema.json, Fonts/
docs/    this manual and Architecture.md
.github/ CI workflow; CLAUDE.md: working rules for agents
Tests/   unit tests (*.cpp), Scenarios/*.atomtest
external/ SDL glm cgltf stb json doctest imgui
```

**Controls:** WASD, Shift jog, mouse look, **E interact** (in dialogue: continue/confirm; W/S or 1–4 choose), Esc release/quit · F1 debug overlay · F2 render scale · F3 baked light · F4 MSAA · F5 fog · F6 shadows · F7 post look · F8 particles · F9 unease moments · F10 developer tools · **F flashlight** (once found) · M mute.

**DRIFT:** click to launch · WASD, arrows or the pointer's position steer · Shift boost · M mute · Esc quit · (development builds: F1 overlay, F10 tools).

**Character lab:** arrows / mouse orbit, wheel zoom · 1–4 clip · 5 blend (Z/X slider) · 6 state machine · −/+ speed · Space pause · . step · B bind pose · K skeleton · W weights · Tab drive (WASD, Shift run, Space jump, Tab back).

---

## 106. Glossary

- **AABB** — axis-aligned bounding box (min/max corners).
- **ACES** — a film-industry colour standard; its filmic tonemapping curve is widely approximated in games.
- **Alpha blending** — mixing a transparent colour over what's behind it by its alpha; order-dependent.
- **Alpha dilation** — filling transparent texels with nearby opaque colour so filtering doesn't pull dark fringes into cut-out edges.
- **Alpha test / alpha mask** — drawing a pixel or discarding it by comparing its alpha with a cutoff; no sorting needed.
- **Alpha-to-coverage** — with MSAA, turning a pixel's alpha into how many of its samples are covered: soft cut-out edges without sorting.
- **ADR (architecture decision record)** — a short note of a decision, why it was taken, and when to revisit it.
- **Animation event** — a named moment of a clip (a foot touching down) that fires when playback crosses it.
- **Animation state machine** — states (clips or blends) and transitions on parameters, as data; gameplay only sets the parameters.
- **Atlas** — several small images packed into one texture.
- **Calibration** — measuring the machine on fixed views to choose a quality tier, instead of guessing from hardware names (§81).
- **Discrete / integrated GPU** — a separate graphics chip with its own memory / one built into the CPU, sharing system memory.
- **Frames in flight** — how many frames the CPU may queue ahead of the GPU; more keeps the GPU busy, but each full slot delays input by a frame (§81).
- **Artifact (CI)** — a file a CI run keeps for a while; for developers, not players (compare release, §87).
- **Clean machine** — a computer with none of the developer's tools or runtimes, the only honest test of a package (§88).
- **Code signing** — a certificate-backed signature on an executable; without it, Windows SmartScreen warns about downloads (§89).
- **Install rules** — CMake's description of what a build installs and where; `cmake --install` copies exactly that (§86).
- **Staging** — assembling a package in a fresh folder before verifying and archiving it.
- **Static / dynamic linking (C++ runtime)** — copying the runtime into the executable (`/MT`) versus loading it from DLLs a machine must have (`/MD`, §84).
- **Subsystem (Windows)** — an executable's declared kind: console (gets a console window) or windows (no console, starts at `WinMain`, §85).
- **Tee** — duplicating one output stream into two destinations, like the plumbing T (§85).
- **Cross-adapter presentation** — one GPU renders and another shows the image; Windows copies each frame between them (hybrid laptops, §83).
- **Flip model / bitblt model** — the two kinds of DXGI swapchain: flip hands whole buffers to the compositor (required by D3D12, modern D3D11); bitblt has the compositor copy from one buffer (old D3D11 games).
- **LUID** — a locally unique ID Windows gives each adapter; GPU performance counters name adapters by it.
- **Optimus profile** — an NVIDIA driver setting that sends a known D3D9–D3D11 or OpenGL program to the dGPU behind the Intel adapter; D3D12 has no equivalent (§83).
- **MUX (display multiplexer)** — a switch that can wire a laptop's panel to the discrete GPU; *muxless* laptops (classic Optimus) can't, so the integrated GPU always drives the panel (§83).
- **ETW (Event Tracing for Windows)** — the operating system's built-in event recording, which tools like PresentMon read; starting a trace needs an administrator or a Performance Log Users member (§93).
- **Fence (GPU)** — a flag the GPU sets when the work submitted before it is done; how the CPU learns that a frame finished (§91).
- **Least privilege** — granting only the rights a task needs, only where it needs them (§93).
- **PresentMon** — Intel's open-source tool that reports, from ETW, when frames are presented and displayed, and input-to-display latency (§90).
- **SID (security identifier)** — the fixed ID of a Windows account or group, the same in every language (e.g. `S-1-5-32-559`, §93).
- **Spike** — a short experiment that checks the assumption a larger piece of work rests on, before building it (§90).
- **Synthetic input** — input injected by a program (`SendInput`) instead of a person, so measurements are repeatable (§90).
- **Double / triple buffering** — a swapchain of two / three images; with vsync, two can force every other refresh to be missed (§81).
- **Input latency** — the time from an input (a mouse move) to its result on screen; queue depth, vsync and the display all add to it.
- **Hybrid graphics (Optimus)** — a laptop with an integrated and a discrete GPU, where the discrete one renders and the integrated one usually drives the built-in screen (§80).
- **Microsoft Basic Display Driver** — Windows' generic software display driver, used when no vendor driver is active; slow and fixed-mode.
- **Precedence** — the order in which setting sources override each other: command line > environment > saved > defaults (§76).
- **Quality tier / preset** — a named bundle of render settings (High, Balanced, Low); Custom when single switches differ (§76).
- **A/B measurement** — timing the old and the new build alternately on the same machine, so drift between runs cancels out.
- **Attachment** — a texture a render pass draws into (colour, depth).
- **Attenuation** — a sound getting quieter with distance.
- **Baked lighting** — light computed offline and stored (in vertex colours or lightmaps) instead of computed every frame.
- **Chart (UV)** — a connected piece of a model laid flat in a UV set; lightmap charts must not overlap.
- **Bandwidth** — bytes moved between GPU and memory per second; the usual bottleneck on integrated GPUs.
- **Bloom / glow** — bright parts of the image blurred and added back, imitating light scattering in the eye and lens.
- **Broad phase** — a cheap first test that narrows down which objects or triangles could collide, before the exact test.
- **Broad phase / narrow phase** — first cheaply list what *might* touch (a grid), then test those exactly.
- **Bind pose** — the pose a mesh was attached to its skeleton in; skinning measures every movement from it.
- **Beer–Lambert law** — light through a medium decays as e^(−density·distance); the basis of exponential fog.
- **Beam (faked)** — a visible light shaft drawn as crossed additive planes along a spot's axis instead of volumetric ray marching.
- **Blinn-Phong** — a specular highlight from the angle between the normal and the half vector (halfway between light and eye), raised to a shininess exponent.
- **Central differences** — estimating a slope from samples on either side, (h(x+e) − h(x−e)) / 2e; the water's normals from its noise height.
- **Billboard** — a quad that always faces the camera.
- **Biquad** — a standard 2nd-order digital filter (low/high/band-pass).
- **Catenary** — the sag curve of a hanging cable (approximated by a parabola for the power lines).
- **Cell** — a connected area of a level (a stretch of street, an alley); only the player's cell and its neighbours are drawn.
- **Chunk** — a piece of a level drawn and culled as a whole, with a distance layer and a shadow flag.
- **Comb filter** — a delay fed back into itself: an echo that repeats and decays; the building block of classic reverbs.
- **Contact normal** — the direction a colliding body is pushed out along; the approaching velocity along it is removed and partly returned.
- **Counter (game state)** — a named number that outlives levels (tokens, balls), next to the yes/no flags.
- **Crossfade** — blending from one animation to the next over a short time, so the pose doesn't pop.
- **Cyclorama** — a studio backdrop curving seamlessly up from the floor, with no visible corner.
- **Characterization test** — a test that pins what code currently does, written before moving it, so the move can be proven not to change it.
- **CI (continuous integration)** — building and testing every pushed change on a clean machine.
- **Clip (animation)** — a named set of keyframed channels that move a model's nodes.
- **Clip space / NDC** — coordinates after projection / after dividing by w.
- **Command buffer** — recorded GPU work, submitted as a unit.
- **Comparison sampler** — a sampler that returns the result of a depth comparison (0..1) instead of the depth; used for shadow maps.
- **Composition root** — the one place that creates and wires a program's long-lived objects; here `Application`.
- **Capability** — an optional piece of data an entity may have (drawn, interactable); systems act on capabilities, not on object types.
- **Composition over inheritance** — building objects from parts instead of deep class hierarchies.
- **Constant-power pan** — stereo panning with cos/sin gains so loudness stays constant across the field.
- **Coplanar** — lying in the same plane; coplanar overlapping faces cause z-fighting.
- **Crossfade loop** — blending a sound's end into its start so it loops seamlessly.
- **Cycles** — Blender's path tracer, used here to bake light.
- **Decal** — a layer of detail (stain, sign, marking) drawn over a surface: blended, depth-tested without writing depth, with a depth bias.
- **Determinism** — the same inputs always producing exactly the same output (byte-identical assets, the same attract loop at any frame rate).
- **Data-driven** — behaviour and content described in data files (levels, dialogue, actions) and interpreted by generic code.
- **Dangling reference** — a pointer or index to an object that no longer exists.
- **dt** — delta time, seconds since the last frame.
- **DXIL / HLSL / dxc** — D3D12 shader bytecode / shader language / compiler.
- **Emissive mask** — a texture saying which pixels of a surface emit light, and in what colour.
- **Equirectangular** — a 2:1 panorama mapping longitude to x and latitude to y; covers every direction.
- **Ease (smoothstep)** — 3t² − 2t³: a 0..1 ramp that starts and ends gently, for camera moves.
- **Double-sided** — rendered from both sides (no back-face culling), with the normal flipped on the back.
- **Entity** — a thing in the world: a name, a position and a set of capabilities.
- **Fingerprint** — a hash of everything a result depends on; unchanged fingerprint, unchanged result, so the work can be skipped.
- **Fireflies** — isolated, far too bright texels from a path tracer finding a rare, very bright light path.
- **Fixed timestep** — advancing a simulation in constant steps paid for by accumulated frame time, so it behaves the same at any frame rate.
- **Fever (pachinko)** — the payout mode after a lottery hit: the attacker gate opens for several rounds.
- **Frustum** — the camera's visible volume.
- **Film grain** — animated noise imitating film, applied in post.
- **Generational handle** — (slot, generation) reference that fails safely once its object is removed.
- **Glyph atlas** — a texture holding every rasterised character of a font.
- **Hysteresis** — keeping a state until the input has clearly moved past the switching point, so it doesn't flicker at the boundary.
- **Input context / action map** — named actions mapped to keys per mode, so gameplay never reads keys directly.
- **Integer scaling** — enlarging pixel art by a whole number, so every pixel stays an equal square.
- **In place (animation)** — a clip with its root motion removed, so something else (physics) moves the character.
- **Inverse bind matrix** — per joint, the transform from model space into that joint's space at the bind pose.
- **Hot reload** — replacing content in a running program when its files change, without restarting.
- **JSON Pointer** — a path to one value inside a JSON document, e.g. `/entities/3/interactable/action/type`.
- **JSON Schema** — a description of a JSON file's shape that editors use for completion and validation.
- **glTF / .glb** — standard 3D interchange format / single-file binary variant.
- **Height fog** — fog whose density falls off with altitude.
- **HDR / LDR** — high / low dynamic range (values above 1.0 or clipped).
- **Hemispheric ambient** — ambient light blended between a sky colour and a ground colour by the surface's up-facing.
- **Impostor** — a pre-rendered picture of an object on a camera-facing card, showing the view closest to the camera's direction.
- **Incremental build** — redoing only the work whose inputs changed.
- **Dear ImGui** — an immediate-mode UI library for developer tools; panels described every frame, `End()` after every `Begin()`.
- **Environment preset** — a named, partial set of sun, ambient, fog, sky, water, rain and wind values laid over a level's own; weather as data.
- **Fresnel (Schlick)** — reflection growing toward grazing angles, ≈ F0 + (1 − F0)(1 − cos θ)^5; ~2 % for water seen from above.
- **Immediate-mode UI** — UI re-described every frame by the game instead of kept as a persistent widget tree.
- **Instancing** — drawing many copies of a mesh in one draw call with per-instance data.
- **Integration / scenario test** — a test that drives the real, running program end to end.
- **Invariant** — a condition that must always hold (e.g. "the first frame of a level is drawn from its spawn").
- **Kerning** — per-pair spacing adjustment between characters (e.g. "AV").
- **Keyframe** — a value at a time; animation interpolates between keyframes.
- **Lambert** — diffuse lighting ∝ cos(angle between normal and light).
- **Light culling** — deciding per draw which lights can touch it (sphere against box), so the shader skips the rest.
- **Oblique near plane** — a projection whose near plane is tilted onto an arbitrary plane (Lengyel), so the hardware clips everything behind it; used to clip a reflection at the water.
- **Lightmap** — a texture of baked light mapped by its own non-overlapping UV set.
- **Lint** — an automatic check that rejects suspicious input (here: z-fighting geometry at export).
- **Mover** — an entity capability shuttling it between two points (the train).
- **Median / p95** — the middle frame time / the time 95 % of frames beat; robust to hitches, unlike the mean.
- **Median cut** — building a palette by repeatedly splitting the colour box with the widest range.
- **LZW** — the dictionary compression GIFs use: repeated runs become short codes.
- **Linear blend skinning** — each vertex moved by the weighted sum of its joints' palette matrices.
- **Marker** — an empty in Blender named `spawn:`/`entity:` that places something the level file describes.
- **Mipmap** — pre-filtered smaller copies of a texture.
- **MSAA / resolve** — multisample AA / averaging samples into a normal image.
- **Model / view separation** — keeping logic (the dialogue runner) apart from its presentation (the dialogue view).
- **Palette (joint)** — the matrices a skinned draw uses: joint world × inverse bind, one per joint.
- **Phase (animation)** — how far through its cycle a clip is, 0..1; walk and run blend at the same phase to stay in step.
- **Normal offset** — nudging a shadow lookup along the surface normal to avoid acne.
- **Oversampling (glyphs)** — rasterising glyphs at higher resolution so text placed between pixels stays sharp.
- **Orthographic projection** — parallel projection without perspective; used for sun shadows.
- **PCF** — percentage-closer filtering: averaging several shadow comparisons for soft edges.
- **Paired difference / ABBA** — measuring A and B alternately (AB BA AB…) and taking each round's B − A as the sample, so machine drift cancels.
- **Planar reflection** — the scene rendered again from a camera mirrored in a flat surface, sampled by that surface.
- **Payload (runtime)** — the generated products a game needs to run, as opposed to sources and tooling files.
- **PUBLIC / PRIVATE / INTERFACE (CMake)** — who needs a target's dependency: its users and itself, itself only, or its users only.
- **Peter-panning** — shadows detached from their caster because of too much bias.
- **Pipeline** — shaders + fixed-function state, baked.
- **RAII** — resource acquisition is initialisation: an object's destructor releases what it owns, so cleanup follows ownership automatically.
- **Raycast** — finding the first surface a line segment hits; used for line of sight.
- **Render target / render-to-texture** — a texture the GPU draws into and later samples like any other.
- **Representation layer** — near, middle or far: how detailed (and how expensive) a piece of the world is drawn.
- **Reverb** — the dense tail of reflections a room adds to a sound; here imitated with comb and all-pass filters.
- **Reach (pachinko)** — two reels matching while the third still turns: the tease before a hit or a near miss.
- **Replay determinism** — the same seed and inputs reproduce a whole session exactly.
- **Rest threshold** — a speed below which a contact doesn't bounce, so resting bodies don't jitter.
- **Restitution** — the share of the approach speed a bounce gives back (1 elastic, 0 dead).
- **Root motion** — travel built into a clip (a jump's rise, a walk's forward drift) rather than done by gameplay.
- **Render pass** — scope of drawing into a set of attachments, with load/store ops.
- **Rigid animation** — whole parts moving by node transforms, without deforming (compare skinning).
- **Render scale** — scene resolution as a fraction of the window.
- **Scancode** — a key's physical position, the same whatever symbol a keyboard layout prints on it.
- **Separable filter** — a 2D filter split into a horizontal and a vertical 1D pass (the Gaussian blur of the glow).
- **Sequence (action)** — a list of timed steps written as data and run over several frames (the bus arriving).
- **Sample / frame (audio)** — one amplitude value / one value per channel at a point in time.
- **Sampler** — texture read settings (filter, wrap).
- **Shadow acne** — speckled false self-shadowing from depth-comparison errors.
- **Shadow map** — a depth image rendered from a light, used to test visibility from that light.
- **Skyline card** — a flat, alpha-tested cut-out of distant buildings; rings of them give parallax in front of the sky.
- **Substep** — one of several short physics steps inside a frame's step, for stability and to avoid tunnelling.
- **Skinning** — deforming a mesh by weighted joints (a skeleton), §53.
- **Slerp** — spherical linear interpolation between rotations (quaternions), at constant angular speed.
- **Source / product (assets)** — what people edit versus what tools generate from it.
- **Spot light** — a point light limited to a cone, with inner and outer angles and a range.
- **Shader variant (permutation)** — the same source compiled with different defines, chosen per draw, instead of a runtime branch (compare uber-shader).
- **Sky gradient** — an authored zenith-to-horizon colour blend with a sun disc and halo, in place of a simulated atmosphere.
- **Spring arm** — a third-person camera on an arm that shortens at once in front of walls and eases back out.
- **Slot map** — a container of reusable slots addressed by generational handles.
- **Soft clipping** — saturating loud audio smoothly (tanh) instead of hard-cutting at ±1.
- **Spawn** — a named position and facing where the player enters a level.
- **sRGB / linear** — display-encoded / physically-linear colour.
- **State machine** — logic organised as explicit states and the transitions between them (dialogue runner, level manager).
- **Story flag** — a named boolean recording progress ("keeper_permission"), surviving level changes.
- **Swapchain** — the window's ring of presentable images.
- **Sway (vertex)** — moving vertices in the vertex shader by a weighted wind offset, for foliage and cloth.
- **Soft knee** — a threshold that eases in over a band instead of switching on (the glow's bright pass).
- **Tunnelling** — a fast body passing through a thin one between two physics steps.
- **Third person** — the camera outside the character, usually behind it; movement relative to the camera.
- **Thermal throttling** — a hot CPU/GPU lowering its clocks, so the same work gets slower during a run.
- **Streak particle** — a particle drawn as a thin quad along its motion instead of facing the camera; rain.
- **Tessellation (here)** — splitting large faces into a grid so vertex-stored data (baked light) has vertices to live on.
- **Texel snapping** — moving a shadow box only in whole-texel steps to stop shimmering.
- **Tonemapping** — compressing HDR values into the display range with a smooth curve.
- **Transfer buffer** — CPU-visible staging memory for uploads.
- **Typewriter effect** — revealing a line of text a character at a time.
- **Uniform (buffer)** — small constants for shaders, pushed per draw or per frame.
- **Unit test** — a fast test of one piece of logic in isolation.
- **`std::variant` / `std::visit`** — a type-safe union / calling the right handler for its current alternative; the compiler checks every alternative is handled.
- **Vignette** — gradual darkening toward the image corners.
- **Voice** — one playing instance of a sound in a mixer.
- **Vsync** — syncing presentation to the monitor refresh.
- **Windowed falloff** — inverse-square attenuation multiplied by a window that reaches exactly zero at the light's range.
- **Swapchain loss** — the window's presentable images becoming unusable (device removed, display changed); here handled by a fallback for the next launch (§80).
- **Template method** — a base class fixing the order of steps and letting a subclass fill some in; `Application` and `OnConfigure` / `OnInitialize`.
- **Use-after-free** — touching memory or a resource after its owner released it; here, a GPU wrapper outliving its device.
- **Validation ladder** — checking a change with the cheapest step that can catch its mistakes, escalating only on success.
- **Uber-shader** — one shader covering many features with runtime branches; fewer pipelines, but skipped code can still cost (§57, §69).
- **Watchdog (wall-clock)** — an outside timer that stops a process whose own, frame-driven timeouts can no longer fire.
- **Winding** — vertex order of a triangle (CW/CCW), used for back-face culling.
- **Wet material** — a material whose emitted light ripples and which catches a moving sheen; the fake wet road.
- **xorshift** — a tiny, fast pseudo-random generator; seeded, it gives the same sequence on every platform.
- **Baseline (measurement)** — a recorded number for later versions to compare against, as opposed to a comparison drawn now (§103).
- **Cel / toon shading** — diffuse light quantised into a few flat bands instead of a smooth gradient (§98).
- **Compile-time switch** — a preprocessor definition that removes code from a build entirely, rather than disabling it at runtime (`ATOM_DEV_TOOLS`, §101).
- **DPI awareness** — whether a Windows program draws at the display's real pixel density; an unaware one is drawn at 100 % and stretched, blurring it (§102).
- **Envelope** — how a note's loudness evolves: here a linear attack, then an exponential decay (§99).
- **Fidelity sheet** — a part-by-part comparison of a port against its original, with how each part was checked (§103).
- **FP64** — 64-bit (double-precision) floating point in shaders; optional in D3D12 and missing on many integrated GPUs (§102).
- **Framework layer** — code every game on an engine shares but the engine doesn't own: logs, settings, the command line, diagnostics (§95).
- **Generative music** — music produced live by rules and randomness rather than played from a recording (§99).
- **Inverted hull** — an outline drawn as a slightly inflated copy of the mesh with its front faces culled, so only a rim shows (§98).
- **Literal (shader)** — a number written in code; without a suffix its type comes from context, which can make it double (§102).
- **Lock-free / SPSC queue** — a queue made safe with atomic indices instead of a lock; single-producer, single-consumer: one thread writes, one reads (§99).
- **PolyBLEP** — a cheap correction around each jump of a sawtooth that removes most of its aliasing (§99).
- **Port (faithful)** — reproducing a program on another platform with the same behaviour, before changing anything (§96).
- **Real-time thread** — a thread with a hard deadline (audio): it must never wait on a lock or the allocator (§99).
- **Relative mouse mode** — the cursor hidden and pinned, the program reading only movement; for mouse-look, not for pointing (§103).
- **Sample-accurate scheduling** — starting each note on its exact sample inside an audio block, not at the block's start (§99).
- **Test selection** — running only the tests a change can affect, chosen from the changed files (§100).
- **Z-fighting** — flicker when two surfaces share the same (or nearly the same) depth, so the depth test picks a different winner per pixel and frame; in AtomEngine caused by coplanar overlapping faces (§33).
