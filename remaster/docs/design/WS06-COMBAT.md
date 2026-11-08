# Workstream 06: Combat

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*:
- **Combat Mechanics:** Tactical fleet combat was turn-based, utilizing a simplified 3D arena derived from the *Falcon 4.0* flight engine [WIKI]. Combat operated in simultaneous 15-second resolution rounds [WIKI, AFC-CAL].
- **Maneuver Orders:** Orders were issued to entire ship groups or individual hulls: Assault (long range), Charge (close range), Harry/Circle, Strafe/Flyby, Evade, Ram, and Retreat [AFC-CAL].
- **Tactical Options:** At the start of an encounter, the player could choose to Take Command (manual tactical), Open Hailing Frequencies (propose ceasefire/truce), or Auto-Resolve [AFC-CAL, WIKI].
- **Cloaking Ambush:** Romulan and Klingon cloaked vessels received a free surprise attack volley during the opening combat round before decloaking [LPA, AFC-CAL].
- **Planetary Assault:** Conquering colonies required planetary bombardment to neutralize orbital batteries and ground invasion using troop transports against planetary defense bunkers [AFC-CAL]. After conquest, empires chose an invasion aftermath policy: Liberate (restoring independence), Subjugate (imposing martial rule), or Exterminate and recolonize [AFC-CAL].
- **Shortcomings Identified by Community and Simon:**
  - Simon, 8 Nov 2026: *"scale is off, torpedos are way wrong, the ships look huge compared to the nearby planet, the battle should be a little more spread out. it looks like your trying to replica oringinal BOTF combat, but that was arguably the original games weakest points. it needs to be a modernized, updated, smartly designed 3d turn based space battle."* (`BATTLE-DESIGN.md:5-13`).
  - Community criticisms: Combat was rigid, weapons lacked punch, ships bobbed unnaturally like boats on water, and battles degraded into chaotic melees [WIKI, AFC-CAL].

## 2. Modern Target

Folding Simon's explicit decisions (§7 of `MASTER-PLAN.md` and `BATTLE-DESIGN.md` Proposals 1–12):

1. **WEGO Plan-Then-Resolve:**
   - Tactical combat uses a simultaneous planning and execution architecture.
   - *Plan Phase:* Simulation time is paused. Both combatants simultaneously issue waypoint vectors, tactical maneuvers, weapon priorities, and subsystem targeting orders. Visual overlays provide real-time trajectory and range feedback.
   - *Resolve Phase:* Simulation unpauses and plays out deterministically for a 15-second execution window in full 3D space. Weapon tracking, torpedo trajectories, evasive maneuvering, and shield impacts are rendered synchronously (`BATTLE-DESIGN.md` Proposal 11, `TacticalCombat.h:580-690`).
2. **Four Shield Facings:**
   - Ships feature independent Fore, Aft, Port, and Starboard shield quadrants (`BATTLE-DESIGN.md` Proposal 9).
   - Tactical maneuvering allows ships to protect weakened facings or flank enemies to strike collapsed quadrants. Directional shield impact shaders flash in faction colors.
3. **Subsystem Targeting:**
   - Specific ship subsystems can be targeted:
     - *Engines / Warp Core:* Disabling drops sublight speed to 0, prevents turns, and disables warp retreat.
     - *Weapons Arrays / Torpedo Launchers:* Reduces weapon mount damage output or knocks out launchers.
     - *Shield Emitters:* Prevents shield regeneration on corresponding facings.
     - *Bridge / Sensor Array:* Lowers hit accuracy, disables tactical formations, and blinds task force coordination (`BATTLE-DESIGN.md` Proposal 10).
4. **3D Plan-Phase Overlays:**
   - Translucent movement spline path previews on the tactical plane.
   - Directional firing arc wedges (Forward, Forward-Wide, Broadside, Aft, 360°) projected in LCARS colors.
   - Range rings indicating maximum and optimal beam/torpedo weapon ranges (`BATTLE-DESIGN.md` Proposal 5).
