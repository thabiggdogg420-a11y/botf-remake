# Workstream 06: Combat

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*:
- **Combat Mechanics:** Tactical fleet combat was turn-based, utilizing a simplified 3D arena derived from the *Falcon 4.0* flight engine [WIKI]. Combat operated in simultaneous 15-second resolution rounds [WIKI, AFC-CAL].
- **Maneuver Orders:** Orders were issued to entire ship groups or individual hulls: Assault (long range), Charge (close range), Harry/Circle, Strafe/Flyby, Evade, Ram, and Retreat [AFC-CAL].
- **Tactical Options:** At the start of an encounter, the player could choose to Take Command (manual tactical), Open Hailing Frequencies (propose ceasefire/truce), or Auto-Resolve [AFC-CAL, WIKI].
- **Cloaking Ambush:** Romulan and Klingon cloaked vessels received a free surprise attack volley during the opening combat round before decloaking [LPA, AFC-CAL].
- **Planetary Assault:** Conquering colonies required planetary bombardment to neutralize orbital batteries and ground invasion using troop transports against planetary defense bunkers [AFC-CAL]. After conquest, empires chose an invasion aftermath policy: Liberate (restoring independence), Subjugate (imposing martial rule), or Exterminate and recolonize [AFC-CAL].
- **Shortcomings Identified by Community and Simon:**
  - Simon, 8 Nov 2026: *"scale is off, torpedos are way wrong, the ships look huge compared to the nearby planet, the battle should be a little more spread out. it looks like your trying to replica oringinal BOTF combat, but that was arguably the original games weakest points. it needs to be a modernized, updated, smartly designed 3d turn based space battle."* (`docs/BATTLE-DESIGN.md:5-13`, `docs/ART-BIBLE.md`).
  - Community criticisms: Combat was rigid, weapons lacked punch, ships bobbed unnaturally like boats on water, and battles degraded into chaotic melees [WIKI, AFC-CAL].

## 2. Modern Target

Folding Simon's explicit decisions (§7 of `docs/MASTER-PLAN.md` and `docs/BATTLE-DESIGN.md` Proposals 1–12):

1. **WEGO Plan-Then-Resolve:**
   - Tactical combat uses a simultaneous planning and execution architecture.
   - *Plan Phase:* Simulation time is paused. Both combatants simultaneously issue waypoint vectors, tactical maneuvers, weapon priorities, and subsystem targeting orders. Visual overlays provide real-time trajectory and range feedback.
   - *Resolve Phase:* Simulation unpauses and plays out deterministically for a 15-second execution window in full 3D space. Weapon tracking, torpedo trajectories, evasive maneuvering, and shield impacts are rendered synchronously (`docs/BATTLE-DESIGN.md` Proposal 11, `TacticalCombat.h:950-1350`).
2. **Four Shield Facings:**
   - Ships feature independent Fore, Aft, Port, and Starboard shield quadrants (`docs/BATTLE-DESIGN.md` Proposal 9).
   - Tactical maneuvering allows ships to protect weakened facings or flank enemies to strike collapsed quadrants. Directional shield impact shaders flash in faction colors.
3. **Subsystem Targeting:**
   - Specific ship subsystems can be targeted:
     - *Engines / Warp Core:* Disabling drops sublight speed to 0, prevents turns, and disables warp retreat.
     - *Weapons Arrays / Torpedo Launchers:* Reduces weapon mount damage output or knocks out launchers.
     - *Shield Emitters:* Prevents shield regeneration on corresponding facings.
     - *Bridge / Sensor Array:* Lowers hit accuracy, disables tactical formations, and blinds task force coordination (`docs/BATTLE-DESIGN.md` Proposal 10).
4. **3D Plan-Phase Overlays:**
   - Translucent movement spline path previews on the tactical plane.
   - Directional firing arc wedges (Forward, Forward-Wide, Broadside, Aft, 360°) projected in LCARS colors.
   - Range rings indicating maximum and optimal beam/torpedo weapon ranges (`docs/BATTLE-DESIGN.md` Proposal 5).
5. **True Scale and Steady Flight:**
   - True-scale ship meshes (lengths 110m to 682m); planets remain distant atmospheric backdrops.
   - Formations spaced 1.7x wider than the standard grid (`BattleScene.SPREAD`).
   - Zero unnatural idle bobbing or pitch oscillation; ships bank smoothly only during turns (`docs/BATTLE-DESIGN.md` Proposals 1, 2).
