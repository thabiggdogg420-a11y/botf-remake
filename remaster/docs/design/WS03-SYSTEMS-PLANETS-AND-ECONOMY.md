# Workstream 03: Systems, Planets and Economy

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*:
- **Colony Organization:** The star system was the fundamental economic and governing unit [TREKCORE, AFC-CAL]. Population was pooled across the system in millions [TREKCORE].
- **Five Work Areas:** Population was assigned across five distinct jobs: Food, Industry, Energy, Intelligence, and Research [TREKCORE, AFC-CAL]. Unfed or unemployed population died, severely impacting credits and morale [AFC-CAL].
- **Planet Classes & Terraforming:** Included nine standard classes: Class M (Terran/Oceanic), O, P, L, H, K, Y, Gas Giants, and Asteroids [TREKCORE, AFC-CAL]. Colony ships performed terraforming to convert hostile biospheres into Class M equivalents, expanding maximum population capacity [TREKCORE, AFC-BSG].
- **Racial Structure Ladders:** Each empire possessed distinct structure names and progression ladders across the five fields [AFC-STR]:
  - *Food:* Automated Farms (Fed), Hunting Grounds (Kli), Organics Plants (Rom), Hydroponics (Car), Grower's Markets (Fer).
  - *Industry:* Mass Replicators (Fed), Foundries (Kli), Fabricators (Rom), Assembly Yards (Car), Machining Guilds (Fer).
  - *Energy:* Plasma Reactors (Fed), Matter Furnaces (Kli), Gravitic Turbines (Rom), Fusion Plants (Car), Power Exchanges (Fer).
  - *Intel:* Databanks (Fed), Security Centers (Kli), Intel Centers (Rom), Shadow Teams (Car), Data Brokerages (Fer).
  - *Research:* Universities (Fed), Halls of Learning (Kli), Laboratories (Rom), Science Centers (Car), Research Labs (Fer).
- **Special Structures & Dilithium:** Included Dilithium Refineries, Orbital Batteries, Scanners, Shield Generators, and Shipyards [AFC-STR]. Dilithium refineries acted as a strict cap on capital ship construction (roughly one active shipyard build per refinery) [AFC-CAL, AFC-XAV, WIKI].
- **Morale & Emergency Programs:** Morale ranged across eight tiers (Fanatic to Rebellious) [AFC-CAL]. When colonies faced rebellion, empires could invoke Emergency Programs: Federation Martial Law, Klingon Police State, Romulan Tribunal, Cardassian Inquisition, and Ferengi Festival of Fun [TREKCORE].
- **Economy Constraints:** Credits derived from industry and trade paid for upkeep, rush-buying, and bribes; vanilla strictly limited rush-buying to one purchase per system per turn [AFC-CAL, WIKI].

Mods and Remakes:
- **UDM3:** Integrated dilithium into all minor worlds, eliminated the arbitrary single-purchase limit per turn, and added Protectorate HQs for subjugated populations [UDM3].
- **Supremacy:** Added multi-tier resource extraction (Deuterium, Dilithium, Raw Materials) and detailed labour units [Supremacy].

## 2. Modern Target

1. **3D System View:**
   - Replace flat 2D circular representations (`PlanetOrb` in `remaster/godot/scenes/ui/screens/system_screen.gd`) with an embedded 3D viewport.
   - Central luminous star with spectral flares, surrounded by physically scaled, textured 3D orbiting planets, asteroid belts, and visible orbital infrastructure (shipyards, starbases, defense platforms).
2. **Comprehensive Structure Catalog:**
   - Implement the full set of 318 distinct structures defined in `remaster/data/base/buildings.json` (loaded into `referenceData::BuildingCatalogue`, `ReferenceCatalogue.h:13`), respecting racial progression ladders, energy consumption, and population staffing requirements (`docs/DATA-FORMAT.md`).
3. **Strategic Dilithium Economy:**
   - Dilithium is modelled as a physical strategic resource:
     - Deposits occur on specific planetary bodies (Class M, Asteroids, Volcanic).
     - Refineries generate Dilithium units per turn.
     - Each active capital ship requires continuous Dilithium maintenance; new ship construction requires an upfront Dilithium allocation. Deficits trigger severe speed and combat output debuffs (`Simulation.cpp:1080, 5020`).
