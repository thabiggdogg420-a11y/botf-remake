# Workstream 05: Ships, Fleets and Logistics

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*:
- **Ship Design & Progression:** Ships did not use custom modular ship designers; instead, each empire possessed fixed canonical classes that upgraded automatically as technological levels advanced [WIKI, AFC-OV, AFC-SHIP].
- **Ship Roles & Missions:** Defined seven core operational roles [AFC-SHIP]:
  - *Scout:* Long range, high sensors, reveals enemy stats in tactical combat.
  - *Raider / Destroyer:* Fast, medium range, ideal for perimeter patrols and commerce harassment.
  - *Warship / Cruiser:* Heavy firepower, durable shields, core combat line.
  - *Strike Cruiser:* Dedicated long-range torpedo bombardment platform.
  - *Command Ship:* Coordinates fleet targeting, enables manual targeting of enemy ships.
  - *Colony Ship:* Unarmed, medium range, performs planetary terraforming and colonization.
  - *Troop Transport:* Carries invasion ground forces, constructs outposts and starbases.
- **Crew Experience & Academies:** Crews progressed through five distinct ranks: Green, Regular, Veteran, Elite, and Legendary [AFC-SHIP]. Only the Klingon Empire could train crews directly to Legendary [AFC-SHIP]. Empire training facilities provided flat training experience per turn: Starfleet Academy (50), Klingon Tactical College (45), Cardassian Central Command (40), Romulan Naval Academy (35); the Ferengi possessed no training academy [AFC-CAL].
- **Strategic Fleet Orders:** Included Move/Engage, Evade, Avoid, Intercept (patrolling interceptors engaged intruders up to 4 sectors away), Raid (plundering enemy system income), Terraform, Build Outpost/Starbase, and Invade [AFC-CAL, AFC-BSG, AFC-XAV].
- **Logistics & Supply:** Fleets operated within supply radii: Colonies provided supply radius 3, Outposts provided radius 3, Starbases provided radius 5 [AFC-CAL]. Ships venturing beyond supply suffered speed penalties and risk of attrition.

Mods and Remakes:
- **Rise of the UFP:** Added specialized orders including Defend, Idle, Redeploy (rapid strategic redeployment), and distinct task force tactical formations [Rise of the UFP].
- **UDM3:** Integrated carrier wings, escort frigates, and refit modernization refactoring [UDM3].

## 2. Modern Target

1. **Expanded 223-Ship Canon Roster:**
   - Full integration of 223 ship records (125 legacy reference records in `remaster/data/base/ships.json` plus 98 expanded roster classes in `remaster/data/base/roster.json`), categorized into eight canon eras (Enterprise, TOS, Movie, TNG, Dominion War, Nemesis, Kelvin, and Future) (`ShipRoster.h:22-45`, `docs/DATA-FORMAT.md`).
   - 43 buildable canon classes in the Remastered preset with authentic weapon mounts, firing arcs, warp speeds, and sensor ratings (`ShipRosterTypes.h:10-75`).
2. **Comprehensive Strategic Order Palette:**
   - *Set Course:* Warp transit to target sector with calculated pathfinding around hazards.
   - *Attack / Engage:* Aggressive pursuit and engagement of hostile fleets or starbases.
   - *Intercept:* Stationed fleet monitors adjacent sectors (radius 1–2 based on sensors) and automatically intercepts traversing enemy craft.
   - *Raid:* Harass enemy trade lanes and siphon credit revenue from unfortified systems.
   - *Patrol:* Circular route maintaining sensor coverage and suppressing piracy.
   - *Defend / Garrison:* Anchor to a starbase, outpost, or colony, providing defensive bonuses during bombardment.
   - *Redeploy:* High-speed strategic transit between fortified allied starbases at 1.5x standard warp speed.
   - *Evade & Avoid:* Actively avoid combat or flee from superior hostiles.
   - *Terraform, Colonize, Invade, Build Outpost, Upgrade Starbase.*
3. **Dynamic Crew Experience & Combat Veterancy:**
   - Crew rank ladder: Green (+0%), Regular (+5% accuracy/evasion), Veteran (+10%), Elite (+15%), Legendary (+20% accuracy/evasion, +10% critical resistance).
   - Combat engagements, surviving hull damage, and training academies grant crew experience points (`Simulation.h:Ship::crewExperience`).
