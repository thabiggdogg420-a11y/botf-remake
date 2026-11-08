# Workstream 10: Art

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*:
- **Display Resolution:** Rendered at a fixed 800×600 resolution [AFC-OV].
- **Ship Visuals:** Tactical combat utilized low-polygon software 3D models with basic texture mapping derived from the *Falcon 4.0* engine [WIKI]. In the strategic view, ships were represented solely by flat 2D icons on the star chart [TREKCORE].
- **Planets & Environments:** In the star system view, planets were depicted as static 2D bitmap discs with flat coloring [TREKCORE, WIKI].
- **Current Remaster State:**
  - The Godot client contains zero committed raster image assets (`CURRENT-STATE-AUDIT:141`).
  - Star systems render planets as primitive colored circles (`PlanetOrb` in `system_screen.gd:150`).
  - Out of 159 canonical ship asset keys, 126 utilize primitive procedural geometry (`ShipFactory.CANON_LENGTHS`), with only 33 hero models present in gitignored local asset folders (`CURRENT-STATE-AUDIT:131-133`).
  - Simon, 7 Nov 2026: *"scale is off, torpedos are way wrong, the ships look huge compared to the nearby planet... the 3d battling is a bit shit, the ships bob up and down like boats, and theres no weapon textures to fire?"* (`BATTLE-DESIGN.md:5-10`).
  - Simon, 29 Sep 2026: *"make the end result be AAA quality ATLEAST. ship models, sounds, UI, all of the features of the game should be WAAAAYYY better than the current state of the game, and 500x better than the original game."* (`CURRENT-STATE-AUDIT:169`).

## 2. Modern Target

Following the standards set in `M0-LANES.md` (Lane L3 Art Bible):

1. **Scale & Proportion Rules:**
   - **True Ship Scaling:** All 159 ship classes are modelled at true canon meter lengths (`ShipFactory.CANON_LENGTHS`), ranging from small escort raiders (e.g., *B'rel* Bird-of-Prey at 110m, *Defiant* at 170m) to command battlecruisers (*Constitution* at 289m, *Galaxy* at 642m, *Negh'Var* at 682m, *D'deridex* at 1041m).
   - **Cosmic Scale Separation:** In combat scenes, celestial bodies (stars, planets, moons) are placed in distant environmental backdrop layers with authentic visual depth, ensuring planetary spheres realistically dwarf starships without crowding tactical space (`BATTLE-DESIGN.md` Proposal 2).
2. **Physical-Based Rendering (PBR) Lighting Rig:**
   - Single dominant directional key light representing the sector's primary star (color-temperature matching spectral class: warm yellow for Class G, harsh blue-white for Class O/B, deep amber for Class M).
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
     - Atmospheric Rayleigh scattering Fresnel rim and night-side emissive city light networks (`ReferencePlanets.h`).
5. **Portraits and Character Illustrations:**
   - High-definition 2D/3D character portraits for all 6 major leaders and 30 minor civilizations in authentic bridge settings.
6. **AAA Visual Effects (VFX) Library:**
   - Continuous glowing phaser beams with localized glare bloom.
   - Curved photon and quantum torpedoes with glowing heads and trailing particle wakes.
   - Directional translucent shield impact ripples matching hit angles.
   - Multi-stage ship destruction: interior secondary explosions, hull fractures, debris chunks, and fading plasma fires.
7. **Contact Sheet Turntable Verification:**
   - All 3D assets verified via automated turntable rendering (`remaster/tools/turntable/`) across 8 canonical angles prior to integration (`M0-LANES:74`).

## 3. Player-Facing Surfaces (Screens and Scenes)

- Master 3D Viewports:
  - `scenes/battle/battle_scene.gd`: 3D tactical combat arena with PBR lighting and VFX.
  - `scenes/galaxy/galaxy_map.gd`: 3D galaxy disc, star billboard shaders, and volumetric nebulae.
  - `scenes/ui/screens/system_screen.gd`: 3D orbiting star system viewport.
  - `scenes/ui/screens/fleets_screen.gd`: 3D ship turntable inspector.
  - `scenes/ui/screens/diplomacy_screen.gd`: 3D Hail Viewscreen with bridge backdrops.
- LCARS UI:
  - All screens themed via `scenes/ui/theme/lcars.gd`.

## 4. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/ShipRosterData.h`:
  - Validate that every ship class record defines correct `lengthMeters`, `assetKey`, and hardpoint arc coordinates (`ShipRosterData.h:35-310`).
- `remaster/game/Source/BotfSim/ReferencePlanets.h`:
  - Provide texture asset key mappings and atmospheric parameters for all 9 planet classes.
- `remaster/game/Source/BotfSim/ReferenceCatalogue.h`:
  - Provide icon asset key references for all 318 structure records.

## 5. Client Changes

- `remaster/godot/scenes/battle/ship_factory.gd` and `hero_look.gd`:
  - Replace procedural primitive generation with a glTF asset loader querying `remaster/assets/manifest.json`.
  - Apply physical PBR materials with bloom-enabled emissive maps for warp nacelles and impulse exhausts.
- `remaster/godot/scenes/ui/screens/system_screen.gd`:
  - Replace 2D `PlanetOrb` with an embedded 3D viewport containing rotating textured planet spheres.
- `remaster/godot/scenes/ui/theme/lcars.gd`:
  - Implement full vector LCARS widget set (pill caps, elbow brackets, segmented data readouts).
- `remaster/godot/scenes/battle/battle_fx.gd`:
  - Implement particle systems for torpedo wakes, shield flashes, beam bloom, and explosion debris meshes.

## 6. Content and Art Needs (by Asset Key)

- 159 Ship Hulls: `ship.<faction>.<class>` (e.g., `ship.fed.galaxy`, `ship.kli.vor_cha`, `ship.rom.d_deridex`, `ship.car.galor`, `ship.fer.d_kora`, `ship.dom.battlecruiser`).
- 9 Planet Texture Sets: `planet.tex.class_m`, `planet.tex.class_o`, `planet.tex.class_p`, `planet.tex.class_l`, `planet.tex.class_h`, `planet.tex.class_k`, `planet.tex.class_y`, `planet.tex.gas_giant`, `planet.tex.asteroids`.
- 36 Character Portraits: `portrait.major.*` (6 keys), `portrait.minor.*` (30 keys).
- 318 Structure Icons: `icon.struct.<race>.<type>.<tier>`.
- VFX Shaders:
  - `vfx.beam.phaser`, `vfx.beam.disruptor`, `vfx.torpedo.photon`, `vfx.torpedo.quantum`.
  - `vfx.shield.facing_impact`, `vfx.explosion.fireball`, `vfx.warp.flash`, `vfx.cloak.distortion`.

## 7. Acceptance Tests

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
   - Execute `remaster/tools/turntable/render_turntable.py` for 5 distinct ship classes; verify 8-angle contact sheet renders cleanly without texture seams or lighting artifacts.

## 8. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Hero-quality 3D models for Federation (*Constitution*, *Miranda*, *Defiant*, Starbase) and Klingon (*K't'inga*, *B'rel*, *Vor'cha*, Starbase).
- PBR textured Class M world and Gas Giant for the Sol and Qo'noS systems.
- Complete photon torpedo, Klingon disruptor, and phaser beam VFX.
- Federation and Klingon LCARS UI themes.