5. **True Scale and Steady Flight:**
   - True-scale ship meshes (lengths 110m to 682m); planets remain distant atmospheric backdrops.
   - Formations spaced 1.7x wider than the standard grid (`BattleScene.SPREAD`).
   - Zero unnatural idle bobbing or pitch oscillation; ships bank smoothly only during turns (`BATTLE-DESIGN.md` Proposals 1, 2).
6. **Cinematic Director & Camera Rig:**
   - Default Tactical Camera frames all combatants from a raised three-quarter angle with 10% margins.
   - Directed cutaways (exchange and kill shots) enforce framing rules: whole hulls in frame, glare caps preventing lens whiteout, and avoiding clipping through shield meshes (`BATTLE-DESIGN.md` Proposals 3, 6).
7. **Hit & Damage Feedback:**
   - Floating damage numbers in LCARS styling.
   - Directional shield flashes, hull plating burns, and persistent fire/plasma venting lasting across rounds on heavily damaged ships (`BATTLE-DESIGN.md` Proposal 7).
8. **Planetary Assault & Invasion Aftermath:**
   - 3D orbital bombardment view targeting planetary batteries and surface shield domes.
   - Ground combat resolver incorporating troop regiments, bunker networks, and civilian militia.
   - Authentic aftermath choices: *Liberate* (restores sovereignty, grants diplomatic trust), *Subjugate* (annexes world under martial law, creates unrest), *Exterminate* (purges population, leaves empty class M world, triggers extreme galactic grievance) (`Simulation.cpp:resolveInvasion`).

## 3. Player-Facing Surfaces (Screens and Scenes)

- `remaster/godot/scenes/battle/battle_scene.gd`:
  - 3D tactical space, true-scale starships, backdrop stars/planets, weapon VFX, and particle explosions.
- `remaster/godot/scenes/battle/battle_view.gd`:
  - WEGO Plan-Phase HUD: Subsystem Target Selector (Engines, Weapons, Shields, Bridge), Maneuver Dial (Assault, Charge, Circle, Flank, Evade, Ram, Retreat), Commit Orders button.
- `remaster/godot/scenes/ui/screens/combat_screen.gd`:
  - Tactical battle registry, per-ship health cards (4 shield bars, hull bar, subsystem icons), Hail modal, Auto-Resolve toggle, Combat Log.
- `remaster/godot/scenes/ui/screens/overlays.gd`:
  - Invasion Aftermath Modal (Liberate / Subjugate / Exterminate) with diplomatic consequence warnings.

## 4. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/TacticalCombat.h`:
  - Extend `Ship` struct with `int32_t shieldFacings[4]` (Fore, Aft, Port, Starboard), `int32_t subsystemHealth[4]`, `SubsystemTarget chosenSubsystem`.
  - Add `WEGOOrderBuffer` collecting simultaneous player and AI orders prior to round resolution.
  - In `resolveCombatRound()` (lines 1100–1450), calculate vector angles from attacker to defender to apply damage against the correct shield facing; route penetrating damage to targeted subsystems.
  - Implement subsystem failure states (e.g., engine failure halts displacement).
- `remaster/game/Source/BotfSim/Simulation.cpp`:
  - In `resolveInvasion()` (lines 4600–4780), implement aftermath policy consequences on population, morale, and empire diplomatic trust matrices.
- `remaster/tests/scenarios/TacticalCombatTests.cpp`:
  - Add unit tests verifying four-facing shield depletion, subsystem targeting damage distribution, and deterministic WEGO replay fingerprints.

## 5. Client Changes

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

## 6. Content and Art Needs (by Asset Key)

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

## 7. Acceptance Tests

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

## 8. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Tactical duel: Federation (Constitution & Miranda) vs Klingon (K't'inga & B'rel).
- Full WEGO 15-second Plan-then-Resolve execution loop.
- Four independent shield facings with directional hit VFX.
- Subsystem targeting operational for Engines and Weapons.
- Tactical camera default with automated kill cutaway shots.
