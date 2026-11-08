# Workstream 09: AI

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*:
- **Shipbuilding Deficiencies:** The strategic AI suffered from severe biases toward building lightweight craft (strike cruisers, destroyers, and light Ferengi raiders) while rarely investing in top-tier heavy cruisers or battleships [AFC-CAL].
- **Piecemeal Incursions:** Rather than concentrating forces, AI fleets advanced across borders in fragmented single-ship or two-ship detachments, feeding damaged vessels piecemeal into superior defensive lines [AFC-CAL].
- **Erratic & Out-of-Character Diplomacy:**
  - AI factions frequently declared multi-front wars against strong players simultaneously, ignoring territorial vulnerability [AFC-CAL].
  - Klingons frequently sued for peace when suffering minor tactical losses, acting completely counter to their established warrior culture [WIKI].
  - Minor races could be bribed away from established loyalties in a single turn without significant diplomatic counter-actions [AFC-CAL, AFC-XAV].
- **Artificial Difficulty:** Vanilla difficulty settings (from Simple up to "Impossible") did not enhance AI tactical intelligence; instead, they granted massive hidden multipliers to AI credit generation, industry output, and sensor vision [AFC-OV].
- **Current Remaster Core State:**
  - `remaster/game/Source/BotfSim/Simulation.cpp:1323` (`planRivals`) handles rival empire planning.
  - While personality profiles exist in `Diplomacy.h:416-440`, rival AI currently struggles to assemble forces large enough to siege fortified starbases, and balance remains provisional (`docs/research/CURRENT-STATE-AUDIT.md:106`).

Mods and Remakes:
- **UDM3:** Overhauled AI buy priorities to emphasize heavy capital hulls and defensive orbital platforms [UDM3].
- **Birth of the Empires (BotE):** Developed an advanced, non-cheating heuristic strategic AI praised for realistic fleet concentration and strategic patience [BotE].

## 2. Modern Target

1. **Personality-Faithful Strategic Doctrines:**
   - *Klingon Empire:* Aggressive front-line expansion; prioritizes heavy battlecruisers and raptor strike craft; fights to the death when holding sacred ground; values martial glory and despises dishonorable retreats (`Diplomacy.h:416-440`).
   - *Romulan Star Empire:* Deploys cloaked hunter-killer fleets; heavily fortifies border starbases; unleashes devastating espionage and sabotage campaigns before committing ground troops.
   - *Cardassian Union:* Systematic industrial mobilization; fortifies colonies with layered orbital batteries; relies on mass subjugation and internal security networks.
   - *Ferengi Alliance:* Leverages mercantile wealth, commercial monopolies, and mercenary raider swarms; avoids protracted military wars, preferring economic attrition and minor-race bribery.
   - *Federation:* Prioritizes scientific research, defensive starbases, and multi-member diplomatic coalitions; avoids offensive wars unless attacked.
   - *Dominion:* Relentless military doctrine; rapidly produces Jem'Hadar attack swarms backed by massive dreadnoughts; utilizes infiltration agents to destabilize rival borders.
2. **Task Force Massing & Anti-Piecemeal Logistics:**
   - AI aggregates ships into coherent **Strike Task Forces** (minimum 4–8 warships based on era and target) at designated rally points before launching offensive incursions (`Simulation.cpp:1323-1780`).
   - Heavily damaged vessels (hull < 30%) disengage from front-line combat and retreat along secure supply conduits to the nearest shipyard for drydock repairs.
3. **Transparent Strategic Reasoning:**
   - When interacting with AI empires in diplomacy, the player can inspect the exact mathematical and emotional components driving their decisions (e.g., "Relative fleet strength: +15", "Honoring alliance with Romulans: -25", "Grievance: Border intrusion: -10") (`Diplomacy.h:1080-1280`).
4. **Non-Cheating "Expert" Difficulty:**
   - Expert difficulty operates with 1.0x baseline economic output and standard fog-of-war vision rules (`Simulation.cpp:2552`).
   - Competitiveness is achieved entirely through optimized build queues, advanced predictive pathfinding, and coordinated task force maneuvering.
5. **Coordinated WEGO Tactical AI:**
   - Tactical AI analyzes player formations, calculates firing arcs, concentrates beam and torpedo fire on weakened shield quadrants, and executes coordinated subsystem strikes on player engines and weapon arrays (`TacticalCombat.h:1820-1990`).

## 3. Web-Build Consequences

In compliance with `docs/M0-LANES.md` ("Browser target"):
- **Native Core Execution & Threading:** Strategic and tactical AI logic runs within the compiled WebAssembly GDExtension core (`botf_core`). The core must not spawn background OS worker threads unless the host web server supplies cross-origin isolation headers (COOP/COEP). All AI heuristic evaluations, pathfinding traversals, and turn planning algorithms run within strict per-turn execution time limits (budgeted <50ms per turn on an average CPU) to guarantee zero UI frame drop or main-thread browser freezing.
- **Budgets & Memory Bounds:** AI memory structures (path caches, task force registries, and psychological historical records) are strictly bounded to prevent WebAssembly linear memory growth from exhausting browser tab memory limits.
- **No Disk Paths at Runtime:** AI personality matrices, heuristic tables, and tactical weights are compiled into the binary or loaded from `data/base/*.json` byte buffers. Zero runtime filesystem calls, `$BOTF_*` queries, or disk writes occur.
- **Saves:** AI strategic postures, active rally targets, task force mission plans, and diplomatic evaluation parameters serialize to `user://`, persisting to IndexedDB on web builds.