4. **Logistics, Supply Lines and Allied Basing:**
   - Supply networks projected from Colonies (radius 3), Outposts (radius 3), and Starbases (radius 5) (`Stations.cpp:1-25, 352, 705`).
   - Allied empires with Affiliation or Alliance treaties share supply networks, enabling forward staging operations (`Diplomacy.h:alliedSupply`).
   - Ships stranded "Out of Supply" suffer 50% movement reduction and face random warp engine breakdown chances.
5. **Carrier Small Craft Wings:**
   - Heavy cruisers and carriers embark tactical fighter/runabout wings (Federation Peregrine, Klingon To'Duj, Cardassian Hideki) that deploy during WEGO tactical combat as distinct strike elements (`TacticalCombat.h:carrierWings`).
6. **3D Ship Detail Viewer:**
   - Interactive 3D inspection module in the Fleets screen displaying real-time hull integrity, shield quadrant status, hardpoint arcs, and crew manifest.

## 3. Web-Build Consequences

In compliance with `docs/M0-LANES.md` ("Browser target") and `docs/ART-BIBLE.md` §12:
- **Renderer Fallback (Forward+ vs Compatibility/WebGL2):** Desktop utilizes Forward+ with clustered lighting and high-poly hero meshes. On Web (Compatibility), the 3D ship viewer and strategic fleet meshes render under WebGL2; classes lacking reduced hero models fall back to procedural geometry (`ship_factory.gd`, `hull.gdshader`) with ~4k tris and zero download overhead. Secondary fill and rim lights run unshadowed.
- **Budgets:** Ship hero hulls are strictly budgeted for web: LOD0 12k to 45k tris by size tier, 2 LOD levels (1.0 and 0.25), textures atlased (Base Color 1024, ORM 1024, Emissive 512, normal maps dropped for hulls under 200m), compressed via ETC2/ASTC. Download size: 0.9 to 2.2 MB per hull tier. Ships load via lazily fetched per-empire packs (≤100 MB per empire).
- **No Disk Paths at Runtime:** Ship records load from `data/base/ships.json` and `data/base/roster.json` handed to core as byte buffers (`referenceData::ShipCatalogue`, `roster::ExpandedRoster`, `ShipRoster.h:22-45`). Models load via `res://` packs. No access to `$BOTF_*` env vars or `LocalAssets` at runtime.
- **Saves:** Fleet coordinates, task force hierarchies, strategic orders, and crew experience points serialize to `user://`, mapping to IndexedDB on the web target.

## 4. Player-Facing Surfaces (Screens and Scenes)

- `remaster/godot/scenes/ui/screens/fleets_screen.gd`:
  - Hierarchical Task Force registry organized by sector and operational posture (`fleets_vm.gd`).
  - Stance toggles: Aggressive, Balanced, Defensive, Withdraw.
  - Interactive 3D Ship Inspector sub-viewport showing 3D hull, weapon hardpoints, and crew rank insignia.
  - Split / Merge Task Force controls.
- `remaster/godot/scenes/galaxy/galaxy_map.gd`:
  - 3D spline course lines with turn-arrival markers.
  - Visible supply boundary overlay and intercept engagement bubbles.
- `remaster/godot/scenes/galaxy/orders.gd`:
  - Contextual right-click menu with core validation strings for disabled orders (`orders.gd:40-120`).

## 5. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/Simulation.h`:
  - Extend `Ship` struct with `int32_t crewExperience`, `CrewRank crewRank`, `int32_t taskForceId`, `CarrierWing carrierWing` (`Simulation.h:150-165`).
  - Add `FleetOrder` enum (`ORDER_MOVE`, `ORDER_ATTACK`, `ORDER_INTERCEPT`, `ORDER_RAID`, `ORDER_PATROL`, `ORDER_DEFEND`, `ORDER_REDEPLOY`, `ORDER_COLONIZE`, `ORDER_TERRAFORM`, `ORDER_BUILD_STATION`, `ORDER_INVADE`).
- `remaster/game/Source/BotfSim/Simulation.cpp`:
  - Implement `executeStrategicOrders()` (`Simulation.cpp:1109-1135, 3200-3500`) resolving Intercept triggers, Raid income siphoning, and Redeploy movement speed modifiers.
  - In `advance()` (`Simulation.cpp:1400-1450`), award crew experience to combat survivors and apply academy training points.
  - Update `supplyRange()` calculation to integrate allied treaty sharing (`Simulation.cpp:3771`).
- `remaster/game/Source/BotfSim/Stations.cpp`:
  - Manage Outpost-to-Starbase upgrade orders and station repair dock capabilities (`Stations.cpp:1-25, 246, 705`).
- `remaster/tests/scenarios/ShipRosterTests.cpp`:
  - Add verification suites for crew promotion thresholds, intercept order dispatch, and carrier wing deployment.

## 6. Client Changes

- `remaster/godot/scenes/ui/screens/fleets_screen.gd`:
  - Refactor to consume `res://scripts/viewmodels/fleets_vm.gd`.
  - Embed 3D model turntable allowing 360-degree rotation of selected starships with weapon hardpoint highlights.
- `remaster/godot/scenes/galaxy/orders.gd`:
  - Bind client input handlers for Intercept, Raid, Defend, and Redeploy.
  - Render intercept radar circles on hover over friendly task forces.

## 7. Content and Art Needs (by Asset Key)

- 159 Ship Model Asset Keys: `ship.<faction>.<class>` (e.g., `ship.fed.defiant`, `ship.fed.galaxy`, `ship.fed.sovereign`, `ship.kli.b_rel`, `ship.kli.k_tinga`, `ship.kli.negh_var`, `ship.rom.d_deridex`, `ship.car.galor`, `ship.fer.d_kora`).
- Small Craft Models: `ship.fed.peregrine`, `ship.fed.runabout`, `ship.kli.to_duj`, `ship.car.hideki`.
- Station Models: `ship.fed.starbase`, `ship.kli.starbase`, `ship.rom.starbase`, `ship.car.starbase`, `ship.neutral.outpost`.
- Rank Insignia Icons: `ui.icon.rank.green`, `ui.icon.rank.regular`, `ui.icon.rank.veteran`, `ui.icon.rank.elite`, `ui.icon.rank.legendary`.

## 8. Acceptance Tests

1. **Strategic Intercept Order:**
   - Station a Federation task force with 2 *Defiant*-class escorts in Sector 04-04 and set order to "Intercept".
   - Move an enemy Klingon scout through adjacent Sector 04-05; advance turn.
   - Verify the Federation task force automatically sorties, intercepts the scout in Sector 04-05, and initiates combat.
2. **Crew Experience Promotion:**
   - Engage in tactical combat with a "Regular" crewed cruiser; destroy an enemy heavy cruiser while sustaining minor hull damage.
   - Verify crew gains experience points; upon crossing threshold, crew promotes to "Veteran" with updated UI rank badge and +5% weapon hit rate.
3. **Logistics Supply Debuff:**
   - Move a task force 6 sectors beyond friendly colony/starbase supply boundaries into deep space.
   - Advance turn; verify ships gain "Out of Supply" status, max warp speed drops by 50%, and maintenance cost increases.
4. **Outpost Construction & Starbase Upgrade:**
   - Order a troop transport to "Build Outpost" in a neutral border sector.
   - Wait 3 turns; verify Outpost appears, establishes a 3-sector supply bubble, and can be ordered to "Upgrade to Starbase".
5. **3D Ship Detail Inspector:**
   - Open Fleets screen, select a *Galaxy*-class explorer; rotate the 3D model; toggle weapon arc overlays.
   - Verify forward phaser array and photon torpedo launcher hardpoints highlight in matching LCARS colors.
6. **Web Compatibility & Memory Budget Verification:**
   - Run Fleets screen in a WebGL2 browser export. Inspect hero hulls under the 2-LOD web budget.
   - Verify memory footprint remains within web allocation and fleet data loads cleanly from `data/base/*.json` via `res://` packs without disk errors.

## 9. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Federation roster: Constitution, Miranda, Defiant classes, Troop Transport, and Starbase.
- Klingon roster: B'rel, K't'inga, Vor'cha classes, Troop Transport, and Starbase.
- Crew ranks active: Green, Regular, Veteran.
- Strategic orders: Move/Attack, Intercept, Build Outpost, Terraform, Invade.
- Full 3D ship viewer operational in the Fleets screen.
