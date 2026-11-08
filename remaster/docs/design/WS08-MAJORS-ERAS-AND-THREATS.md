# Workstream 08: Majors, Eras and Threats

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*:
- **Five Playable Major Powers:**
  - *United Federation of Planets:* Unmatched diplomacy, superior research; morale penalized heavily by unprovoked aggression, declarations of war, or orbital bombardment [AFC-XAV, AFC-CAL]. Special ship: *Defiant*-class heavy escort [AFC-XAV].
  - *Klingon Empire:* Morale increases during active warfare; doubled ground defense and population combat values; legendary crew training; cloaking Birds-of-Prey; poor baseline diplomacy [AFC-XAV, AFC-CAL].
  - *Romulan Star Empire:* Cloaked strike fleets; prohibited from orbital bombardment (must conquer worlds via troop invasion); supreme intelligence and internal security [AFC-XAV].
  - *Cardassian Union:* Hardest economic start; rapid industrial replication; intelligence structures outpace research; efficient colony subjugation; Inquisition emergency program [AFC-XAV, WIKI].
  - *Ferengi Alliance:* Can establish trade treaties with minor civilizations without prior friendship; weakest raw research; no military training academy; high credit generation, cheap raider swarms [AFC-XAV, WIKI].
- **Monsters & Galactic Threats:** Roaming hostile non-player entities spawned by random events [AFC-CAL, AFC-XAV]:
  - Calamarain, Chodak Dreadnought, Combat Drones, Husnock Raider, Gomtuu (Tin Man), Tarrellian vessel, Crystalline Entity, Gaseous Life Form, and Sentinel probes [AFC-CAL, AFC-XAV].
  - *The Borg:* Wandered the galaxy with invincible Borg Cubes that destroyed entire fleets and wiped out systems; later official patches added a specific toggle to disable the Borg threat due to player frustration on small maps [AFC-XAV, LPA].

Mods and Remakes:
- **UDM3:** Integrated the Dominion as a fully playable major power replacing the Ferengi, complete with Jem'Hadar cloning facilities, Ketracel-white dependencies, shapeshifter espionage, Borg spheres, and Species 8472 bioships [UDM3].
- **Rise of the UFP & BotE:** Added era starts (TOS through late 24th century) and multi-faction invasion events [Rise of the UFP, BotE].

## 2. Modern Target

Folding Simon's explicit decision (§7 of `MASTER-PLAN.md`): The Dominion is fully playable as a **sixth major power**.

1. **Playable Dominion (6th Major Empire):**
   - *Leadership & Government:* Ruled by the Founders from the Great Link homeworld, administered by cloned Vorta diplomats, defended by genetically engineered Jem'Hadar shock troops (`ShipRosterData.h`, `Diplomacy.h:personalityForEmpire`).
   - *Ketracel-White Resource Mechanic:* Dominion capital ships and ground regiments consume Ketracel-White produced by specialized pharmaceutical facilities. Shortages cause Jem'Hadar mutiny, severe accuracy penalties, and loss of command cohesion.
   - *Jem'Hadar Cloning Yards:* Ground troops and crew replacements train at 3x standard speed with zero morale degradation.
   - *Infiltration Espionage:* Founders can infiltrate rival governments, generating false intelligence reports and triggering civil unrest.
2. **Era Progression & Multi-Era Starts:**
   - Five selectable campaign eras:
     - *Early Warp (22nd Century / Enterprise):* NX-class explorers, primitive plasma weapons, limited warp speeds (Warp 1–5).
     - *TOS / Movie Era (Late 23rd Century):* Constitution, Miranda, D7 battlecruisers, excimer lasers and early photon torpedoes.
     - *Golden Era (Mid 24th Century / TNG):* Galaxy, Ambassador, Vor'cha, D'deridex classes, isolinear computers.
     - *Dominion War (Late 24th Century / DS9):* Sovereign, Defiant, Akira, Negh'Var, Jem'Hadar battlecruisers, quantum torpedoes.
     - *Post-Nemesis (Early 25th Century):* Advanced transwarp tech, Prometheus multi-vector assault, ablative armor.
3. **Tunable Galactic Threats:**
   - *Threat Level Selector:* Configurable at campaign start: *Disabled*, *Normal*, *Apocalypse*.
   - *Borg Incursions:* Autonomous hostile actor. Spawns tactical Borg Cubes and Spheres that assimilate colonies, absorb defeated starship weapon technology, and adapt shields against repeated beam frequencies (`GalacticEvents.cpp:320-370`).
   - *Species 8472 Outbreaks:* Subspace rift anomalies spawn organic bioships that destroy planets rather than occupying them.
4. **Vanilla Monster Roster:**
   - Full implementation of roaming neutral entities with distinct tactical behaviors:
     - *Crystalline Entity:* Energy-leeching radial attacks that drain shields and strip planetary atmospheres.
     - *Gomtuu:* Living sentient vessel that repels attackers with massive telekinetic forcewaves.
     - *Calamarain:* Energy-cloud entity causing severe plasma storms and system power outages.
     - *Husnock & Chodak Raiders:* Ancient dreadnoughts defending forgotten star systems.