6. **Cinematic Director & Camera Rig:**
   - Default Tactical Camera frames all combatants from a raised three-quarter angle with 10% margins.
   - Directed cutaways (exchange and kill shots) enforce framing rules: whole hulls in frame, glare caps preventing lens whiteout, and avoiding clipping through shield meshes (`docs/BATTLE-DESIGN.md` Proposals 3, 6).
7. **Hit & Damage Feedback:**
   - Floating damage numbers in LCARS styling.
   - Directional shield flashes, hull plating burns, and persistent fire/plasma venting lasting across rounds on heavily damaged ships (`docs/BATTLE-DESIGN.md` Proposal 7).
8. **Planetary Assault & Invasion Aftermath:**
   - 3D orbital bombardment view targeting planetary batteries and surface shield domes.
   - Ground combat resolver incorporating troop regiments, bunker networks, and civilian militia.
   - Authentic aftermath choices: *Liberate* (restores sovereignty, grants diplomatic trust), *Subjugate* (annexes world under martial law, creates unrest), *Exterminate* (purges population, leaves empty class M world, triggers extreme galactic grievance) (`Simulation.cpp:2921-2970`).

## 3. Per-Era Photoreal Benchmark

In compliance with Simon's ruling (Simon, 8 Nov 2026: *"as close to photorealistic as possible, aiming to look exactly like the shows battle scenes from DS9 dominion war era, or relevant era chosen to play in by the player (meaning TOS era weapons and ships match TOS era shows looks, but modernized to look realistic as possible)"*) and `docs/ART-BIBLE.md`:

- **Show-Accurate Visual Quality:** Tactical combat is held to a photorealistic broadcast standard. Every hull, beam, bolt, and impact must look identical to the television and motion picture sequences, modernised with physically based rendering.
- **Reference Footage by Era:**
  - *TOS Era:* Based on *TOS-R* (remastered effects). Crisp primary-color beam lasers, bright amber photon torpedoes, high-contrast star lighting, and clean hull panels.
  - *Movie Era:* Based on *The Motion Picture* through *The Undiscovered Country*. Rich weapon flares, majestic scale, deep cinematic shadow passes, and dense atmospheric discharges (referencing the Mutara Nebula battle in *TWOK*).
  - *TNG / DS9 Dominion War Era:* Primary visual target. Matches the massive fleet actions in *Deep Space Nine* ("Favor the Bold", "Sacrifice of Angels", "Tears of the Prophets", "What You Leave Behind"). Phased polaron violet beams, amber Federation phaser sweeps, emerald Klingon disruptor bolts, bright red photon torpedoes with trailing plumes, and cyan quantum torpedoes.
  - *Voyager Era:* Transphasic torpedoes, Borg cutting beams, and high-frequency shielding.
  - *Late Era:* Cinematic lighting, deep ambient occlusion, and hard directional keys referencing *Picard* Season 3.
- **Photoreal Rendering Rules:**
  - Physically based materials with micro-panel detailing, aztecing textures, and authentic running lights.
  - Lighting rig: Dominant single hard directional sun key light with crisp shadow maps, cool subtle fill opposite, edge rim light, and minimal ambient fill (`godot/scenes/battle/battle_environment.gd`).
  - Filmic ACES tonemapping with clamped bloom (HDR threshold 1.8, glare budget capped to prevent whiteouts).
  - Show camera language: Long-lens fleet compression shots, fast close-range flybys, and slow tracking pans.
- **Weapons by Era:**
  - Phaser sweep across target hull geometry leaving localized scorch decals.
  - Distinct pulsed disruptor bolts for Klingon/Romulan craft.
  - Curving torpedo ballistics with spherical glowing heads and fading ion trails.
  - Realistic translucent shield impact meshes that ripple across the specific facing struck.
  - Persistent hull breaches venting atmospheric gas and burning plasma plumes on hulls under 40% integrity.
  - Subspace warp flash and spatial distortion on warp entry and exit.

## 4. Web-Build Consequences

