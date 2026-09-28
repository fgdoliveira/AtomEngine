# Changelog

## 0.0.3 — in progress

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
