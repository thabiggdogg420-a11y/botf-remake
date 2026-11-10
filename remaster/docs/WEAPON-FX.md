# Weapon effects: the DS9 Dominion War reference spec

Outranked by Simon's recorded words in `AI_stuff\standing\simon\botf-rulings.md` and by these:

- 8 Nov 2026: "as close to photorealistic as possible, aiming to look exactly like the shows battle scenes from DS9 dominion war era, or relevant era chosen to play in by the player".
- 9 Nov 2026: "torpedoes still havent been done properly, phaser either, despite having proper benchmarks to work towards".

The numbers here come from frames measured off the shows in `BATTLE-REFERENCE.md` (DS9 Sacrifice of Angels, The Way of the Warrior, Tears of the Prophets, What You Leave Behind; First Contact for the film era). That file outranks this one on any measured value. It covers the Dominion War era; other eras get their own table when the era switch exists (ART-BIBLE "Combat visual benchmark").

The code is `godot/scenes/battle/battle_fx.gd` (`weapon_style()`, one look per weapon, owner and job kind) with `shaders/weapon_beam.gdshader`, `shaders/torpedo.gdshader`, `shaders/flare.gdshader` and `shaders/shockwave.gdshader`. Timing comes from the battleplay timeline (`battle_timeline.gd`, driven by the core's ticks and resolution order) and is not changed here.

## Units

- **World:** 1 world unit (wu) is about 17.7 m: a Galaxy (642 m) is 36 wu long (`BattleShip.display_length`).
- **Screen:** pixel sizes are at 1080 lines and scale with the viewport height. Every effect has a world size (what it is up close) and a screen floor (what it never shrinks below in a wide shot). Beams and bolts apply the floor per vertex in the shader, so the far end of a long beam keeps it as well.
- **Brightness:** HDR values. Above 1.8 feeds the glow pass (`battle_environment.gd`). The glare budget stays: stacked impacts on one target share one budget, and no frame may have more than 1% of its pixels at pure white.

## What the show does, and what we draw

### Federation phaser (PhaserArray, PhaserBank, PhaseCannon, PulsePhaser)

**In the show (BATTLE-REFERENCE: SoA 1160, 782; WotW 4916):** a pale gold beam with a near-white core, 1/15-1/25 of the target hull wide and only a few pixels across a fleet shot, held 0.6-1.5 s. A segment of the array strip lights up and the glow runs along the strip to one point, which fires. The beam is a thick orange-amber beam with a hot, almost white core and a soft bloom. It flickers slightly as it holds for a second or two, tracking across the target. On shields it makes a bright splash with a ripple across the bubble. On the hull it makes a burn with sparks and venting.

| Part | Spec |
|---|---|
| Charge | 0.30 s before the beam lights. A strip on the hull, perpendicular to the line of fire, 0.22 x hull length (2 to 9 wu), floor 5 px. It glows dim amber, then two bright points run from its ends to the emitter. The emitter is a small hot point, 9 UE with a floor, at 0.55 of the beam's brightness for the whole hold. |
| Beam | A camera-facing ribbon. Full width 1.3 wu (23 m) world, 8 px floor. Profile: core #FFF8E0 at 0.16 of the half width, body #F5CB62 at 0.42, glow #C8781A to the edge, round end caps. Peak HDR: core 2.4, body 1.6, glow 0.8. Ferengi use the same profile with body #FFC21E. When n beams strike one target at once, the last 12 half widths of each dim to 1/sqrt(n) so the meeting point does not white out. |
| Reach and hold | Reaches the target 0.11 s after it lights. Holds for the playback's `beam_hold_seconds` (0.9 s; measured 0.4-1.5 s), fading over the last 25%. |
| Crackle | The core shifts sideways by up to 6% of the width (noise, wavelength about 6 wu). Brightness flickers 0.85 to 1.15 at about 25 Hz. Faint pulses run from the emitter to the target. |
| Tracking | The aim point walks about 40 degrees round the target while the beam holds. |
| Shield hit | A contact splash: a 4-spike flare, white-gold #FFE8A8, max(0.7 x target radius, 44 px), its brightness shared among the beams on that target. The bubble gets a ripple at the moving contact point every 0.25 s (at most 6 at once), coloured as the weapon mixed 45% toward shield blue #80CCFF. On first contact, the SHIELD job's flash shows as well. |
| Hull hit | A contact burn: a white-orange flare, max(0.36 x target radius, 32 px). A continuous spark stream from the contact point (16 particles alive, 0.5 s life; 8 on web). At impact the HULL job's sparks, debris, plasma vent (#B3D9FF) and scorch decal show. |

### Photon torpedo (Federation; Klingon, Cardassian and Ferengi variants)

**In the show (WotW 4891, 4916):** an orange 4-point star with a near-white core and a round halo. No tail and no lens streak. Salvos of 4-5, ~0.1-0.15 s apart. A hit is a small 4-point flare, gone in 0.25 s, with no ring. Torpedoes leave the tube in quick salvos, each with a launch flash. A hit makes a big flash and an expanding shockwave ring.

| Part | Spec |
|---|---|
| Head | A round billboard S across. S = max(8 wu, 48 px). Core #FFE2B8 radius 0.13 S, HDR 2.4, warm inner glow; outer glow in the weapon colour pulsing 0.8-1.2 at 6 Hz. 4 long thin spikes (0.85-1.0 S) near the axes, steady with a slight glint; no streak (quantum keeps a faint one). |
| Colours | Federation #FF8C33 (orange). Klingon #FF2A1A (a deeper red: kept off the green of their disruptors and off pink). Cardassian #FFC21A. Ferengi #9966FF. |
| Motion smear | A faint ribbon 0.3 S long inside the glow (measured: none visible). |
| Salvo | The core's projectile count per volley (the core decides how many). Tubes fire 0.16 s apart (playback). |
| Launch flash | A spiked flare max(1.6 x 8 wu, 67 px) at the tube, HDR 1.6, fading in 0.18 s. |
| Impact | A 4-point flare max(0.8 x target radius, 90 px), the torpedo colour 30% to white, peak HDR 1.2, decaying as exp(-9u). No shockwave ring (rings stay on kills). Hull hits add sparks, debris, venting and a scorch decal. |

### Quantum torpedo

Drawn as the photon torpedo, with these changes:
- glow #8CC0FF and core #F2F8FF;
- a thin ring halo at 0.45 S;
- 6 sharper spikes and a blue-white streak;
- the head is 1.15 S;
- the impact ring is blue-white.

### Klingon disruptor (Disruptor drawn as bolts, DisruptorCannon)

**In the show (SoA 1249; WotW 4805, 4916):** green bolts in pairs from the wing cannons (white-green head, short tapering tail, wide green halo, ~1/4 of a Bird-of-Prey long), and **red** disruptor beams from Vor'cha and K'Vort cruisers.

| Part | Spec |
|---|---|
| Bolt | A capsule. A teardrop (round bright head, tail tapering to 0.3 of the width). Length max(6 wu, 50 px), width max(1.4 wu, 22 px). Core #E6FFE0 (HDR 3.4), body #33FF4D (2.2), glow #10C030 (0.9). The head is brighter than the tail (0.35 to 1). Pulse bands run along it. |
| Burst | 2 to 4 bolts, 0.12 s apart, each 0.34 s in flight (playback). A muzzle flash at the cannon. |
| Beam form | Red: core #FFD6CC, body #FF2A1C, glow #B00808, held 0.25-0.6 s. |
| Impact | A green-tinted flash, with the shield ripple or hull sparks. |

### Romulan (Disruptor, PlasmaCannon, PlasmaTorpedo)

**In the show:** green, heavier and more plasma-like than the Klingon fire.

| Part | Spec |
|---|---|
| Disruptor and plasma bolts | The Klingon bolt at 1.5x width and 1.2x length. Body #33FF99, core #E0FFF0, glow #0FB060. A slow wobble and a softer, wider glow. |
| Plasma torpedo | A diffuse green ball (#40FF80) about 1.3 S, with a churning surface. 4 faint spikes and no horizontal streak. |

### Cardassian (Disruptor, drawn as a beam)

**In the show (Tears 1712; SoA 343, 1186):** a thin yellow-gold beam strung with bright beads that run to the target, held 1.4 s and more.

| Part | Spec |
|---|---|
| Beam | 0.9 of the phaser width. Core #FFF8D8, body #F2D24A, glow #B88A0C. Beads every 4 half widths, running at 3 a second. |

### Dominion (PolaronBeam)

**In the show (SoA 1186):** pale lavender-white beams with a broad blue-violet halo, wider than a phaser. Jem'Hadar fighters fire short white-gold blobs in pairs (SoA 343).

| Part | Spec |
|---|---|
| Beam | 1.5x the phaser width. Core #F8F6FF, body #BFB2FF, glow #6E52FF. A slight stutter. Fighter pulses: blobs 0.45 of a bolt long, core #FFFBEA, body #FFD98A. |

## Draw order

All effects are additive. They depth-test against hulls, so a hull in front hides them, and they never write depth. Layers, back to front:
1. hulls;
2. shield bubble;
3. smoke (alpha);
4. beams, bolts and charge strips (`render_priority` 1);
5. torpedo trails (2);
6. torpedo heads (3);
7. impact flashes, launch flashes and shockwave rings (4).

## Desktop and web

- **Forward+ (desktop)** is held to the full spec.
- **Compatibility (WebGL2)** runs the same shaders. They differ in two ways:
  - Additive output is lifted with an sRGB-like curve, `ALBEDO^(1/2.2)`, because the web glow pass adds about 16 times less on black (battleplay probe).
  - Continuous beam sparks are halved (8 particles, not 16), to stay inside the web VFX budget (ART-BIBLE 12.1).
- Beam and bolt widths use `PROJECTION_MATRIX` in the vertex shader, which both renderers support (its [1][1] is negative under Vulkan, so the shader takes its absolute value). The viewport height cancels out. No screen or depth textures are used.
- Each effect node owns its material with plain uniforms. Instance uniforms were tried and failed on web.

## Distinguishable at a battle zoom

At a battle zoom each weapon reads by shape first and colour second:
- a phaser is a continuous amber line with a white core;
- a torpedo is a round star with spikes and a horizontal streak;
- a disruptor is a green dash that travels.

## Measured (M1 weaponfx, 10 Nov 2026)

- Desktop, Forward+, RTX 4090, 1600x900, no vsync, 24v24 round of full fire: 91 fps average, 59.8 fps 1% low, 45 live effects at peak.
- Known gaps against the show, listed in the lane report: salvos are one torpedo per Fire event in the sandbox (the core decides); focused fire puts up to ten beams on one target; the shield bubble is the existing hex ellipsoid; the kill explosion (not changed here) still whites out a close-up.
