# AtomEngine — Technical Manual

A study guide to every concept the engine uses, as of **v0.0.3 / M20**. v0.0.1 (M2–M8) covers rendering, lighting and fog, shadows, tonemapping and grading, particles, audio and the scripted unease moments. v0.0.2 (M9–M14) adds text and UI, entities and interaction, dialogue, data-driven levels, automated testing and validation (§28–§33). v0.0.3 (M15–M20) adds baked lighting (vertex colours and lightmaps), alpha-tested materials, decals, rigid animation and vertex sway, a fourth level, and authoring tools: schemas, precise errors, hot reload and Blender markers (§34–§39).
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
40. [Anatomy of a frame and what it costs](#40-anatomy-of-a-frame-and-what-it-costs)
41. [Build system and project layout](#41-build-system-and-project-layout)
42. [Glossary](#42-glossary)

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
- **Bakes** (v0.0.3): `atom_bake.py` bakes light into vertex colours and writes sway weights into their alpha (§34, §38); `atom_lightmap.py` bakes lightmaps (§35). Markers become `<level>.markers.json` (§39). A full build takes about two minutes and stays byte-identical.
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

Four levels ship: **A** the street, **B** the shrine grounds (through the barred gate), **C** a machiya interior (through the house door at the east end) and, since v0.0.3, **D** the windmill field (through the field path at the west end, §38). A level file may leave spawn and entity positions to Blender markers (§39).

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

## 40. Anatomy of a frame and what it costs

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

**v0.0.3** adds more vertices (tessellation for the bake, §34), a lightmap sample in the interior, alpha-tested cards in the scene and shadow passes, decals, and sway in the vertex shader. Its per-level frame times are measured for the v0.0.3 release (M21) and not recorded here yet.

---

## 41. Build system and project layout

- **CMake** (≥ 3.25), C++20. Targets: `AtomEngine` (static lib), `AtomGameLib` (gameplay as a static lib), `AtomGame` (exe), `AtomTests` (doctest unit tests), `AtomShaders` (custom target compiling HLSL). Each `Tests/Scenarios/*.atomtest` is a ctest test that runs `AtomGame` with `ATOM_TEST_SCRIPT` (label `scenario`).
- **Dependencies as git submodules, pinned**: SDL 3.4.16, GLM 1.0.1, cgltf v1.15, stb, nlohmann/json 3.12.0, doctest 2.5.3.
- **Version**: `project(VERSION …)` in CMake becomes `ATOM_VERSION`, shown in the log and the window title.
- Post-build step copies `Assets/` next to the executable; shaders are compiled into `bin/<Config>/shaders/`.
- Visual Studio's built-in HLSL (FXC) is disabled on `.hlsl`/`.hlsli` files (`VS_TOOL_OVERRIDE None`) so only dxc compiles them; `Common.hlsli` is a dependency of every shader.
- `NoTrack/` and `build/` are git-ignored; this manual lives in `docs/`.
- **Environment switches** for development: `ATOM_VSYNC=0` (uncapped frame rate), `ATOM_AUDIO_CAPTURE=file.wav` (record the mix), `ATOM_START_LEVEL=<level>[:<spawn>]` (start anywhere), `ATOM_TEST_SCRIPT=<file>` (run a scenario, exit 0/1), `ATOM_ASSET_ROOT=<repo>` (read the source tree and hot-reload, §39).
- **Assets**: `blender -b --factory-startup -P Tools/Blender/build_assets.py` rebuilds every glb, the lightmap and the markers; the game build copies `Assets/` next to the executable, so rebuild the game (or use `ATOM_ASSET_ROOT`) to see new assets.
- **Running tests**: `ctest --test-dir build -C Release` (all), `-LE scenario` (unit tests only, no GPU), `-L scenario` (in-game).

```
Engine/  Assets/ Audio/ Core/ Physics/ Platform/ Renderer/ Scene/ UI/
Game/    DemoApp, PlayerController, AudioScape, SoundSynth,
         Atmosphere, UneaseDirector, Main
         World/ Interaction/ Dialogue/ Level/ Testing/
Shaders/ Basic, Shadow, Particle, Fullscreen, Post, UI (.hlsl)
         + Common.hlsli, Sway.hlsli
Tools/Blender/  kit + street + levels + lint + bakes (vertex, lightmap)
                + markers + export
Assets/  Kit/ Street/ Shrine/ Interior/ Fields/ (.glb, lightmap .png),
         Levels/*.json (+ *.markers.json), Dialogue/*.json,
         Schemas/*.schema.json, Fonts/
docs/    this manual
Tests/   unit tests (*.cpp), Scenarios/*.atomtest
external/ SDL glm cgltf stb json doctest
```

**Controls:** WASD, Shift jog, mouse look, **E interact** (in dialogue: continue/confirm; W/S or 1–4 choose), Esc release/quit · F1 debug overlay · F2 render scale · F3 baked light · F4 MSAA · F5 fog · F6 shadows · F7 post look · F8 particles · F9 unease moments · M mute.

---

## 42. Glossary

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
- **Beer–Lambert law** — light through a medium decays as e^(−density·distance); the basis of exponential fog.
- **Billboard** — a quad that always faces the camera.
- **Biquad** — a standard 2nd-order digital filter (low/high/band-pass).
- **Catenary** — the sag curve of a hanging cable (approximated by a parabola for the power lines).
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
- **Data-driven** — behaviour and content described in data files (levels, dialogue, actions) and interpreted by generic code.
- **Dangling reference** — a pointer or index to an object that no longer exists.
- **dt** — delta time, seconds since the last frame.
- **DXIL / HLSL / dxc** — D3D12 shader bytecode / shader language / compiler.
- **Double-sided** — rendered from both sides (no back-face culling), with the normal flipped on the back.
- **Entity** — a thing in the world: a name, a position and a set of capabilities.
- **Frustum** — the camera's visible volume.
- **Film grain** — animated noise imitating film, applied in post.
- **Generational handle** — (slot, generation) reference that fails safely once its object is removed.
- **Glyph atlas** — a texture holding every rasterised character of a font.
- **Hot reload** — replacing content in a running program when its files change, without restarting.
- **JSON Pointer** — a path to one value inside a JSON document, e.g. `/entities/3/interactable/action/type`.
- **JSON Schema** — a description of a JSON file's shape that editors use for completion and validation.
- **glTF / .glb** — standard 3D interchange format / single-file binary variant.
- **Height fog** — fog whose density falls off with altitude.
- **HDR / LDR** — high / low dynamic range (values above 1.0 or clipped).
- **Hemispheric ambient** — ambient light blended between a sky colour and a ground colour by the surface's up-facing.
- **Immediate-mode UI** — UI re-described every frame by the game instead of kept as a persistent widget tree.
- **Instancing** — drawing many copies of a mesh in one draw call with per-instance data.
- **Integration / scenario test** — a test that drives the real, running program end to end.
- **Invariant** — a condition that must always hold (e.g. "the first frame of a level is drawn from its spawn").
- **Kerning** — per-pair spacing adjustment between characters (e.g. "AV").
- **Keyframe** — a value at a time; animation interpolates between keyframes.
- **Lambert** — diffuse lighting ∝ cos(angle between normal and light).
- **Lightmap** — a texture of baked light mapped by its own non-overlapping UV set.
- **Lint** — an automatic check that rejects suspicious input (here: z-fighting geometry at export).
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
- **Render pass** — scope of drawing into a set of attachments, with load/store ops.
- **Rigid animation** — whole parts moving by node transforms, without deforming (no skinning).
- **Render scale** — scene resolution as a fraction of the window.
- **Sample / frame (audio)** — one amplitude value / one value per channel at a point in time.
- **Sampler** — texture read settings (filter, wrap).
- **Shadow acne** — speckled false self-shadowing from depth-comparison errors.
- **Shadow map** — a depth image rendered from a light, used to test visibility from that light.
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
- **Z-fighting** — flicker when two surfaces share the same (or nearly the same) depth, so the depth test picks a different winner per pixel and frame; in AtomEngine caused by coplanar overlapping faces (§33).