## 4. Player-Facing Surfaces (Screens and Scenes)

- `remaster/godot/scenes/ui/screens/diplomacy_screen.gd`:
  - AI Psychological Dossier: Displays current strategic doctrine, threat assessment gauge, and detailed decision breakdown tooltips (`diplomacy_vm.gd`).
- `remaster/godot/scenes/ui/screens/combat_screen.gd`:
  - Tactical AI intent indicators during the WEGO Plan Phase (e.g., "Klingon task force assuming Flank maneuver targeting Fore shields") (`combat_vm.gd`).
- `remaster/godot/scenes/galaxy/galaxy_map.gd`:
  - Sensor-revealed AI task force rally indicators and border patrol markers.

## 5. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/Simulation.h`:
  - Add `AIStrategicPosture` enum (`POSTURE_TURTLE`, `POSTURE_EXPAND`, `POSTURE_RALLY`, `POSTURE_STRIKE`, `POSTURE_DEFEND`).
  - Add `TaskForceObjective` struct tracking target sector, assigned hulls, and rally progress.
- `remaster/game/Source/BotfSim/Simulation.cpp`:
  - Completely rewrite `planRivals()` (`Simulation.cpp:1323-1780`) to implement task force staging, rally points, and starbase siege doctrines.
  - Implement drydock retreat orders for damaged ships in `executeFleetOrders()` (`Simulation.cpp:1109-1135, 3300-3450`).
- `remaster/game/Source/BotfSim/Diplomacy.h`:
  - Implement `explainDecision(int proposer, int target, ProposalType proposal)` returning a structured list of integer score adjustments for UI rendering (`Diplomacy.h:441-530`).
- `remaster/game/Source/BotfSim/TacticalCombat.h`:
  - Implement tactical AI targeting heuristics in `planAIShipOrders()`: prioritize exposed shield facings and disable player engines when player is retreating (`TacticalCombat.h:1850-1980`).
- `remaster/tests/scenarios/SoakTests.cpp`:
  - Run multi-seed 300-turn AI-vs-AI soak campaigns verifying zero deadlocks, successful starbase captures, and deterministic turn hashes.

## 6. Client Changes

- `remaster/godot/scenes/ui/screens/diplomacy_screen.gd`:
  - Render dynamic score breakdown tooltips on treaty proposal responses consuming `res://scripts/viewmodels/diplomacy_vm.gd`.
- `remaster/godot/scenes/galaxy/galaxy_map.gd`:
  - Update enemy fleet markers to indicate detected task force compositions when within sensor range.

## 7. Content and Art Needs (by Asset Key)

- AI Posture Indicators:
  - `ui.icon.ai.posture_rally`: Rallying task force icon.
  - `ui.icon.ai.posture_strike`: Incursion assault icon.
  - `ui.icon.ai.posture_defense`: Perimeter defense icon.
- Audio Voice Cues:
  - `vo.diplomacy.demand.<fac>`: AI ultimatum voice lines.
  - `vo.diplomacy.taunt.<fac>`: Faction-specific combat taunts.

## 8. Acceptance Tests

1. **Task Force Concentration Before Attack:**
   - Establish a fortified border starbase with 4 defenders.
   - Declare war on the Klingon AI; monitor enemy staging area via long-range sensors.
   - Verify the AI does not send single scouts to attack the starbase; verify it rallies at least 6 warships before initiating a combined assault.
2. **Damaged Ship Retreat:**
   - In a tactical engagement against AI ships, inflict 75% hull damage on an enemy battlecruiser while destroying its escort.
   - Advance to subsequent rounds; verify the damaged battlecruiser disengages, retreats from combat, and charts a course to its nearest shipyard.
3. **Transparent Diplomatic Scoring:**
   - Propose a Non-Aggression Pact to the Romulan Star Empire.
   - Hover over the response; verify an LCARS tooltip details each score factor (Base Trust, Fleet Ratio, Ideology Penalty, Variance) totaling the final decision.
4. **Expert Difficulty Integrity:**
   - Run a 100-turn campaign on Expert difficulty; inspect AI save file state.
   - Verify AI resource income exactly matches its population and structure generation without hidden credit grants.
5. **Tactical Subsystem Focus:**
   - Enter combat with a heavily shielded Federation cruiser; observe tactical AI weapon fire.
   - Verify AI ships concentrate phaser and torpedo fire on a single shield quadrant and target ship engines once shields collapse.
6. **Web Assembly Turn Latency & Determinism Test:**
   - Execute a 50-turn AI-vs-AI campaign inside a WebGL2 browser export.
   - Measure AI turn calculation latency; verify turn execution finishes in <50ms without UI freezing or memory leaks, producing identical state hashes to the native MSVC build.

## 9. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Klingon military AI doctrine fully operational.
- Fleet staging and task force massing active for Klingon fleets against Sol.
- Transparent diplomatic decision scoring visible in the Hail Viewscreen.
- Tactical AI capable of focusing fire on Federation shield facings and subsystem arrays.