In compliance with `docs/M0-LANES.md` ("Browser target") and `docs/ART-BIBLE.md` §12:
- **Renderer Fallback (Forward+ vs Compatibility/WebGL2):** Desktop tactical battles use Forward+ with full volumetric fog, clustered light omni shadows, and screen-space reflections. On Web (Compatibility/WebGL2):
  - Volumetric fog is replaced by 6 to 8 billboard cloud quads (`blend_add`, 512x512 R8) positioned at nebula banks matching tint and drift.
  - `space_sky.gdshader` is pre-baked to a 2048x1024 panorama at pack time.
  - Only the primary star sun light casts shadows; fill and rim lights run unshadowed.
  - Particle burst limits: capped at ≤600 particles per burst across ≤8 emitters (desktop uses 2000 particles across 24 emitters).
  - Spatial shaders (beam, shield, hull, flare, trail) use standard render modes; `cloak.gdshader` uses `hint_screen_texture` which is tested on WebGL2.
  - Anti-aliasing defaults to 2x MSAA with a render scale slider.
- **Budgets:** Ship hero hulls use web LODs (LOD0 12k to 45k tris, 2 LOD levels, 1024 textures, ETC2/ASTC compressed, download 0.9 to 2.2 MB per hull). Audio cues use Ogg q3 32kHz mono for SFX (≤80 KB/clip) and Ogg q2 32kHz mono for voice acknowledgements (≤120 KB/clip).
- **No Disk Paths at Runtime:** Tactical scene meshes, weapon VFX, textures, and audio cues load strictly from `res://` packs or memory buffers. The runtime engine never accesses local filesystem paths (`LocalAssets` or `$BOTF_*`).
- **Saves:** Mid-battle save states and deterministic WEGO replay logs (`BattleRecord`) serialize to `user://`, persisting to IndexedDB on web builds.

## 5. Player-Facing Surfaces (Screens and Scenes)

- `remaster/godot/scenes/battle/battle_scene.gd`:
  - 3D tactical space, true-scale starships, backdrop stars/planets, weapon VFX, and particle explosions.
- `remaster/godot/scenes/battle/battle_view.gd`:
  - WEGO Plan-Phase HUD: Subsystem Target Selector (Engines, Weapons, Shields, Bridge), Maneuver Dial (Assault, Charge, Circle, Flank, Evade, Ram, Retreat), Commit Orders button (`combat_vm.gd`).
- `remaster/godot/scenes/ui/screens/combat_screen.gd`:
  - Tactical battle registry, per-ship health cards (4 shield bars, hull bar, subsystem icons), Hail modal, Auto-Resolve toggle, Combat Log.
- `remaster/godot/scenes/ui/screens/overlays.gd`:
  - Invasion Aftermath Modal (Liberate / Subjugate / Exterminate) with diplomatic consequence warnings.

## 6. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/TacticalCombat.h`:
  - Extend `Ship` struct with `int32_t shieldFacings[4]` (Fore, Aft, Port, Starboard), `int32_t subsystemHealth[4]`, `SubsystemTarget chosenSubsystem`.
  - Add `WEGOOrderBuffer` collecting simultaneous player and AI orders prior to round resolution.
  - In `resolveCombatRound()` (`TacticalCombat.h:950-1350`), calculate vector angles from attacker to defender to apply damage against the correct shield facing; route penetrating damage to targeted subsystems.
  - Implement subsystem failure states (e.g., engine failure halts displacement).
- `remaster/game/Source/BotfSim/Simulation.cpp`:
  - In `resolveInvasions()` (`Simulation.cpp:2921-2970`), implement aftermath policy consequences on population, morale, and empire diplomatic trust matrices.
- `remaster/tests/scenarios/TacticalCombatTests.cpp`:
  - Add unit tests verifying four-facing shield depletion, subsystem targeting damage distribution, and deterministic WEGO replay fingerprints.

## 7. Client Changes

- `remaster/godot/scenes/battle/battle_timeline.gd`:
  - Implement 15-second WEGO synchronized playback coordinator interpolating ship transforms and timing weapon projectile launches.
- `remaster/godot/scenes/battle/battle_fx.gd`:
  - Add directional shield impact bubble shader responding to facing hit angles.
  - Spawn floating combat text (damage numbers, "CRITICAL", "ENGINES DISABLED").
  - Add persistent hull smoke trails and plasma fire emitters linked to damaged ship bones.