## 3. Player-Facing Surfaces (Screens and Scenes)

- `remaster/godot/scenes/ui/screens/new_campaign_screen.gd`:
  - Major Faction Selection: 6 faction cards (Federation, Klingon, Romulan, Cardassian, Ferengi, Dominion).
  - Era Selector: Enterprise, TOS, TNG, Dominion War, Nemesis.
  - Threat Settings: Borg Incursion slider (Disabled/Normal/Apocalypse), Monster Density slider.
- `remaster/godot/scenes/ui/screens/database_screen.gd`:
  - Threat Dossiers and Monster Encyclopedias with full 3D rotatable models and historical lore.
- `remaster/godot/scenes/battle/battle_scene.gd`:
  - Encounter scenes against Borg Cubes, Crystalline Entity, and roaming monsters.

## 4. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/Simulation.h`:
  - Add `EMPIRE_DOMINION = 6` to empire enums (`MaxEmpires = 6`).
  - Add `KetracelWhiteState` to `EmpireState`.
  - Add `ThreatSettings` struct to `CampaignSetup`.
  - Add `MonsterEntity` struct to track roaming space entities in `GameState`.
- `remaster/game/Source/BotfSim/Rules.cpp`:
  - In `newCampaign()`, initialize Dominion starting assets, Vorta administrators, and Ketracel-white inventory.
  - Implement era-appropriate starting technology profiles and ship hull baselines (`Rules.cpp:180-240`).
- `remaster/game/Source/BotfSim/GalacticEvents.cpp`:
  - Implement Borg Cube spawn and assimilation loop (`GalacticEvents.cpp:320-390`).
  - Implement roaming AI pathing for Crystalline Entity, Gomtuu, and Calamarain.
- `remaster/tests/scenarios/RemasterGameplayTests.cpp`:
  - Add test fixtures validating 6-player campaign generation, Dominion resource mechanics, and Borg assimilation triggers.

## 5. Client Changes

- `remaster/godot/scenes/ui/screens/new_campaign_screen.gd`:
  - Add Dominion faction selector card with authentic purple/gold aesthetic and vector emblem.
  - Add Era and Threat configuration controls.
- `remaster/godot/scenes/ui/theme/lcars.gd`:
  - Implement Dominion LCARS theme styling (Violet, Deep Purple, Gold accents) and vector insignia.

## 6. Content and Art Needs (by Asset Key)

- Dominion 3D Hulls:
  - `ship.dom.attack_ship`: Jem'Hadar strike raider.
  - `ship.dom.battlecruiser`: Jem'Hadar heavy warship.
  - `ship.dom.dreadnought`: Massive command dreadnought.
- Threat & Monster 3D Models:
  - `ship.threat.borg_cube`: Fully textured geometric Borg Cube with glowing green interior nodes.
  - `ship.threat.borg_sphere`: Borg scout sphere.
  - `threat.entity.crystalline`: Translucent crystalline lattice structure with internal refractive lighting.
  - `threat.entity.gomtuu`: Organic bio-hull mesh.
  - `threat.entity.calamarain`: Particle-driven glowing energy cloud.
- Portraits:
  - `portrait.major.dominion_founder`, `portrait.major.dominion_vorta`, `portrait.major.dominion_jemhadar`.
- Audio:
  - `vo.advisor.dom`: Weyoun / Female Changeling advisor voice cues.
  - `vo.threat.borg`: Authentic Borg collective assimilation demands ("We are the Borg...").
  - `mus.theme.dominion`: Majestic, militaristic Dominion orchestral theme.

## 7. Acceptance Tests

1. **Dominion Campaign Setup:**
   - Select Dominion as the player empire in the New Campaign screen; start game.
   - Verify capital system is the Founders' Homeworld, starting ships are Jem'Hadar attack ships, and Ketracel-White inventory is tracked on the top bar.
2. **Ketracel-White Depletion:**
   - Advance turns without building Ketracel-White synthesis facilities until reserves hit 0.
   - Verify Jem'Hadar fleets suffer combat mutiny warnings, accuracy drops by 40%, and maintenance costs double.
3. **Era Start Equipment Verification:**
   - Start campaign in the "TOS Era".
   - Verify starting ships are Constitution and Daedalus classes, research begins at Tier 3, and TNG classes (Galaxy/Defiant) are locked.
4. **Borg Incursion Encounter:**
   - Configure Borg threat to "Normal"; advance campaign until a Borg Cube spawns.
   - Engage the Cube in 3D combat; verify Cube absorbs standard phaser damage and deploys cutting beam and tractor beam weapons.
5. **Crystalline Entity Roaming:**
   - Spawn Crystalline Entity event; verify entity moves across sectors along an organic trajectory and strips planetary population if entering an inhabited system.

## 8. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Focus on the classic TNG / Dominion War era duel between Federation and Klingon.
- Threat settings configurable at setup (Disabled or Normal).
- Crystalline Entity implemented as a roaming neutral space monster.
- Dominion faction data structures integrated into the core schema to prevent future breaking migrations.
