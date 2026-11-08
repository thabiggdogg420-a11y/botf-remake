# Workstream 10: Art

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*:
- **Display Resolution:** Rendered at a fixed 800×600 resolution [AFC-OV].
- **Ship Visuals:** Tactical combat utilized low-polygon software 3D models with basic texture mapping derived from the *Falcon 4.0* engine [WIKI]. In the strategic view, ships were represented solely by flat 2D icons on the star chart [TREKCORE].
- **Planets & Environments:** In the star system view, planets were depicted as static 2D bitmap discs with flat coloring [TREKCORE, WIKI].
- **Current Remaster State:**
  - The Godot client contains zero committed raster image assets (`docs/research/CURRENT-STATE-AUDIT.md:141`).
  - Star systems render planets as primitive colored circles (`PlanetOrb` in `remaster/godot/scenes/ui/screens/system_screen.gd`).
  - Out of 159 canonical ship asset keys, 126 utilize primitive procedural geometry (`ShipFactory.CANON_LENGTHS`), with only 33 hero models present in gitignored local asset folders (`docs/research/CURRENT-STATE-AUDIT.md:131-133`).
  - Simon, 7 Nov 2026: *"scale is off, torpedos are way wrong, the ships look huge compared to the nearby planet... the 3d battling is a bit shit, the ships bob up and down like boats, and theres no weapon textures to fire?"* (`docs/BATTLE-DESIGN.md:5-10`).
  - Simon, 29 Sep 2026: *"make the end result be AAA quality ATLEAST. ship models, sounds, UI, all of the features of the game should be WAAAAYYY better than the current state of the game, and 500x better than the original game."* (`docs/research/CURRENT-STATE-AUDIT.md:169`).

## 2. Modern Target

Following the standards set in `docs/M0-LANES.md` (Lane L3 Art Bible):

