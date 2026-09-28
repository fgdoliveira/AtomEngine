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
