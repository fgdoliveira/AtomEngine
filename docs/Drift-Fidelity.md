# DRIFT: fidelity to the web original

DRIFT (v0.0.13) is a port of a three.js web game (`3d-game-00`: `main.js`,
`ship.js`, `world.js`, `interact.js`, `toon.js`, `audio.js`, `index.html`)
to AtomEngine. The brief was **faithful first**: the same rules, the same
look, the same music, before anything new. This sheet says how each part
was checked and where the port deliberately differs.

**Checked by:**
- *test*: a unit test pins the original's constants (`Tests/DriftFlightTests.cpp`,
  `DriftWorldTests.cpp`, `SynthTests.cpp`);
- *scenario*: `Scenario.drift_fly`, a 20 s autopilot run on a fixed seed;
- *by eye*: side by side with the web version during development;
- *by ear*: the listening check, which only a person can do.

## Rules

| Part | Original | Checked by |
|---|---|---|
| The path | two sines and a cosine | test |
| Cruise speed | eases toward 38 + flow × 34 m/s | test |
| Boost | forward speed × up to 1.7; the model stretches | test |
| Steering | lateral velocity eases toward input × (16, 12); keys plus the pointer's position × 0.35 | test (input: by eye) |
| The leash | within 11 m of the path | test |
| Camera | FOV 70 + boost × 18 + flow × 6, eased; looks 12 m ahead; shake decays at 2/s | test |
| The course | a segment every 80 m, 720 m ahead; a ring and 7 orbs each; 2 + min(4, segment / 4) rocks sized 0.8–2.6; recycled 20 m behind | test |
| Flow | ring within 3.1 m +0.10, miss −0.08; orb +0.025 and chain +1; rock −0.25 and chain 0; decays 0.012/s; 0..1 | test |
| Frame step | `dt` clamped to 0.05 s | code review |
| A whole run | rings are passed, orbs collected, no lifetime errors | scenario |

## Look

| Part | Original | Port | Checked by |
|---|---|---|---|
| Shading | `MeshToonMaterial`, a 3-step gradient map (70/160/255) | the `BasicToon` shader variant, same steps | by eye |
| Outlines | inverted hull, 0.045, `#07060f`, all but the orbs | the outline pipeline, same values | by eye |
| Stars and streaks | 1500 points within 400 m; 120 lines within 130 m, shown above 45 m/s, ≤ 0.55, 0.08 s long | particles with a 1×1 white atlas; same counts and rules; colours compensated for the engine's fog | test, by eye |
| Bloom | `UnrealBloomPass(0.6, 0.5, 0.85)`, strength 0.45 + flow × 0.7 | the engine's glow, threshold 0.85, same strength curve | by eye (a different blur kernel: close, not identical) |
| Engine light | `PointLight(0xffa050, 4 + boost × 10 + sin(30t) × 0.8)` | same | by eye |
| HUD | Space Grotesk; keys 11 px, chain 42 px, title up to 88 px | Space Grotesk; **16 / 46 / 92 px** | by eye (sizes: a deliberate change, below) |

## Sound

| Part | Original | Checked by |
|---|---|---|
| The synth | WebAudio oscillators, envelopes, a biquad low-pass bus, a delay | test (`Atom::Synth`: frequency, envelope, filter, delay) |
| The sequence | eighth notes at 96 BPM from 0.1 s | test |
| Flow opens the filter | toward 500 + flow² × 6000 Hz | test |
| Pickups, chime, thud, mute | as `audio.js` | test (audible and sane) |
| **The whole soundtrack** | | **by ear: pending, the user's listening check** |

## Deliberate differences

- **Text is 4 px larger** than the original's CSS (v0.0.13, the user's
  call): the web page scales with the browser; a 1280 × 720 window at
  125 % display scaling made 12 px text small.
- **A window, not a page:** 1280 × 720 by default instead of the browser's
  full viewport; the layout (anchored to the corners and the centre) is
  the same.
- **No cursor over the game** (hidden, not captured), so the pointer
  still steers as in the original.
- **The music starts on the launch click,** as the original's
  `AudioContext` does.
- **Developer facilities** (F1, F10, the `ATOM_DRIFT_*` autopilot) exist
  in development builds only; the package has none (M82).
