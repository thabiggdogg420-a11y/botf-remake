# BOTF Remastered: art bible

Written 8 Nov 2026 by lane L3 of M0. Private project, never distributed.

Sources that outrank this bible:
- Simon's own words, quoted in `research/CURRENT-STATE-AUDIT.md` §3 and in `BATTLE-DESIGN.md`.
- The browser target (Simon, 8 Nov 2026, in `M0-LANES.md`): one Godot project ships as a Windows exe and as a locally served web export.
- `MASTER-PLAN.md` §1 (vision) and §5 workstream 10 (art).

Source tags: `[file:line]` points at the repo (paths under `remaster/` unless stated). `[bible]` marks a rule this document sets for the first time; the art director lane may amend those, every other rule restates what the code or Simon already did. Numbers marked "target" are budgets, not measurements.

## 1. Principles

1. **Original-or-better, then AAA.** "make the end result be AAA quality ATLEAST ... 500x better than the original game" (Simon, 29 Sep 2026, audit §3.2). Faithful content first, the look is the remaster.
2. **One look across every surface.** Galaxy, system, battle, hail and UI share palette, lighting logic and VFX language. A screen that looks like a different game fails review.
3. **Readable before pretty.** Ownership, ship class, range and damage must read at the zoom the player uses (BATTLE-DESIGN rules 2, 3 and the camera notes).
4. **Real hulls, real scale.** "the ships look huge compared to the nearby planet" (Simon, 8 Nov 2026, BATTLE-DESIGN.md line 7). Scale is a rule, not a taste (§2).
5. **Two targets, one source.** Every asset has a desktop budget and a web budget (§12). Nothing is authored that cannot be reduced to its web budget.
6. **Judged by frame, never by tests.** "tests + clean launch log is not a check" (memory: StarhelmBC look-before-handover, 29 Oct). Art status changes only through the review method (§11).

## 2. Scale rules

**World units.** 1 battle world unit is about 17.7 m of ship, 1 tactical cell is 28 units (`BattleScene.CELL`, `godot/scenes/battle/battle_scene.gd:21`). Display length of a hull = canon metres x (120/350) / 170 x 28 world units, so 1 m = 0.0565 units [`godot/scenes/battle/battle_ship.gd:10`, `battle_ship.gd:113-123`]. A Galaxy (642 m) is 36.3 units, 1.3 cells.

**Hull lengths are true.** Every ship key has one canon length in metres in `ShipFactory.CANON_LENGTHS` [`godot/scenes/battle/ship_factory.gd:29`], reproduced in the manifest as `length_m`. The rules:
- A hull is always drawn at its canon length against every other hull. No hull is enlarged to be readable; the camera and plan-phase icons carry readability instead (§6).
- Two exceptions already in code, kept: no hull draws shorter than 60 m (Danube 23 m, Peregrine 30 m, Raven 36 m, Ferengi shuttle 20 m and pod 45 m draw as 60 m) and none longer than 1000 UE units, about 2917 m [`battle_ship.gd:12-13`]. The Narada (8000 m) therefore draws at 2917 m. A model for a clamped hull is still built at its canon length; only the display clamps.
- A hero glb carries its own `lengthMeters`, which wins over the table [`battle_ship.gd:63-67`]. The two must agree to within 2 percent; `asset_status.py --check` does not test this yet, the art director lane compares them in the turntable caption.
- Model axes: nose +X, up +Y, metres, origin at the bounding-box centre after import [`battle_ship.gd:182-194`, hero manifest `forward`/`up`/`units` fields].
- Stations draw as a footprint of 2 cells, not at canon length [`battle_ship.gd:15`].
- Class-to-class size ratios must be visible in one frame: Defiant (171 m) beside Sovereign (685 m) is 4:1.

**Planet against ship.**
- Battle backdrop: a gas giant is a backdrop, never beside a hull. Today radius 700 units placed about 5.3k units from the arena centre [`godot/scenes/battle/battle_environment.gd:104-107`]. Rule [bible]: a planet's radius is at least 25 times the longest hull in the frame, and it sits at least 100 hull-lengths from any ship. 700 units is 19.3 Galaxy lengths of radius; it passes only because the Galaxy is the longest hull in the usual frame, a Narada or Borg cube needs the radius raised to 25 times 165 units, so the radius is set per battle from its longest hull.
- True planet sizes are not drawn: Earth radius 6371 km against a 642 m Galaxy is 9,900 to 1. The 700-unit planet is 12.4 km in ship scale, a deliberate 5-times compression that keeps the planet visible without a ship ever sharing its frame. [bible]
- System view (3D, WS3): schematic scale. Star largest, planets by `EPlanetSize` Small, Medium, Large (`ReferencePlanets.h` EPlanetSize) at 1 : 1.6 : 2.4 diameter, gas giants 4 : 1 over Small, orbit spacing for legibility. Ships appear only as marked icons or tiny silhouettes scaled to stay at least 6 px; the caption says "schematic scale".
- Galaxy view: icons and volumes only, no scale claim.