1. **Scale & Proportion Rules:**
   - **True Ship Scaling:** All 159 ship classes are modelled at true canon meter lengths (`remaster/godot/scenes/battle/ship_factory.gd:29`), ranging from small escort raiders (e.g., *B'rel* Bird-of-Prey at 110m, *Defiant* at 170m) to command battlecruisers (*Constitution* at 289m, *Galaxy* at 642m, *Negh'Var* at 682m, *D'deridex* at 1041m).
   - **Cosmic Scale Separation:** In combat scenes, celestial bodies (stars, planets, moons) are placed in distant environmental backdrop layers with authentic visual depth, ensuring planetary spheres realistically dwarf starships without crowding tactical space (`docs/BATTLE-DESIGN.md` Proposal 2).
2. **Physical-Based Rendering (PBR) Lighting Rig:**
   - Single dominant directional key light representing the sector's primary star (color-temperature matching spectral class: warm yellow for Class G, harsh blue-white for Class O/B, deep amber for Class M) (`godot/scenes/battle/battle_environment.gd:62`).
   - Subtle omnidirectional ambient bounce from galactic nebula clouds.
   - Elimination of exaggerated false rim-lighting; hulls use roughness and metallic workflow to catch authentic specular glints along saucer rims and nacelle housings.
3. **LCARS UI Design System:**
   - Modernized, high-resolution vector LCARS design system with responsive scaling (1080p, 1440p, 4K).
   - Strict faction palettes:
     - *Federation:* Blue, Orange, Pale Gold, Lavender.
     - *Klingon:* Crimson, Burnt Amber, Armor Steel, Gold.
     - *Romulan:* Emerald Green, Deep Teal, Charcoal, Cyan.
     - *Cardassian:* Warm Amber, Desert Ochre, Bronze, Dark Slate.
     - *Ferengi:* Deep Gold, Sienna, Tan, Brass.
     - *Dominion:* Deep Violet, Indigo, Magenta, Warm Gold.
4. **Photorealistic 3D Planet Models:**
   - Detailed spherical meshes with five shader layers:
     - Surface albedo texture map with high-resolution topographic relief.
     - Normal map with mountain elevation and oceanic depth.
     - Specular/Roughness map creating oceanic glint and continental matte.
     - Dynamic cloud layer rotating independently with shadow projection on terrain.
     - Atmospheric Rayleigh scattering Fresnel rim and night-side emissive city light networks (`ReferencePlanets.h:17-45`).
5. **Portraits and Character Illustrations:**
   - High-definition 2D/3D character portraits for all 6 major leaders and 30 minor civilizations in authentic bridge settings.
6. **AAA Visual Effects (VFX) Library:**
   - Continuous glowing phaser beams with localized glare bloom.
   - Curved photon and quantum torpedoes with glowing heads and trailing particle wakes.
   - Directional translucent shield impact ripples matching hit angles.
   - Multi-stage ship destruction: interior secondary explosions, hull fractures, debris chunks, and fading plasma fires.
7. **Contact Sheet Turntable Verification:**
   - All 3D assets verified via automated turntable rendering (`remaster/tools/turntable/turntable.py`) across 8 canonical angles prior to integration (`docs/M0-LANES.md:74`).

## 3. Per-Era Photoreal Benchmark

In compliance with Simon's ruling (Simon, 8 Nov 2026: *"as close to photorealistic as possible, aiming to look exactly like the shows battle scenes from DS9 dominion war era, or relevant era chosen to play in by the player (meaning TOS era weapons and ships match TOS era shows looks, but modernized to look realistic as possible)"*) and `docs/ART-BIBLE.md`:

- **Era-Specific Aesthetic Parity:** Visual assets are held strictly to broadcast visual parity across each era:
  - *TOS Era:* Based on *TOS-R*. High-contrast stellar illumination, vibrant primary-color phaser beams, amber photon torpedoes, and clean non-weathered hull aztecing.
  - *Movie Era:* Based on *TMP* to *TUC*. Cinematic theatrical lighting, complex hull plating panels, dramatic lens flares, and dense nebula backdrops (*TWOK* Mutara Nebula).
  - *Dominion War Era:* Primary visual target. Matches the massive fleet actions in *Deep Space Nine*. True-scale fleet formations, phaser sweeps leaving glowing hull scorch decals, violet phased polaron bolts, emerald disruptors, trailing photon torpedo wakes, and cyan quantum torpedoes.
  - *Voyager & Late Era:* Sleek hull materials, transwarp effects, and deep shadow cinematography referencing *Picard* Season 3.
- **Photoreal Rules:**
  - Physically based materials with micro-panel details, authentic aztecing, and glowing running lights.
  - Lighting rig: Key sun from upper-left front with crisp directional shadows, cool fill opposite, subtle rim behind, and minimal ambient fill.
  - Tonemapping: ACES tonemapper with exposure 1.0, white 8.0, HDR threshold 1.8, and clamped bloom glare budget (`godot/scenes/battle/battle_environment.gd`).
  - Camera language: Long-lens fleet compression, low-angle high-speed passes, and slow majestic pans.

## 4. Web-Build Consequences

In compliance with `docs/M0-LANES.md` ("Browser target") and `docs/ART-BIBLE.md` §12:
- **Renderer Fallback (Forward+ vs Compatibility/WebGL2):** Desktop uses Forward+; Web uses Compatibility (WebGL2). Dual texture import is enabled (`import_s3tc_bptc` for desktop, `import_etc2_astc` for web).
- **Per-Asset Class Budgets:**

| Asset Class | Desktop (Forward+) | Web (Compatibility, WebGL2) |
|---|---|---|
| **Ship Hero Hull** | LOD0 20k to 100k tris by size tier, 4 LODs, 2048 textures (BC, N, ORM, E), S3TC/BPTC | LOD0 12k to 45k tris, 2 LODs (1.0 and 0.25), slots atlased: BC 1024, ORM 1024, E 512, normal dropped under 200m, ETC2/ASTC. Download 0.9 to 2.2 MB per hull tier. Procedural fallback (~4k tris). |
| **Planet Class** | Albedo 4096x2048, normal 2048x1024, 16k-tri sphere, 3 LODs | Albedo 1024x512, no normal, 2k-tri sphere, 2 LODs, ~700 KB per class. |
| **Planet Layers** | 2048x1024 R8/RGB clouds and lights | 512x256, 120 KB. |
| **Portraits** | 1024x1024 PNG, 4-layer animation | 512x512 lossy WebP, 2 layers, ≤150 KB each; hail backdrops 1280x720 WebP. |
| **Structure Icons** | 256x256 in atlas | 128x128 in shared ≤2048 atlas, ~8 KB each across 114 families. |
| **VFX & Particles** | 2000 particles/burst, 24 emitters | 600 particles/burst, 8 emitters; shaders capped at ≤4 loop octaves. |

- **Download Budget:** Initial web download is capped at ≤60 MB (core wasm, UI, fonts, shared SFX, starter empire hulls). Additional empire assets load via lazy-fetched per-empire packs (≤100 MB per empire).
- **Declared Fallbacks:** Volumetric fog in battle environments falls back to 6 to 8 billboard cloud quads (512x512 R8, `blend_add`); space sky shader is baked to a 2048x1024 panorama; key sun casts shadows while fill/rim lights remain unshadowed; 2x MSAA with render-scale slider.
- **No Disk Paths at Runtime:** All art assets load strictly via `res://` packs or memory buffers. Zero runtime access to `LocalAssets` or `$BOTF_*` paths.
- **Saves:** Art settings, theme selections, and resolution scale persist to `user://` (IndexedDB).

## 5. Player-Facing Surfaces (Screens and Scenes)

- Master 3D Viewports:
  - `remaster/godot/scenes/battle/battle_scene.gd`: 3D tactical combat arena with PBR lighting and VFX.
  - `remaster/godot/scenes/galaxy/galaxy_map.gd`: 3D galaxy disc, star billboard shaders, and volumetric nebulae.
  - `remaster/godot/scenes/ui/screens/system_screen.gd`: 3D orbiting star system viewport.
  - `remaster/godot/scenes/ui/screens/fleets_screen.gd`: 3D ship turntable inspector.
  - `remaster/godot/scenes/ui/screens/diplomacy_screen.gd`: 3D Hail Viewscreen with bridge backdrops.
- LCARS UI:
  - All screens themed via `remaster/godot/scenes/ui/theme/lcars.gd`.

## 6. Core Changes (Naming Files)

- `remaster/data/base/roster.json` and `remaster/game/Source/BotfSim/ShipRoster.h`:
  - Validate that every ship class record defines correct `lengthMeters`, `assetKey`, and hardpoint arc coordinates (`ShipRoster.h:22-45`, `docs/DATA-FORMAT.md`).
- `remaster/game/Source/BotfSim/ReferencePlanets.h`:
  - Provide texture asset key mappings and atmospheric parameters for all 9 planet classes (`ReferencePlanets.h:17-45`).
- `remaster/data/base/buildings.json` and `remaster/game/Source/BotfSim/ReferenceCatalogue.h`:
  - Provide icon asset key references for all 318 structure records (`ReferenceCatalogue.h:13`).

## 7. Client Changes

- `remaster/godot/scenes/battle/ship_factory.gd` and `hero_look.gd`:
  - Replace procedural primitive generation with a glTF asset loader querying `remaster/assets/manifest.json`.
  - Apply physical PBR materials with bloom-enabled emissive maps for warp nacelles and impulse exhausts.
- `remaster/godot/scenes/ui/screens/system_screen.gd`:
  - Replace 2D `PlanetOrb` with an embedded 3D viewport containing rotating textured planet spheres.
- `remaster/godot/scenes/ui/theme/lcars.gd`:
  - Implement full vector LCARS widget set (pill caps, elbow brackets, segmented data readouts).
- `remaster/godot/scenes/battle/battle_fx.gd`:
  - Implement particle systems for torpedo wakes, shield flashes, beam bloom, and explosion debris meshes.

## 8. Content and Art Needs (by Asset Key)

- 159 Ship Hulls: `ship.<faction>.<class>` (e.g., `ship.fed.galaxy`, `ship.kli.vor_cha`, `ship.rom.d_deridex`, `ship.car.galor`, `ship.fer.d_kora`, `ship.dom.battlecruiser`).
- 9 Planet Texture Sets: `planet.tex.class_m`, `planet.tex.class_o`, `planet.tex.class_p`, `planet.tex.class_l`, `planet.tex.class_h`, `planet.tex.class_k`, `planet.tex.class_y`, `planet.tex.gas_giant`, `planet.tex.asteroids`.
- 36 Character Portraits: `portrait.major.*` (6 keys), `portrait.minor.*` (30 keys).
- 318 Structure Icons: `icon.struct.<race>.<type>.<tier>`.
- VFX Shaders:
  - `vfx.beam.phaser`, `vfx.beam.disruptor`, `vfx.torpedo.photon`, `vfx.torpedo.quantum`.
  - `vfx.shield.facing_impact`, `vfx.explosion.fireball`, `vfx.warp.flash`, `vfx.cloak.distortion`.

## 9. Acceptance Tests

1. **True Scale Proportions:**
   - Place a *Defiant*-class escort (170m) and a *Galaxy*-class cruiser (642m) side-by-side in the tactical scene.
   - Verify visually and via bounding box metrics that the *Galaxy* is roughly 3.7 times longer than the *Defiant*.
2. **Planet PBR Shader Inspection:**
   - Open the System screen for Earth; rotate the camera.
   - Verify oceanic specular reflection glints against the sun, clouds cast shadows onto the landmasses below, and night-side city emissive lights appear as terrain enters shadow.
3. **Weapon VFX Bloom and Flight:**
   - Fire a photon torpedo during tactical combat; verify torpedo manifests as an intense glowing sphere head with a short trailing particle wake, flying on an authentic shallow curve for 0.8 seconds before impact.
4. **LCARS UI Theming:**
   - Switch active empire from Federation to Klingon; verify UI smoothly transitions palette to deep crimson/amber LCARS styling with zero font distortion or layout breaks.
5. **Turntable Contact Sheet Generation:**
   - Execute `python remaster/tools/turntable/turntable.py --renderer both ship.fed.galaxy ship.kli.vor_cha`; verify 8-angle contact sheet renders cleanly under both Desktop and Compatibility modes without texture seams or lighting artifacts.
6. **Per-Era Photoreal Benchmark & Show Still Comparison:**
   - Render 3 tactical combat frames across distinct eras:
     - *Dominion War:* Galaxy, Defiant, and Vor'cha fleet exchange; compare against DS9 "Sacrifice of Angels" battle stills. Verify phaser beam sweep, torpedo glow plumes, shield bubble flashes, and hull specular highlights achieve broadcast fidelity.
     - *Movie Era:* Enterprise refit in stellar key light; compare against *The Wrath of Khan* film stills.
     - *TOS Era:* Enterprise vs D7 battlecruiser; compare against *TOS-R* broadcast stills.
   - Review each frame side by side; confirm frame passes visual review with zero artificial lighting artifacts.
7. **Web Build Asset Budgets & Compatibility Test:**
   - Run the game in a WebGL2 browser export.
   - Inspect loaded asset sizes; verify total initial payload is ≤60 MB, hero hulls stay within 12k–45k tris, and texture slots load ETC2/ASTC formats cleanly without disk read errors.

## 10. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Hero-quality 3D models for Federation (*Constitution*, *Miranda*, *Defiant*, Starbase) and Klingon (*K't'inga*, *B'rel*, *Vor'cha*, Starbase).
- PBR textured Class M world and Gas Giant for the Sol and Qo'noS systems.
- Complete photon torpedo, Klingon disruptor, and phaser beam VFX.
- Federation and Klingon LCARS UI themes.