- `remaster/godot/scenes/battle/battle_markers.gd`:
  - Render 3D weapon arc cones and range rings projected onto the tactical plane during the Plan Phase.
- `remaster/godot/scenes/battle/battle_camera.gd`:
  - Enforce director shot acceptance rules (framing whole hulls, glare budget cap, avoiding whiteouts).

## 8. Content and Art Needs (by Asset Key)

- Weapon VFX:
  - `vfx.beam.phaser_fed`: Amber/orange continuous beam with bloom glow.
  - `vfx.beam.disruptor_kli`: Bright green pulsed bolt/beam.
  - `vfx.beam.disruptor_rom`: Emerald continuous disruptor beam.
  - `vfx.beam.spiral_car`: Amber spiral wave beam.
  - `vfx.torpedo.photon`: Red/orange glowing sphere head with 0.8s curved flight trail.
  - `vfx.torpedo.quantum`: Brilliant white/cyan pulsing torpedo head.
- Shield & Damage Shaders:
  - `vfx.shield.hit_facing`: Quadrant-specific translucent energy mesh ripple.
  - `vfx.hull.plasma_fire`: Multi-particle persistent hull fire and black smoke puff.
- Audio SFX:
  - `weapon.phaser.fire`, `weapon.disruptor.fire`, `weapon.photon.launch`, `weapon.quantum.launch`.
  - `sfx.shield.impact`, `sfx.hull.breach`, `sfx.subsystem.destroyed`, `sfx.ship.warp_out`.

## 9. Acceptance Tests

1. **WEGO Plan-Then-Resolve Execution:**
   - Enter tactical combat with a Federation cruiser against a Klingon battlecruiser.
   - Verify Plan Phase pauses time; set ship course to flank left and click "Commit Orders".
   - Verify 15-second Resolve phase executes simultaneously, with both ships maneuvering and firing in real time.
2. **Shield Facing Depletion:**
   - Maneuver a ship to continuously fire upon the Port flank of an enemy target.
   - Verify the Port shield gauge drops to 0% and begins taking hull damage, while Fore, Aft, and Starboard shields remain at 100%.
3. **Subsystem Disabling:**
   - Target the "Engines" of an enemy cruiser.
   - Inflict penetrating hull damage until the engine subsystem collapses.
   - Verify floating text displays "ENGINES DISABLED", the enemy ship's sublight speed drops to 0, and it cannot execute turn maneuvers in subsequent rounds.
4. **Plan-Phase Tactical Overlays:**
   - Select a friendly starship during the Plan Phase.
   - Verify weapon firing arc wedges (e.g., Forward 90°, Broadside 60°) and range rings project cleanly onto the 3D plane without visual clipping.
5. **Invasion Aftermath:**
   - Successfully invade an enemy world using troop transports.
   - Select "Liberate"; verify sovereignty is returned to the original minor race and an empire trust boost is logged in the chronicle.
6. **Per-Era Photoreal Benchmark & Show Still Comparison:**
   - Frame and capture 5 distinct battle scenes across eras:
     - *DS9 Dominion War:* Defiant and Galaxy-class ships engaging Jem'Hadar cruisers; compare against stills from "Sacrifice of Angels". Verify phaser beam sweep, torpedo glow, shield mesh ripples, and hull panel specular highlights match show footage.
     - *Movie Era:* Constitution refit and Klingon K't'inga in high-contrast stellar light; compare against *The Wrath of Khan* still captures.
     - *TOS Era:* Enterprise vs D7; compare against *TOS-R* broadcast stills.
   - Acceptance criteria: side-by-side composite passes review with 0 artificial lighting anomalies and authentic era weapon geometry.
7. **Web Compatibility Renderer & IndexedDB Save Test:**
   - Execute a 2-ship tactical engagement in a WebGL2 browser export.
   - Verify battle renders without error under Compatibility mode using billboard cloud quads, capped particle bursts, and unshadowed fill lights.
   - Save mid-battle state, refresh browser, reload from IndexedDB, and verify 100% deterministic round resumption.

## 10. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Tactical duel: Federation (Constitution & Miranda) vs Klingon (K't'inga & B'rel).
- Full WEGO 15-second Plan-then-Resolve execution loop.
- Four independent shield facings with directional hit VFX.
- Subsystem targeting operational for Engines and Weapons.
- Tactical camera default with automated kill cutaway shots.