**Empty space cap.** A battle frame holds whole hulls at a minimum size with no more than 60 percent empty background; the Presentation director enforces this (MASTER-PLAN §4; BATTLE-DESIGN already drops a station from the fit when it would shrink the ships under about 2.2 percent of frame height). [bible]

## 3. Palette per empire

Two palettes exist today and they are the starting point: the LCARS UI palette per empire [`godot/scenes/ui/lcars.gd:24-31`] and the hull look per empire [`ship_factory.gd:11-23`]. Colour-blind owner colours (Okabe-Ito) exist for the five majors [`lcars.gd:33-39`] and stay.

| Empire | UI primary | UI secondary | Map owner | Hull base | Hull accent | Window light | Engine / nacelle glow | Accent glow |
|---|---|---|---|---|---|---|---|---|
| Neutral (0) | #8fa6c8 | #c3cfe0 | #9aa6b8 | n/a | n/a | n/a | n/a | n/a |
| Federation (1) | #6fa8ff | #f2b45a | #5b9dff | (0.70,0.72,0.76) | (0.56,0.59,0.66) | (1.0,0.9,0.7) | (0.25,0.55,1.0) | (1.0,0.25,0.1) |
| Klingon (2) | #e0484a | #c98a4b | #e2463f | (0.33,0.36,0.30) | (0.42,0.30,0.24) | (1.0,0.55,0.25) | (1.0,0.25,0.08) | (0.3,1.0,0.25) |
| Romulan (3) | #3fc98a | #93dcbf | #35c37f | (0.33,0.48,0.40) | (0.25,0.36,0.31) | (0.55,1.0,0.65) | (0.25,1.0,0.45) | (0.3,1.0,0.5) |
| Cardassian (4) | #d9a44a | #e9d6a6 | #d6a13f | (0.58,0.49,0.36) | (0.44,0.36,0.28) | (1.0,0.72,0.25) | (1.0,0.6,0.15) | (1.0,0.75,0.2) |
| Ferengi (5) | #f08a2c | #f5c16c | #f28c28 | (0.62,0.40,0.24) | (0.50,0.32,0.20) | (0.5,1.0,0.55) | (0.3,1.0,0.5) | (1.0,0.7,0.2) |