4. **Emergency Programs:**
   - Implement active empire emergency decrees with tangible tradeoffs:
     - *Federation Martial Law:* Halts rebellion, boosts ground defense +25%, but lowers research by 20% and inflicts long-term trust loss.
     - *Klingon Police State:* Eliminates unrest, boosts industry +15%, but risks random population purges.
     - *Romulan Tribunal:* Maximizes internal security +40%, suppresses dissent, costs credits per turn.
     - *Cardassian Inquisition:* Drastically boosts intelligence generation +50% and forced labor, reduces population growth.
     - *Ferengi Festival of Fun:* Costs large credit sum, raises morale by 2 tiers immediately.
5. **Automation & Governors:**
   - System-level AI governors: *Manual*, *Balanced*, *Growth*, *Research*, *Manufacturing*, and *Defense* (`Simulation.h:30`, `Simulation.cpp:1187-1216, 6015-6230`).
   - Governors automatically reassign population jobs and toggle structure power based on projected shortages and goals, eliminating late-game micromanagement fatigue.
6. **Dynamic Rush Construction:**
   - Allow multiple rush-buys per turn with escalating cost penalties (base cost × 1.5^(purchases_this_turn)), maintaining economic balance.

## 3. Web-Build Consequences

In compliance with `docs/M0-LANES.md` ("Browser target") and `docs/ART-BIBLE.md` §12:
- **Renderer Fallback (Forward+ vs Compatibility/WebGL2):** Desktop 3D system view renders under Forward+ with multi-layer PBR materials (albedo, normal, roughness, night city emissive maps, Rayleigh atmospheric scattering, and rotating cloud shadow projection). On Web (Compatibility), high-poly spheres and normal maps are dropped to stay within WebGL2 fill-rate constraints; spheres fall back to 2k-tri meshes with 1024x512 albedo textures and 512x256 cloud/light layers (~700 KB per planet class). Atmospheric scattering uses a cheap 2-stop Fresnel rim pass.
- **Budgets:** Structure icons are authored as 256x256 on desktop but downsampled and atlased to 128x128 in a single shared atlas of ≤2048 for web builds (~8 KB per icon across 114 building families). System viewport memory is clamped to ensure smooth 60 FPS in-browser rendering.
- **No Disk Paths at Runtime:** All 318 building records, costs, and masks load from `data/base/buildings.json` handed to the core via memory buffer (`referenceData::BuildingCatalogue`, `ReferenceCatalogue.h:13`). Planet textures and UI icons load strictly via `res://` packs. Zero runtime code touches `LocalAssets` or `$BOTF_*` paths.
- **Saves:** Colony workforce allocations, structure queues, emergency decrees, and governor priorities serialize to `user://`, which maps directly to browser IndexedDB in web builds.

## 4. Player-Facing Surfaces (Screens and Scenes)

- `remaster/godot/scenes/ui/screens/system_screen.gd`:
  - 3D interactive System Viewport with planet click-to-focus and orbit visualization.
  - LCARS population job assignment sliders (Food, Industry, Energy, Intel, Research) with real-time output forecasts (`Simulation.cpp:4615-4700`).
  - Structure grid displaying race-specific icons, energy toggle switches, and build queue.
  - Emergency Program button palette with status indicators and cooldown meters.
  - Governor mode dropdown (`system_vm.gd`).
- `remaster/godot/scenes/ui/screens/empire_screen.gd`:
  - Empire-wide resource totals: Credits, Dilithium Stockpile/Upkeep, Energy Grid Reserves, Average Morale.
  - Colony management ledger with multi-system governor assignment (`empire_vm.gd`).
- `remaster/godot/scenes/ui/trade_panel.gd`:
  - Trade route management panel with domestic route allocation and foreign treaty commercial conduits.

## 5. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/Simulation.h`:
  - Extend `EmpireState` with `int32_t dilithiumReserve`, `int32_t dilithiumIncome`, `int32_t dilithiumConsumed` (`Simulation.h:120-135`).
  - Extend `Colony` struct with `EmergencyProgram activeProgram`, `int32_t programDuration`, `int32_t dilithiumOutput`.
  - Add `dilithiumRequired` to `ShipRecord` / `RosterClass`.
