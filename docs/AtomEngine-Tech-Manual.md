# AtomEngine — Technical Manual

A study guide to every concept the engine uses, as of **v0.0.4 / M28**. v0.0.1 (M2–M8) covers rendering, lighting and fog, shadows, tonemapping and grading, particles, audio and the scripted unease moments. v0.0.2 (M9–M14) adds text and UI, entities and interaction, dialogue, data-driven levels, automated testing and validation (§28–§33). v0.0.3 (M15–M20) adds baked lighting (vertex colours and lightmaps), alpha-tested materials, decals, rigid animation and vertex sway, a fourth level, and authoring tools: schemas, precise errors, hot reload and Blender markers (§34–§39). v0.0.4 (M22–M28) adds the night city: chunks, cells and distance layers, a collision grid, draw sorting and a model cache; emissive masks, glow, halos and a night sky; facade shells, impostors and skyline cards; per-chunk lightmaps, live lights and a wet road; action sequences; render-to-texture screens, a fixed timestep and a room reverb; and a cached asset build (§40–§46).
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
47. [Anatomy of a frame and what it costs](#47-anatomy-of-a-frame-and-what-it-costs)
48. [Build system and project layout](#48-build-system-and-project-layout)
49. [Glossary](#49-glossary)

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

**Mipmaps.** A chain of pre-shrunk copies (256, 128, 64 … 1 px). When a surface is far away, one screen pixel covers many texels; sampling the full-size image then *aliases* (shimmers). The GPU instead picks the mip level whose texel size ≈ pixel size. AtomEngine generates the chain on the GPU (`SDL_GenerateMipmapsForGPUTexture`), which is why textures are created with `COLOR_TARGET` usage too.

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

**Code.** `Texture::Create` (format choice), swapchain setup in `Renderer::Initialize`.

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
- **2× horizontal oversampling**: each glyph is rasterised at double width. Text positioned between pixels then samples a sub-pixel-accurate glyph, so small text stays crisp.
- The atlas stores **white RGB with coverage in alpha**, so any colour is just a tint. It's a **linear** texture (not sRGB): alpha is a coverage fraction, not a colour.
- **Kerning** (per-pair spacing like "AV") is precomputed into a map of non-zero pairs, so the header doesn't expose stb types.
- The bundled font is a Latin subset of *Shippori Mincho* (SIL OFL) — a mincho (serif) face that suits the setting.

**The overlay (`Engine/UI/UIRenderer`).**
- API in window pixels, origin top-left: `DrawRect`, `DrawText`, `MeasureText`, `WrapText` (inserts line breaks at spaces to fit a width). Text is **UTF-8**; malformed bytes decode as `?`.
- Colours are given in **sRGB** (as picked in a paint program) and converted to linear for blending (§12).
- Solid rectangles sample a **white texel** in the atlas, so a single shader and pipeline draw both rectangles and text. Consecutive quads with the same texture are **batched** into one draw call.
- Vertices are uploaded each frame (like particles, §25) and drawn **after the post pass**, straight into the swapchain image with `LOAD` (keep what post wrote). So UI is never tonemapped, graded, fogged or grained, and is always at window resolution regardless of render scale (§20).
- The pen is snapped to whole pixels vertically; horizontal positions use the oversampling.

**What the game draws.** The fading controls hint at start, the `[E] prompt` of the current target, the message line (`MessageFeed`: one line of feedback that fades after a few seconds; a new message replaces the old), the dialogue panel (§30), and the **F1** debug panel (frame time, render settings, level, position, draws, voices).

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

Times outside the keys clamp to the first or last key. This is *rigid* animation: whole parts move, nothing bends. Bending needs skinning (joints and weights), which was left out on purpose.

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

## 47. Anatomy of a frame and what it costs

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
- **The night levels** draw nothing in the shadow pass (night lighting turns sun shadows off); their extra work is glow, live lights and, in the hall, the render-texture screens.

---

## 48. Build system and project layout

- **CMake** (≥ 3.25), C++20. Targets: `AtomEngine` (static lib), `AtomGameLib` (gameplay as a static lib), `AtomGame` (exe), `AtomTests` (doctest unit tests), `AtomShaders` (custom target compiling HLSL). Each `Tests/Scenarios/*.atomtest` is a ctest test that runs `AtomGame` with `ATOM_TEST_SCRIPT` (label `scenario`).
- **Dependencies as git submodules, pinned**: SDL 3.4.16, GLM 1.0.1, cgltf v1.15, stb, nlohmann/json 3.12.0, doctest 2.5.3.
- **Version**: `project(VERSION …)` in CMake becomes `ATOM_VERSION`, shown in the log and the window title.
- Post-build step copies `Assets/` next to the executable; shaders are compiled into `bin/<Config>/shaders/`.
- Visual Studio's built-in HLSL (FXC) is disabled on `.hlsl`/`.hlsli` files (`VS_TOOL_OVERRIDE None`) so only dxc compiles them; `Common.hlsli` is a dependency of every shader.
- `NoTrack/` and `build/` are git-ignored; this manual lives in `docs/`.
- **Asset build options** (after `--`): `--no-cache` re-bakes every lightmap, `--gpu` bakes on the NVIDIA GPU for light tuning (§46), `--no-export` stops after the lint.
- **Environment switches** for development: `ATOM_VSYNC=0` (uncapped frame rate), `ATOM_AUDIO_CAPTURE=file.wav` (record the mix), `ATOM_START_LEVEL=<level>[:<spawn>]` (start anywhere), `ATOM_TEST_SCRIPT=<file>` (run a scenario, exit 0/1), `ATOM_ASSET_ROOT=<repo>` (read the source tree and hot-reload, §39).
- **Assets**: `blender -b --factory-startup -P Tools/Blender/build_assets.py` rebuilds every glb, the lightmaps (skipping unchanged ones, §46) and the markers; the game build copies `Assets/` next to the executable, so rebuild the game (or use `ATOM_ASSET_ROOT`) to see new assets.
- **Running tests**: `ctest --test-dir build -C Release` (all), `-LE scenario` (unit tests only, no GPU), `-L scenario` (in-game).

```
Engine/  Assets/ Audio/ Core/ Physics/ Platform/ Renderer/ Scene/ UI/
Game/    DemoApp, PlayerController, AudioScape, SoundSynth,
         Atmosphere, UneaseDirector, Main
         World/ Interaction/ Dialogue/ Level/ Testing/
Shaders/ Basic, Shadow, Particle, Fullscreen, Post, UI, Sky, Halo,
         GlowBright, GlowBlur (.hlsl) + Common.hlsli, Sway.hlsli
Tools/Blender/  kit + street + levels + city + night + pachinko + lint
                + bakes (vertex, lightmap, cached) + impostors + markers
                + export
Assets/  Kit/ Street/ Shrine/ Interior/ Fields/ City/ Night/ Pachinko/
         Sky/ (.glb, lightmap .png, impostor atlas),
         Levels/*.json (+ *.markers.json), Dialogue/*.json,
         Schemas/*.schema.json, Fonts/
docs/    this manual
Tests/   unit tests (*.cpp), Scenarios/*.atomtest
external/ SDL glm cgltf stb json doctest
```

**Controls:** WASD, Shift jog, mouse look, **E interact** (in dialogue: continue/confirm; W/S or 1–4 choose), Esc release/quit · F1 debug overlay · F2 render scale · F3 baked light · F4 MSAA · F5 fog · F6 shadows · F7 post look · F8 particles · F9 unease moments · M mute.

---

## 49. Glossary

- **AABB** — axis-aligned bounding box (min/max corners).
- **ACES** — a film-industry colour standard; its filmic tonemapping curve is widely approximated in games.
- **Alpha blending** — mixing a transparent colour over what's behind it by its alpha; order-dependent.
- **Alpha dilation** — filling transparent texels with nearby opaque colour so filtering doesn't pull dark fringes into cut-out edges.
- **Alpha test / alpha mask** — drawing a pixel or discarding it by comparing its alpha with a cutoff; no sorting needed.
- **Alpha-to-coverage** — with MSAA, turning a pixel's alpha into how many of its samples are covered: soft cut-out edges without sorting.
- **Atlas** — several small images packed into one texture.
- **Attachment** — a texture a render pass draws into (colour, depth).
- **Attenuation** — a sound getting quieter with distance.
- **Baked lighting** — light computed offline and stored (in vertex colours or lightmaps) instead of computed every frame.
- **Chart (UV)** — a connected piece of a model laid flat in a UV set; lightmap charts must not overlap.
- **Bandwidth** — bytes moved between GPU and memory per second; the usual bottleneck on integrated GPUs.
- **Bloom / glow** — bright parts of the image blurred and added back, imitating light scattering in the eye and lens.
- **Broad phase** — a cheap first test that narrows down which objects or triangles could collide, before the exact test.
- **Beer–Lambert law** — light through a medium decays as e^(−density·distance); the basis of exponential fog.
- **Billboard** — a quad that always faces the camera.
- **Biquad** — a standard 2nd-order digital filter (low/high/band-pass).
- **Catenary** — the sag curve of a hanging cable (approximated by a parabola for the power lines).
- **Cell** — a connected area of a level (a stretch of street, an alley); only the player's cell and its neighbours are drawn.
- **Chunk** — a piece of a level drawn and culled as a whole, with a distance layer and a shadow flag.
- **Comb filter** — a delay fed back into itself: an echo that repeats and decays; the building block of classic reverbs.
- **Clip (animation)** — a named set of keyframed channels that move a model's nodes.
- **Clip space / NDC** — coordinates after projection / after dividing by w.
- **Command buffer** — recorded GPU work, submitted as a unit.
- **Comparison sampler** — a sampler that returns the result of a depth comparison (0..1) instead of the depth; used for shadow maps.
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
- **Double-sided** — rendered from both sides (no back-face culling), with the normal flipped on the back.
- **Entity** — a thing in the world: a name, a position and a set of capabilities.
- **Fingerprint** — a hash of everything a result depends on; unchanged fingerprint, unchanged result, so the work can be skipped.
- **Fireflies** — isolated, far too bright texels from a path tracer finding a rare, very bright light path.
- **Fixed timestep** — advancing a simulation in constant steps paid for by accumulated frame time, so it behaves the same at any frame rate.
- **Frustum** — the camera's visible volume.
- **Film grain** — animated noise imitating film, applied in post.
- **Generational handle** — (slot, generation) reference that fails safely once its object is removed.
- **Glyph atlas** — a texture holding every rasterised character of a font.
- **Hysteresis** — keeping a state until the input has clearly moved past the switching point, so it doesn't flicker at the boundary.
- **Hot reload** — replacing content in a running program when its files change, without restarting.
- **JSON Pointer** — a path to one value inside a JSON document, e.g. `/entities/3/interactable/action/type`.
- **JSON Schema** — a description of a JSON file's shape that editors use for completion and validation.
- **glTF / .glb** — standard 3D interchange format / single-file binary variant.
- **Height fog** — fog whose density falls off with altitude.
- **HDR / LDR** — high / low dynamic range (values above 1.0 or clipped).
- **Hemispheric ambient** — ambient light blended between a sky colour and a ground colour by the surface's up-facing.
- **Impostor** — a pre-rendered picture of an object on a camera-facing card, showing the view closest to the camera's direction.
- **Incremental build** — redoing only the work whose inputs changed.
- **Immediate-mode UI** — UI re-described every frame by the game instead of kept as a persistent widget tree.
- **Instancing** — drawing many copies of a mesh in one draw call with per-instance data.
- **Integration / scenario test** — a test that drives the real, running program end to end.
- **Invariant** — a condition that must always hold (e.g. "the first frame of a level is drawn from its spawn").
- **Kerning** — per-pair spacing adjustment between characters (e.g. "AV").
- **Keyframe** — a value at a time; animation interpolates between keyframes.
- **Lambert** — diffuse lighting ∝ cos(angle between normal and light).
- **Lightmap** — a texture of baked light mapped by its own non-overlapping UV set.
- **Lint** — an automatic check that rejects suspicious input (here: z-fighting geometry at export).
- **Mover** — an entity capability shuttling it between two points (the train).
- **Marker** — an empty in Blender named `spawn:`/`entity:` that places something the level file describes.
- **Mipmap** — pre-filtered smaller copies of a texture.
- **MSAA / resolve** — multisample AA / averaging samples into a normal image.
- **Model / view separation** — keeping logic (the dialogue runner) apart from its presentation (the dialogue view).
- **Normal offset** — nudging a shadow lookup along the surface normal to avoid acne.
- **Oversampling (glyphs)** — rasterising glyphs at higher resolution so text placed between pixels stays sharp.
- **Orthographic projection** — parallel projection without perspective; used for sun shadows.
- **PCF** — percentage-closer filtering: averaging several shadow comparisons for soft edges.
- **Peter-panning** — shadows detached from their caster because of too much bias.
- **Pipeline** — shaders + fixed-function state, baked.
- **RAII** — resource acquisition is initialisation: an object's destructor releases what it owns, so cleanup follows ownership automatically.
- **Raycast** — finding the first surface a line segment hits; used for line of sight.
- **Render target / render-to-texture** — a texture the GPU draws into and later samples like any other.
- **Representation layer** — near, middle or far: how detailed (and how expensive) a piece of the world is drawn.
- **Reverb** — the dense tail of reflections a room adds to a sound; here imitated with comb and all-pass filters.
- **Render pass** — scope of drawing into a set of attachments, with load/store ops.
- **Rigid animation** — whole parts moving by node transforms, without deforming (no skinning).
- **Render scale** — scene resolution as a fraction of the window.
- **Separable filter** — a 2D filter split into a horizontal and a vertical 1D pass (the Gaussian blur of the glow).
- **Sequence (action)** — a list of timed steps written as data and run over several frames (the bus arriving).
- **Sample / frame (audio)** — one amplitude value / one value per channel at a point in time.
- **Sampler** — texture read settings (filter, wrap).
- **Shadow acne** — speckled false self-shadowing from depth-comparison errors.
- **Shadow map** — a depth image rendered from a light, used to test visibility from that light.
- **Skyline card** — a flat, alpha-tested cut-out of distant buildings; rings of them give parallax in front of the sky.
- **Skinning** — deforming a mesh by weighted joints (a skeleton); not used in AtomEngine.
- **Slerp** — spherical linear interpolation between rotations (quaternions), at constant angular speed.
- **Slot map** — a container of reusable slots addressed by generational handles.
- **Soft clipping** — saturating loud audio smoothly (tanh) instead of hard-cutting at ±1.
- **Spawn** — a named position and facing where the player enters a level.
- **sRGB / linear** — display-encoded / physically-linear colour.
- **State machine** — logic organised as explicit states and the transitions between them (dialogue runner, level manager).
- **Story flag** — a named boolean recording progress ("keeper_permission"), surviving level changes.
- **Swapchain** — the window's ring of presentable images.
- **Sway (vertex)** — moving vertices in the vertex shader by a weighted wind offset, for foliage and cloth.
- **Soft knee** — a threshold that eases in over a band instead of switching on (the glow's bright pass).
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
- **Winding** — vertex order of a triangle (CW/CCW), used for back-face culling.
- **Wet material** — a material whose emitted light ripples and which catches a moving sheen; the fake wet road.
- **xorshift** — a tiny, fast pseudo-random generator; seeded, it gives the same sequence on every platform.
- **Z-fighting** — flicker when two surfaces share the same (or nearly the same) depth, so the depth test picks a different winner per pixel and frame; in AtomEngine caused by coplanar overlapping faces (§33).