Rules:
- **Federation** is cool light grey hulls with blue warp glow, red nav lights, amber secondary UI. **Klingon** is dark olive-brown hulls with red-orange engines and green weapon glow. **Romulan** is green-grey hulls with green glow. **Cardassian** is tan and ochre. **Ferengi** is copper with green engine light.
- Hull colour is a tint on a textured hero hull; it must never turn a hull the wrong empire's colour family. Today the hero Klingon Vor'cha reads pale blue-grey in the turntable rig (`turntable.py` capture of `ship.kli.vorcha`, 8 Nov) while the Klingon procedural look is dark olive: that is a defect for the art director lane to fix with the Klingon tint slot, not a colour decision.
- **Weapon colours** are fixed per weapon, then per empire for generic types [`godot/scenes/battle/battle_timeline.gd:398-428`]: Federation phasers orange (1.0,0.45,0.05), Disruptor green (0.15,1.0,0.25) or Cardassian yellow, Federation torpedo red-orange (1.0,0.42,0.12), Klingon torpedo deep red (1.0,0.16,0.1) (M1 weaponfx, 10 Nov 2026: the show's red, kept off their green disruptors), Romulan torpedo and plasma green (0.2,1.0,0.5), Quantum blue (0.4,0.65,1.0), Polaron violet, Temporal magenta (0.8,0.3,1.0). No two empires share a torpedo colour.
- **Minor races and threats** [bible]: minors use the neutral UI palette plus one hull accent each, from the signature of their model; they never use an empire primary. Borg: hull (0.17,0.20,0.17), glow (0.45,1.0,0.45), no windows. Dominion: hull (0.30,0.27,0.40), accent (0.55,0.35,0.75), engine (0.7,0.5,1.0). Species 8472: organic hull (0.45,0.36,0.30) with glow (0.8,1.0,0.2) (matches `BioEnergy`). Breen, Xindi and other mod races take their hull colour from their reference model, one saturated accent for ownership.
- **Colour is never the only carrier of ownership.** Every empire also has an emblem silhouette [`lcars.gd:61`]; the Ferengi emblem list is empty today and needs an outline (manifest `ui.emblem.fer`, status missing).

## 4. Lighting rig

One rig for battle, the turntable and every hull review; the values below are what `BattleEnvironment.build` uses [`godot/scenes/battle/battle_environment.gd`]:

| Light | Colour | Energy | Rotation (deg) | Notes |
|---|---|---|---|---|
| Sun (key) | (1.0,0.93,0.82) | 3.0 | (-24,-128,0) | shadows, 2 parallel splits, bias 0.08, normal bias 2.5 (acne fix) [`:62`] |
| Fill | (0.75,0.8,1.0) | 0.9 | (-35,52,0) | sky_mode light-only, lifts dark Klingon hulls [`:79`] |
| Nebula rim | (0.6,0.66,0.9) | 0.7 | (35,40,0) | edge separation [`:86`] |
| Ambient | (0.26,0.27,0.33) | 1.0 | n/a | colour source, not sky [`:22`] |

Tonemap ACES, exposure 1.0, white 8.0. Glow on: intensity 0.4, HDR threshold 1.8, HDR scale 1.2, screen blend; levels 1 to 6 on [`:27-37`]. These clamps exist because stacked fire whited out frames (R3.5).

Rules [bible]:
- Key from upper left front, cool fill opposite, rim behind: every portrait, hail scene and planet uses the same directions so a planet's lit side matches the battle sun.
- Emissives (windows, engines, nav lights) carry HDR values above 1.8 so glow picks them up; a hull surface is never emissive above 1.0.
- Shadow quality follows the Settings option; the rig must read with shadows off (the turntable renders them on, a second capture with shadows off is part of review of any hull with deep recesses).
- **Desktop vs web:** see §12; the web rig drops volumetric fog and shadows the fill and rim lights.

## 5. LCARS UI style

Reference implementation: `godot/scenes/ui/lcars.gd` (palettes, fonts, theme builders) and `scenes/ui/theme/lcars_<empire>.tres` (five themes). Lane L4 rebuilds the screens on a widget library; this section is the design system it implements.

- **Surface colours:** background #04060c, panels #0a1022 (solid) and 0.9 alpha, panel levels #0e1630 and #131d3c; hairlines (0.55,0.67,0.9) at 13 and 26 percent alpha; text #e7edf9, #b2bfd8, #7c8aa8 [`lcars.gd:6-21`]. Status: good #5fd98f, warn #f5c451, alert #ff5c50, info #7fb3ff, shield #6fd3ff.
- **Accent** is the player's empire primary, applied to rails, frame titles and the active control; the secondary is for highlights; the map owner colour is only for map ownership.
- **Shape:** rounded rectangles, radius 12 by default, elbows with 18 px outer radius on rails [`lcars.gd:142`, `:219`]. Large flat colour blocks are allowed only at rails and headers; content sits on dark panels.
- **Type:** Antonio (display, all-caps headings, rail buttons), Inter (body), Share Tech Mono (numbers and tables). Licences in `godot/scenes/ui/fonts/LICENSE-*.txt`. Minimum body size 14 px at 1.0 UI scale; numerals always mono.
- **Every number has its explanation:** hover or focus shows the source of a figure (MASTER-PLAN §5, WS3). Art supports this with a consistent info glyph.
- **Direct map interaction, right-click menus, pan and zoom are requirements**, not style (audit §3.3).
- **Motion:** 120 to 200 ms ease-out for panels, no looping decoration that competes with the map. No motion option may be unavoidable (accessibility, WS12).
- **Accessibility:** the colour-blind palette is a setting, every status has an icon plus colour, UI scale 0.8 to 1.6 [`lcars.gd` ui_scale].
- **Icons:** one set, 2 px stroke on a 24 px grid, squared ends, drawn once as SVG and rasterised per target (manifest `ui.icon_set`).

## 6. VFX language

Principle: an effect states who did what to whom. Colour says whose weapon, shape says the weapon family, size says the damage.

- **Families:** beams are camera-facing ribbons with a hot core, a body and a glow; pulses are capsule bolts; torpedoes are a spiked star with a lens streak and a short motion smear. Every size, colour, timing and the draw order are in `docs/WEAPON-FX.md` (M1 weaponfx, 10 Nov 2026), built by `battle_fx.gd` `weapon_style()` with `shaders/weapon_beam.gdshader` and `shaders/torpedo.gdshader`.
- **Minimum screen size** (at 1080 lines): a beam never draws under 18 px wide, a bolt under 13 by 60 px, a flare under 14 px, a torpedo head under 48 px. The beam and bolt floor is applied per vertex in `weapon_beam.gdshader`, so the far end of a long beam keeps it too (M1 weaponfx, 10 Nov 2026; WEAPON-FX.md).
- **Glare budget:** all beams on one target share one glare budget; total glare per target is capped (R3.5 whiteout fix) [`battle_fx.gd:185`, `:493`]. No effect may push more than 1 percent of a frame to pure white; BATTLE-DESIGN records measured strips of 0.07 to 0.78 percent.
- **Hit feedback order:** shield flare at the impact point with hex ripple (shield.gdshader, up to 6 simultaneous hits), then hull sparks, then fire and smoke if hull is damaged, then debris at kill. New for WEGO: a hit ripple per facing and a visible subsystem state (manifest `vfx.shield.facing_hit`, `vfx.subsystem.damage`).
- **Explosions:** fireball, smoke, sparks, debris and a shockwave ring, in that sequence, sized by hull length (kill of a Defiant is about a quarter of a Sovereign's). Kill shots hold on the hull for 1.0 to 1.5 s.
- **Warp and cloak:** warp in and out is a stretch and flash along the heading; cloak is a screen-space shimmer (cloak.gdshader). Neither may hide the hull for more than 0.6 s unless it is the cloaked ship.
- **Plan-phase marks** (firing arcs, range rings, target reticles) use the UI palette, never weapon colours, so a plan mark can't be mistaken for a shot.
- **Galaxy anomalies:** each anomaly has a hue and a motion: nebula soft colour cloud (violet, blue or rose by id), ion storm cyan crackle, subspace rift magenta swirl, wormhole cyan spiral, black hole orange accretion disc with black core, neutron star white pulse beam [`godot/scenes/galaxy/galaxy_map.gd:300-330`, `:498-520`]. The ion storm has blocky artefacts today (lane L6 fixes the shader).

## 7. Portrait style

No portraits exist today (audit §2.18: 0 race portraits, 5 vector emblems). The core names an original icon file per race (`RaceCatalogue`, 35 entries, `game/Source/BotfSim/ReferenceCatalogue.h:543`).

- **Two sizes of portrait [bible]:** a *race card* (square 512 desktop at 1024, 35 of them: 5 majors and 30 minors) for lists, the Database and the Diplomacy screen, and a *hail portrait* (5 major leaders, plus one per minor that has hail lines) used in the animated hail scene.
- **Framing:** head and shoulders, three-quarter view toward screen-right, eyes in the upper third, the same horizon on every portrait so a row of them aligns. Same key light direction as §4.
- **Background:** race card sits on the empire's UI panel colour with a faint emblem; the hail portrait sits on a bridge or viewscreen backdrop (manifest `hail.bg.<fac>`), lit with the same rig.
- **Style:** painted-realistic, restrained, no cartoon outlines; species features accurate to the show (Andorian antennae, Ferengi ears) without prosthetic-lab detail that breaks at 128 px. Skin and costume use the empire palette accent so a Klingon portrait never reads Federation.
- **Animation (hail only):** layered parallax of 2 layers on web (face, bust) and 4 on desktop (adds eyes and mouth); idle breathing 0.2 Hz, lip-sync from the voice clip amplitude. No full video, so the web budget holds.
- **Likeness:** no real-actor likeness. Characters are the species and original leaders, consistent with VOICE-PLAN ("No cloning of real actors' voices").

## 8. Planet texture spec

Today: 8 flat circles on the System screen (`system_screen.gd:607` PlanetOrb) and one procedural gas giant (`planet.gdshader`) in battle. Classes in the core: M, O, P, L, H, K, Y, Gas Giant, Asteroids (`Simulation.h:88`), with 8 reference types (`ReferencePlanets.h:53`). The client PlanetOrb also keys J, N and D, which the core does not use; the manifest follows the core.

Per class, a **texture set**:
- Albedo, equirectangular 2:1, seamless left-right, poles pinched cleanly (no seam fan). sRGB.
- Normal map (desktop only), 2:1, relief only; web bakes relief into the albedo.
- Cloud layer, R8 2:1, drifts at 1.3x planet rotation (not for H, K, Y airless variants).
- Night-side lights, RGB 2:1, shown only on colonised planets, scaled by population tier.
- Atmosphere rim: a shader term, not a texture (fresnel power 3, tinted per class), as in planet.gdshader.
- 6 variants per class on desktop, 4 on web, so no system shows two identical neighbours.

Class looks [bible], anchored on the colours the client already draws (PlanetOrb, `system_screen.gd:609`):

| Class | Anchor | Look | Atmosphere rim |
|---|---|---|---|
| M Terran | #4fae6a | blue oceans, green and tan land, white cloud | pale blue |
| O Oceanic | #3f7fd0 | almost all water, small islands, heavy cloud | blue |
| P Glacial | #cfe3f0 | ice caps over rock, blue-white | thin white |
| L Jungle | #7fa35a | dense green, wet haze | green-yellow |
| H Desert | #d0a060 | tan dunes, dry lakebeds, little cloud | orange |
| K Barren | #9a8a80 | cratered grey, no cloud | none |
| Y Demon | #c0503a | red-black rock, lava glow in the night map, toxic cloud | red-orange |
| Gas Giant | #d9a86a | bands and storms, 3 colour sets (tan, blue-green, rust) | blue-white |
| Asteroids | #707078 | 6 rock meshes in a belt, not a sphere | none |

Also needed: 4 star types (yellow, red, blue, white) with a glow sprite, moons (3 meshes), rings for gas giants, a bombarded surface and terraforming stage looks (manifest `planet.layer.*`, `planet.state.*`, `star.*`).

Scale of the sphere: 128 radial segments by 64 rings maximum, LOD to 32 by 16 for planets under 40 px. Planets are lit by the battle sun so the lit side faces the sun direction (-24,-128).

## 9. Rules for the original BOTF art

The original art is Simon's own game copy, used privately.

- **What exists locally:** original audio only (`game/LocalAssets/audio`: 1,536 ogg plus 163 wav, per audit §2.18). No original race icons or other pictures are extracted: `find game/LocalAssets/botf-original -iname "*.tga"` returns 0. The core names the files (`RaceCatalogue` icons such as `human_02.tga`) but the art itself is not on this PC.
- **Never committed.** No original image, model or sound goes in git or in a repo doc; the manifest holds keys and filenames only (HARD-RULES: LocalAssets are never committed; M0-LANES rules).
- **Reference, not source, for ships.** The 27 BOTF-only classes (Liwi, Rtan, Groumall and others) are modelled from the original's references to look as the original showed them, with new geometry. We do not rip original sprites into a texture.
- **Fidelity test:** an original icon or screen is a layout and *silhouette* reference. Our replacement keeps the same recognisability (the Klingon Vor'cha is still an arrow-prowed cruiser) at higher fidelity, not a copy.
- **Original voice and music** are used as they are, privately, from `LocalAssets/audio`; they are the one place the original asset itself ships in the game (VOICE-PLAN §1 source A). On web they are packed into a locally served bundle and are never published.
- **Third-party hero hulls** (33 glb from the Starhelm Bridge Commander library, `game/LocalAssets/ships`) are placeholders until replaced or approved; their licence status is recorded per key in `assets/manifest.json` `permission` fields and none of those candidates is "clear" for redistribution. They stay private.

## 10. Hull specification (hero models)

- **Format:** glb, metres, nose +X, up +Y. Sockets: `WPN_Beam_nn`, `WPN_Torp_nn`, `ENG_nn`, `FX_Shield_Center` (convention in `assets/manifest.json` `socketConvention`; the hero manifest uses `SOCKET_`-prefixed empties).
- **LODs:** desktop 4 (ratios 1.0, 0.5, 0.25, 0.1; existing heroes carry 1 to 4, e.g. `ship.fed.defiant` has 4 [`game/LocalAssets/ships/godot-hero-manifest.json`]). Switch by screen size (`HeroLook.apply_lods`).
- **Materials:** 1 to 3 slots (hull, details, glow); each slot BC, N, ORM, E maps; hull tint from the empire palette; emissive windows and engine on the E map.
- **Detail targets by size tier [bible]:** small (under 100 m) 20k tris, medium (100 to 400 m) 40k, large (400 to 1000 m) 60k, huge (over 1000 m) 100k, station 80k. Existing heroes range 1k (Romulan starbase) to 55k (Defiant), median 16.5k, 550k tris in all 33 [hero manifest, 8 Nov].
- **Naming:** `ship.<fac>.<class>` per `check_asset_keys.py`; the same key names the model, VFX socket set and audio family.
- **Fallback chain:** unchanged: a missing hero falls to the procedural hull, which every key has (`ShipFactory`).

## 11. Review method (judged frame by frame)

The orchestrator and the art director lane judge pictures, not logs. A status in the manifest moves only through these steps.

1. **Capture.** `python remaster/tools/turntable/turntable.py <key>` renders 8 angles (0 to 315 deg, 14 deg elevation) at 384 px per frame on the rig of §4, Dummy audio. For a web check, add `--renderer compat` (or `--renderer both`). Output `captures/turntable/<renderer>/<key>.sheet.png` plus a `.json` naming renderer, source and length.
2. **Frame-by-frame checklist**, each angle looked at, not the sheet as a whole:
   - silhouette recognisable from side, front and top;
   - hull colour inside the empire family (§3);
   - no missing faces, flipped normals, floating parts, z-fighting, or seams;
   - nose at +X in the 0 deg frame, up is up;
   - emissives read but do not bloom to white;
   - length caption matches the canon length (§2);
   - in-game check: one battle frame at tactical zoom and one at close zoom (`screenshot.gd`), because hulls that pass in the turntable have failed in the arena before.
3. **Compare** against the previous sheet of the same key and the empire's lead hull; a regression in any frame is a fail.
4. **Verdict** recorded in the commit message as `review: <key> frames 0-315 ok | fail: <angle, defect>`.
5. **Status change.** Only after a pass does `status` go to `final`. Never in the same session by the author: the author sets `placeholder`, the reviewer sets `final` (HARD-RULES: nobody audits their own work; M0-LANES).

**Status meanings:** `procedural` built by code at run time, `placeholder` a working stand-in not yet through the review, `final` passed the review, `missing` nothing for the key (the manifest adds this fourth value because the audit shows portraits, structure icons and others are missing rather than placeholders). `python remaster/tools/asset_status.py` prints the counts; today 3 of 690 rows are final (the three fonts).

Planets, portraits and icons use the same method with a different capture: planets through the turntable's planet mode, portraits and icons as a contact sheet of all 35 at 128 px (to test the row alignment) plus one full-size.

## 12. Budgets and the Compatibility renderer

Budgets are **targets** set here; the browser target requires them. Every manifest row carries `desktop_budget` and `web_budget`. All numbers need proof on a real web export before they are treated as limits: the first export that exceeds one is a finding, not a failure of the art.

### 12.1 Per asset class

| Class | Desktop (Forward+) | Web (Compatibility, WebGL2) |
|---|---|---|
| Ship hero hull | LOD0 20k to 100k tris by size tier, 4 LODs, textures 2048 per slot (BC, N, ORM, E), S3TC/BPTC | LOD0 12k to 45k tris, 2 LODs (1.0 and 0.25), slots atlased: BC 1024, ORM 1024, E 512, normal dropped under 200 m, ETC2/ASTC. Download 0.9 to 2.2 MB per hull by tier (900, 1200, 1600, 2200 KB; station 1600) |
| Ship procedural (today) | built in code, about 8k tris | about 4k tris, no download, `hull.gdshader` passes |
| Planet class | albedo 4096x2048, normal 2048x1024, 16k-tri sphere, 3 LODs, 6 variants | albedo 1024x512, no normal, 2k-tri sphere, 2 LODs, 4 variants, about 700 KB per class |
| Planet layers (clouds, lights) | 2048x1024 R8 and RGB | 512x256, 120 KB |
| Portraits | 1024x1024 PNG, 4-layer animation | 512x512 lossy WebP, 2 layers, 150 KB each; hail backdrops 1280x720 WebP |
| Structure icons | 256x256 per icon in an atlas | 128x128 in one shared atlas of 2048 or less, about 8 KB each |
| VFX | 2000 particles per burst, 24 emitters | 600 particles per burst, 8 emitters; shaders with at most 4 loop iterations |
| Fonts | woff2 as today | woff2 subset to Latin and Latin-1, 60 KB each |
| Music | Ogg Vorbis q6, 48 kHz stereo | Ogg Vorbis q3, 44.1 kHz stereo, streamed, 2.5 MB per track maximum, fetched lazily per empire pack |
| Voice | Ogg q5, 48 kHz mono, -18 LUFS | Ogg q2, 32 kHz mono, -18 LUFS, 120 KB per clip maximum, per-empire pack on first use |
| SFX | WAV or Ogg q5, 48 kHz mono | Ogg q3, 32 kHz mono, 80 KB per clip maximum; 12 shared cues preloaded |

- **No MP3** anywhere (loop gaps; Ogg is supported on both targets). **No audio format outside Ogg and WAV.**
- **Texture compression:** the project enables ETC2/ASTC import for web and S3TC/BPTC for desktop (`rendering/textures/vram_compression/import_etc2_astc` and `import_s3tc_bptc`), both on so one import serves both exports.
- **Download plan [bible]:** first download at most 60 MB (core wasm, UI, fonts, title, shared SFX, one empire's hulls for the intro). Each empire is a lazily fetched pack of at most 100 MB (hulls, music, voice). Summing the ship rows' web budgets gives about 215 MB for all 159 keys (`asset_status.py --web`, sum of rows that state `download_kb`), which is why packs are per empire and why the M1 Federation and Klingon pack is the first test of the limits. Today's hero hulls total 757 MB on disk (`game/LocalAssets/ships`, 8 Nov), so the web pack step reduces them by about 85 percent or the web build uses procedural hulls for the classes not yet reduced.

### 12.2 Compatibility renderer rules

The Compatibility renderer is OpenGL-based; it differs from Forward+ as follows. Each item names the code it affects and the rule.

- **Volumetric fog is not available.** `BattleEnvironment` turns it on [`battle_environment.gd:43-49`] and `shaders/nebula_fog.gdshader` is a `shader_type fog`. Rule: web uses 6 to 8 billboard cloud quads, `blend_add`, 512x512 R8, in the same place as the nebula bank; the nebula tint and drift match the fog.
- **Sky shader cost.** `space_sky.gdshader` loops 5 octaves plus a 3x3x3 cell search [`space_sky.gdshader:36,54-56`], too heavy per frame on WebGL2. Rule: bake it to a 2048x1024 panorama at pack time; the desktop keeps the live shader.
- **Lights.** Compatibility has tighter per-mesh light limits and costlier shadows than Forward+ (the exact limits are to be read off the 4.5 documentation when the web export is first built; the turntable's three directional lights render fine under Compatibility). Rule: web keeps the sun with shadows and the fill and rim unshadowed.
- **Glow:** the turntable enables the same glow clamps under Compatibility without error; the look was not measured against Forward+, so judge it on the Compatibility sheet. ACES tonemapping stays. No SSAO, SSR, SSIL, DOF, SDFGI or depth-texture effects anywhere: nothing in the art may depend on them.
- **Shaders.** The spatial shaders in `godot/scenes/battle/shaders` (weapon_beam, beam (unused since M1 weaponfx), blink, flare, hero_hull, hero_plated, hull, planet, shield, shockwave, torpedo, trail) use only standard render modes and no screen or depth textures. `cloak.gdshader` uses `hint_screen_texture`, which Compatibility supports; it must be tested live on web (manifest `vfx.cloak`). Canvas shaders in `scenes/galaxy` (field, nebula, starfield) have loops of 4 to 5 octaves; cap at 4.
- **Particles.** GPUParticles3D works on Compatibility; the web budget is the lower burst count of §12.1. CPU particles are the fallback if the web export fails the particle test.
- **MSAA and render scale.** MSAA 4x on desktop; the web defaults to 2x with a render-scale slider (the turntable renders 4x under Compatibility without error; web performance is not measured).
- **Quality tiers live in project settings,** not hard-coded in scenes (browser target).
- **Declared fallback.** Where a feature has none in Compatibility, the manifest row's `web_budget.renderer` field names the replacement (as for `env.battle.nebula_fog` and `env.battle.space_sky`). A shader without a Compatibility pass or a declared fallback is a defect.
- **Verified by looking.** `turntable.py --renderer compat` renders ships and planets under OpenGL (tested 8 Nov: Federation Galaxy, Akira, Vor'cha, gas giant and a class M sphere all render). The compat render differs in one visible way: the same clear colour (5,7,13 in sRGB) shows as a dark blue in Compatibility and as near black in Forward+ (observed 8 Nov, cause not investigated); the art director lane must judge hull values on the Compatibility sheet separately from the desktop one.

## 13. Asset keys and the manifest

`assets/asset-manifest.json` (the existing `assets/manifest.json` is the older UE-era candidate-source list and keeps its role; each ship row cites it). 690 rows:

| Category | Rows | Source of the key list |
|---|---|---|
| ship | 159 | `CANON_LENGTHS` (ship_factory.gd:29); 33 have a hero glb, 126 are procedural |
| planet | 20 | 9 classes, 5 layers, 2 states, 4 stars (11 are planned) |
| portrait | 45 | 35 races, plus 10 planned (5 leaders, 5 hail backdrops) |
| structure_icon | 114 | `BuildingCatalogue` (318 records) grouped into 114 families; 9-tier ladders share one family |
| vfx | 61 | 30 existing keys, 17 effects found in code, 14 planned |
| environment | 4 | sky, nebula fog, light rig, starfield |
| ui | 13 | fonts 3, theme, emblems 6, 3 planned |
| sfx / music / voice | 23 / 26 / 225 | `cues.json` (261 keys) plus planned categories |

Each row: `key`, `category`, `name`, `source`, `status`, `ws` (MASTER-PLAN §5), `milestone`, `desktop_budget`, `web_budget`, optional `planned`. Ship rows add `length_m`, `faction`, `has_hero_model`, `candidate_source`, `hero_lod0_tris`.

**Milestone rule:** M1 for Federation and Klingon hulls and stations, for every planet class, for the generic structure ladders and the effects the WEGO slice uses; M2 for Romulan, Cardassian, Ferengi and vanilla-minor signature hulls, minor race portraits and singleton structures; M3 for the Dominion, Borg, Species 8472 and the mod races; M4 for the end-of-game speeches.

**Audio status rule:** every cue is `placeholder` until picked by ear. SFX and weapon picks were made by spectrum, not by ear [audit §2.16]; tracks 11 to 15 are unidentified; order acknowledgement is not hooked to the order buttons.

**Tools:** `python remaster/tools/asset_status.py [--milestone M1] [--category ship] [--list missing] [--web] [--check]` prints counts by status and category; `python remaster/tools/turntable/turntable.py [--renderer desktop|compat|both] <key...>` produces contact sheets.

## 14. What this bible asks of other lanes

- L4 (client architecture): build the UI on §5; planet and portrait slots must accept an image and fall back to the current circle and emblem.
- L5 (data): `ReferencePlanets` and the minor race table are the key sources; do not rename planet classes.
- L6 (build): the nightly capture includes one turntable sheet per renderer for the M1 hulls; fix the ion storm and set `application/icon`.
- L1 and L2: every design doc's "content and art needs, by asset key" cites keys from the manifest; a key not in the manifest is added there first.

## Combat visual benchmark (Simon, 8 Nov 2026; this outranks the rest of this bible)

Simon's words: "as close to photorealistic as possible, aiming to look exactly like the shows battle scenes from DS9 dominion war era, or relevant era chosen to play in by the player (meaning TOS era weapons and ships match TOS era shows looks, but modernized to look realistic as possible)".

- **Reference footage.** The reference for each era is the show's battle scenes:
  - TOS era: TOS-R (the remastered effects);
  - movie era: TMP to TUC;
  - TNG and DS9 era: the Dominion War episodes;
  - VOY era;
  - late era: Picard season 3 lighting.

  Desktop (Forward+) is held to this bar. Web is a best-effort scale-down of the same look.
- **Weapons by era.** Each look below is set per era:
  - phaser colour, beam width and how a beam sweeps across a hull;
  - disruptor bolts and beams;
  - photon and quantum torpedo glow and flare;
  - the shield bubble on impact;
  - hull burn and scorch decals;
  - venting plasma and atmosphere;
  - the warp flash.

  The tests compare captured frames side by side with stills from the show.
- **Photoreal rules.** Use physically based hull materials with panel detail and running lights, a single hard key light from the local star with deep shadows and almost no ambient fill, and filmic tonemapping. Add camera language from the shows: long-lens fleet shots, ships passing close to camera, and slow pans.
- **Era selection.** The era the player picks sets the ship roster, the weapon and effects look, and the UI theme.