- `remaster/game/Source/BotfSim/Simulation.cpp`:
  - In `economy()` (`Simulation.cpp:1080, 5020-5028`), compute empire dilithium balance; block capital ship construction when dilithium is insufficient.
  - In `colonyTurn()` (`Simulation.cpp:4615-4700`), evaluate Emergency Program effects on morale, population growth, and job output.
  - In `BuyConstruction` command handler (`Simulation.cpp:350, 1140, 3512`), support progressive multi-buy formula.
- `remaster/game/Source/BotfSim/ReferencePlanets.h`:
  - Add `hasDilithiumDeposit`, `orbitalRadius`, and `textureAssetKey` to `PlanetClassData` (`ReferencePlanets.h:17-45`).
- `remaster/game/Source/BotfSim/BuildTemplates.cpp`:
  - Support automatic queue population respecting racial building hierarchy ladders (`BuildTemplates.cpp:13, 45-120`).

## 6. Client Changes

- `remaster/godot/scenes/ui/screens/system_screen.gd`:
  - Replace `PlanetOrb` with an embedded `SubViewport` containing a 3D system orbit rig consuming `res://scripts/viewmodels/system_vm.gd`.
  - Bind Dilithium gauge and Emergency Program activation buttons to core client bridge.
- `remaster/godot/scenes/ui/theme/lcars.gd`:
  - Implement dynamic structure card rendering using faction-specific LCARS color schemes.
- `remaster/godot/scenes/ui/end_turn_check.gd`:
  - Add validation checks warning the player if a colony has starvation, energy deficits, or if Dilithium supplies are negative.

## 7. Content and Art Needs (by Asset Key)

- `planet.tex.class_m`: PBR textures (albedo, normal, roughness, night lights, cloud map) for Terran/Oceanic worlds.
- `planet.tex.class_o`: Water world texture set.
- `planet.tex.class_p`: Glacial/Ice world texture set.
- `planet.tex.class_l`: Marginal habitable desert/rock texture set.
- `planet.tex.class_h`: Harsh arid/wasteland texture set.
- `planet.tex.class_k`: Mars-like barren desert texture set.
- `planet.tex.class_y`: Demon/toxic volcanic texture set.
- `planet.tex.gas_giant`: Multilayer banded gas giant textures with animated turbulence.
- `planet.tex.asteroids`: Asteroid field particle mesh and textures.
- `icon.struct.<race>.<type>.<tier>`: 318 distinct LCARS structure icons (e.g., `icon.struct.fed.industry.mass_replicator_1`).
- `ui.icon.program.<type>`: Icons for Martial Law, Police State, Tribunal, Inquisition, and Festival of Fun.

## 8. Acceptance Tests

1. **Dilithium Construction Gate:**
   - Start campaign with 0 free dilithium; attempt to queue a heavy warship (Galaxy or Vor'cha class).
   - Verify construction button is disabled and tooltip states "Insufficient Dilithium Refineries".
   - Complete a Dilithium Refinery structure; verify warship construction becomes buildable.
2. **Emergency Program Execution:**
   - Intentionally cause unrest on a colony until morale tier drops to "Rebellious".
   - Activate the faction Emergency Program (e.g., Federation Martial Law); advance turn.
   - Verify unrest is suppressed, morale stabilizes, and research/growth penalties are accurately applied.
3. **Colony Governor Automation:**
   - Set colony governor to "Manufacturing"; advance turn.
   - Verify population jobs automatically reassign to maximum available Industry slots.
   - Switch governor to "Research"; advance turn; verify population moves to Research slots.
4. **Multiple Rush Purchases:**
   - Rush-buy the current item in the build queue; verify credits deducted at base rate.
   - Rush-buy a second queued item in the same turn; verify the cost reflects the 1.5x escalation multiplier.
5. **3D System Viewport:**
   - Open System screen; click different planets in orbit; verify camera smoothly pans to focus on selected planet with accurate orbital telemetry.
6. **Web Compatibility & IndexedDB State Test:**
   - Run system screen in a WebGL2 browser export. Verify planet spheres render with 1024x512 albedo textures and no shader errors.
   - Reassign workforce sliders and queue items, save game, and reload; verify colony state is restored perfectly from IndexedDB.

## 9. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Sol system for Federation (Earth, Mars, Jupiter) and Qo'noS system for Klingon.
- Full 3D planet rendering for Class M, Barren, and Gas Giant.
- Tiers 1 through 5 of Federation and Klingon racial structure ladders.
- Dilithium economy active and enforced.
- Federation Martial Law and Klingon Police State emergency programs functional.
