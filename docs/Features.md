# AtomEngine features

What the engine can do today, each with the evidence for it: where it is
in the code, how it's tested, what it costs and what it doesn't do. This
page is the catalog behind the Showcase's lens (press **Tab** in the
Showcase): every entry here is a *Feature* in
`Showcase/Features/Features.cpp`, and a unit test
(`ShowcaseFeatureTests`) fails if one is missing from this page, names a
source file that doesn't exist, or a manual section that isn't a heading.
A new capability enters the engine this way (ADR-009 in
[Architecture](Architecture.md)).

**Reading the costs.** Measured on the development laptop (Intel Iris Xe,
1280×720, plugged in) with the paired in-process `bench` (README,
*Measuring performance*), in the version noted. "Live" is the number the
lens shows for the frame you're looking at.

**Every Feature can be switched off** in the lens (keys 1-9, 0) or by a
scenario (`set feature_<id> off`); `showcase_lens` switches each one off
and checks its effect leaves the frame (draws, particles or the value
drop to zero), then back on.

| Feature | id | Status | Manual |
|---|---|---|---|
| Stylized water | `water` | stable | §65 |
| Planar reflection | `reflection` | stable, experimental origin (kept in v0.0.8) | §68 |
| Sun shadows | `shadows` | stable | §23 |
| Height fog | `fog` | stable | §22 |
| Weather and wind | `weather` | stable | §67 |
| Live lights | `lights` | stable | §43 |
| Glow and halos | `glow` | stable | §41 |
| Toon shading and outlines | `toon` | stable | §98 |
| Skinned character | `character` | stable | §55 |
| Live synth | `synth` | stable | §99 |

## Stylized water (`water`)
- **What:** a water surface shaded procedurally: Fresnel between the sky
  and a depth colour, scrolling ripples, sun glints, rain rings.
- **Where:** `Shaders/Water.frag.hlsl`; the lake in the village.
- **Tests:** `showcase_lens` (water draws on, then 0); `showcase_tour`
  (`expect_water`); the environment presets' water colours in
  `EnvironmentControllerTests`.
- **Cost:** +0.34 ms from the beach, +0.68 ms over open water (v0.0.8):
  it costs with the screen area it covers. Live: surfaces drawn.
- **Limits:** no refraction of the lake bed and no waves that move
  geometry; one flat surface height per water mesh.

## Planar reflection (`reflection`)
- **What:** the scene drawn a second time from a camera mirrored in the
  water's plane, at half resolution, sampled by the water.
- **Where:** `Engine/Renderer/Renderer.cpp`; the lake.
- **Tests:** `showcase_lens` (reflection draws on, then 0); `showcase_tour`
  (`expect_reflection`).
- **Cost:** +0.12 to +0.22 ms (v0.0.8). Live: draws mirrored.
- **Limits:** reflects the sky and the near layer's opaque draws only - no
  decals, particles, water or distant layers; exact for flat water only;
  skipped when no water is in view; the Low quality tier turns it off.

## Sun shadows (`shadows`)
- **What:** a directional shadow map, texel-snapped so it doesn't shimmer,
  filtered with PCF.
- **Where:** `Shaders/Shadow.vert.hlsl` and the renderer's shadow pass.
- **Tests:** `showcase_lens` (shadow draws on, then 0); `quality_tiers`
  (the switch by hand moves the tier to Custom).
- **Cost:** about 1 ms at 720p (a fixed 2048² depth pass, independent of
  the window size). Live: draws in the shadow pass.
- **Limits:** one map, no cascades: detail falls off over large views.

## Height fog (`fog`)
- **What:** exponential fog thickening with distance and with depth below
  a height, in the scene shader, coloured by the environment.
- **Where:** `Shaders/Common.hlsli`.
- **Tests:** `showcase_lens` (density on, then 0).
- **Cost:** a few instructions per pixel; not measurable alone. Live: the
  density in use.
- **Limits:** no volumetric light shafts; one fog colour per frame.

## Weather and wind (`weather`)
- **What:** environment presets (clear day, sunset, night, rain, overcast,
  fog) blended over a level's own light; rain as stretched particles that
  slant with the wind, leaves blown by it, vegetation swaying.
- **Where:** `Framework/Environment/Atmosphere.cpp`,
  `EnvironmentController`, `Content/Environments/`; the village's clock
  runs through them.
- **Tests:** `showcase_lens` (rain particles, then only the halos);
  `showcase_tour` (the clock: day, dusk, night, rain); `environment`;
  `EnvironmentControllerTests`, `EnvironmentTests`.
- **Cost:** rain +0.16 to +0.25 ms (v0.0.8), mostly its 1,400 drops'
  update, sort and overdraw. Live: particles drawn.
- **Limits:** particles live in a box around the viewer; no snow, puddles
  or wet surfaces outside the demo's road.

## Live lights (`lights`)
- **What:** per-pixel point lights (street lamps, the neon sign), each
  draw lit by the nearest few that reach it, culled per draw.
- **Where:** `Engine/Renderer/Renderer.cpp`; the village lane.
- **Tests:** `showcase_lens` (live lights on, then 0); `night_street`.
  (`LightingTests` covers the spot light's cone, falloff and shadow.)
- **Cost:** live: lights and the draws they light.
- **Limits:** at most 4 lights per draw; point lights cast no shadows
  (the spot light does - the demo's flashlight).

## Glow and halos (`glow`)
- **What:** a bright pass, blurred and added back (bloom), and additive
  billboard halos around lamps.
- **Where:** `Engine/Renderer/Glow.cpp`; the lamps and the neon at night.
- **Tests:** `showcase_lens` (glow strength on, then 0).
- **Cost:** live: strength and halos drawn.
- **Limits:** a fixed blur radius; halos are not occluded by geometry.

## Toon shading and outlines (`toon`)
- **What:** a scene-shader variant with three light steps, and an
  inverted-hull outline pass, chosen per model or material.
- **Where:** `Shaders/BasicToon.frag.hlsl`; the shrine.
- **Tests:** `showcase_lens` (switched off and on); DRIFT uses it
  throughout (`drift_fly`).
- **Cost:** the outline draws each styled model once more. Live: styled
  models.
- **Limits:** outlines on alpha-tested cards (leaves) outline the whole
  card, so those models take toon shading without an outline.

## Skinned character (`character`)
- **What:** skeletal skinning, clips blended (walk/run by speed) and an
  animation state machine (idle, move, jump) driven by parameters; a
  model-viewer lab (E at the workshop) and a playable third-person drive.
- **Where:** `Framework/Character/Animator.cpp`, `LabViewer`,
  `PlayerController`; the workshop.
- **Tests:** `showcase_lab`, `showcase_tour` (`expect_state`);
  `AnimationTests`, `AnimatorTests`, `SkinningTests`, `LabViewerTests`,
  `DriveTests`.
- **Cost:** live: the animator's state.
- **Limits:** up to four joints per vertex; no inverse kinematics, root
  motion or animation retargeting.

## Live synth (`synth`)
- **What:** sound synthesised on the audio thread (oscillators,
  envelopes, a low-pass filter bus, a delay), played from the game thread
  through a lock-free command queue - no locks or allocation on the audio
  thread. The radio shed is played with J K L ;.
- **Where:** `Engine/Audio/Synth.cpp`, `SynthStream`; the radio shed.
  DRIFT's whole soundtrack runs on it.
- **Tests:** `SynthTests`, `BoothSynthTests`; `drift_fly`.
- **Cost:** more than 10× real time in Release. Live: silent until a key.
- **Limits:** 160 voices; no sample playback in the synth (the mixer,
  `AudioSystem`, plays sampled sounds).
