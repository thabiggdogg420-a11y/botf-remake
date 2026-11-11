# MOD-5: threat actors (Borg, Species 8472, the vanilla monsters)

Lane MOD-5 (Barclay, 12 Nov 2026). Outranked by `AI_stuff\standing\simon\botf-rulings.md` (11 Nov: every vanilla feature plus the five mod packs as presets; the Borg
"Playable too", so both an empire and a threat). Sources: `research/MOD-PARITY.md` rows X14, X15, X16, X24, E04 and `research/PARITY-MATRIX.md` (Space Entities and
Monsters, Borg Invasion Event). Code: `game/Source/BotfSim/Threats.h`, `Threats.cpp`; hooks in `Simulation.cpp`, `TacticalCombat.h`, `Rules.cpp`, `Presets.h`;
bridge `web/bridge/WebBridge.cpp`; client `godot/`. Schema 20 (lane MOD-3 uses 19; renumbered at merge). Private, not for distribution.

## 1. The model

A **threat actor** is a map object, not a fleet and not an empire: `ThreatActor {id, kind, x, y, targetX, targetY, hull, shields, spawnTurn, adapt, siege, kills}`.
`GameState` holds `threats` (the switches), `threatActors`, `nextThreatId`, `threatLastSpawn[12]` and `assimilated`. Keeping actors out of `fleets` means no fleet
code (supply, upkeep, AI, task forces, victory) ever sees one. In a battle an actor is a **proxy combatant**: fleet id `ThreatCombatantBase (3,000,000) + id`,
side `ThreatOwner (9)`, stats written over the proxy as `Stations.cpp` does for a station. So it fights through the existing tactical engine (rules 2, 3 and 4,
player-commanded, auto-resolved or replayed in a turn session) with no second combat model. A threat is at war with everyone; it never retreats.

The twelve kinds (`Threats.h`, stats from `ships.json` ids 115 to 124 and the roster): Borg cube (115), Borg sphere (roster 1600), Species 8472 bioship (1714),
Husnock (122), Calamarain (116), Chodak (117), Crystalline Entity (118), Echo Papa 607 (119), Edo Guardian (120), Gomtuu (121), Tarellian (124) and Deep Space Nine
(roster Nor 1303, the peaceful replacement for the Edo Guardian).

## 2. Turn flow and determinism

`threats::step` (after fleet movement, on the moving copy) removes actors whose lifespan is up, retargets and moves the rest, then rolls spawns.
`threats::battles` runs inside `resolveTacticalBattles` before the empire-vs-empire duels (so a session resumes it unchanged): per actor, one duel against each
empire's fleets in its sector, empire ids ascending, wounds carried from duel to duel. `threats::afterCombat` (on the carried state) does sieges, assimilation,
bombardment, regeneration and the Borg-empire rules. Every random draw comes from `rngState` and only when a switch is on, so a game with the switches off consumes no
draw and writes no state; the default game is unchanged apart from the save header. A moving actor goes toward its target at its speed (one step at a time, round
black holes and subspace rifts); a hunter retargets the nearest colony each turn (ties by system id), a roamer a random sector, a drifter does the same on alternate
turns, a guard never moves. Static actors (Echo Papa, Edo Guardian or DS9) appear once, at turn 1, in empty deep space away from colonies.

| Kind | From turn | Chance per turn (normal) | Gap | Speed | Behaviour | Fights | Effect on a colony in its sector | Leaves after |
|---|---|---|---|---|---|---|---|---|
| Borg cube | 50 | 4% | 40 | 1 | hunt | yes | assimilate | 90 turns or after 3 + frequency systems |
| Borg sphere | 35 | 5% | 30 | 2 | hunt | yes | assimilate | 70 turns, same |
| Species 8472 | 45 | 4% | 35 | 2 | hunt | yes | bombard: a third of the population a turn, the world is destroyed | 50 |
| Husnock | 30 | 3% | 40 | 1 | hunt | yes | raid: a tenth of the population, a battery, -10 morale; never destroys | 60 |
| Calamarain | 10 | 3% | 30 | 1 | drift | fleets only | none | 45 |
| Chodak | 20 | 3% | 40 | 1 | roam | yes | raid | 50 |
| Crystalline Entity | 25 | 3% | 45 | 1 | hunt | yes | strip: a sixth of the population, gone below 5 | 55 |
| Echo Papa 607 | 1 | once | - | 0 | guard | yes | none | never |
| Edo Guardian | 1 | once | - | 0 | guard | when a fleet shares its sector | none | never |
| Gomtuu | 15 | 2% | 50 | 1 | drift | never | none | 60 |
| Tarellian | 5 | 2.5% | 40 | 1 | drift | when a fleet shares its sector | none | 40 |
| Deep Space Nine | 1 | once | - | 0 | guard | never | none | never |

Chance is multiplied by frequency (rare 0.5, normal 1, frequent 2); frequent also doubles the cap (one actor of a kind at a time otherwise). All numbers are
*design* values (Simon can overturn); the vanilla record's stats are used unscaled.

## 3. Colony effects

Applied after combat to an actor that survives in a colony's sector (minor-race worlds are never attacked; with `sparesAi` neither is any AI empire). Stations in the
sector lose hull each turn (a twentieth of the actor's attack) and fall at zero. A **siege** counts turns: assimilation needs 2, plus one per two defense batteries,
plus one for a starbase, plus three when it is the empire's last world. When it completes the colony is removed (trade routes, megaproject and station funding with it) and
`assimilated` records the system for 60 turns (shown on the map); the cube heals 20% and the kill counts toward its withdrawal. A recolonised system loses its mark.

## 4. Switches (new game, saved, and per preset)

`ThreatSetup {monsters, borg, frequency none|rare|normal|frequent, borgSphere, species8472, edoStation, sparesAi, disabledMask}`, all off by default.
* **Monsters** on or off: every kind except the Borg. **Borg threat** on or off: the Borg-only off switch (X14). **Frequency**: none stops all spawns.
* **Replacements** (X15): sphere for cube, Species 8472 for Husnock, DS9 for Edo Guardian.
* **Per-actor toggles** (X24): `disabledMask`, one bit per kind. **sparesAi** (UDM3: no random hostility for the AI): actors hunt and fight only the player.
* A seated Borg Collective empire **replaces the Borg threat** (same family, one at a time); the monsters are unaffected.
* Presets (`options.threats` in a preset file): Dominion War turns on monsters, Borg, sphere, 8472 and DS9; Galaxies and Ultimate turn on monsters and Borg at normal;
  Ages the same at rare; Classic, Remastered and Balance of Power leave threats off (this keeps the default game identical). `CampaignSetup.threats` set means
  the screen's choice wins over the preset; unset takes the preset's.

## 5. Borg rules (threat and playable empire)

* **Assimilation of systems**: above (threat). For a Borg empire, a captured colony is assimilated (`onColonyCaptured`): a quarter of the population is lost,
  batteries become factories, the queue is cleared, morale starts at 30 and climbs to the Collective's 60; it is marked on the map while the Borg hold it.
* **Assimilation of ships**: a cube heals a twelfth of its hull per wrecked ship it destroys; a Borg empire that wins a battle (expanded roster on) gets one Borg Probe
  drone, at half hull, for every two enemy armed ships it destroyed, at the battle sector.
* **Morale**: Borg colonies target a flat 60 and ignore minor-race morale (`moraleTarget`).
* **Structure and adaptation**: Borg ships mend (an idle cube 15% a turn, an idle Borg empire ship 10%); a cube adapts, +10% shields and attack after each battle it survives,
  to a cap of 40%.
* Deferred: the Galaxies Mod's "expensive and slow" Borg build costs (MOD-6/MOD-7 overlay values), a Borg tech tree, and cube-vs-colony ground assimilation of ships in orbit.

## 6. Save, bridge and client

Schema 20 adds the optional `THREATS` section (written only when a switch is on or an actor or mark exists): header line, `LAST_SPAWN` and one line per actor and mark.
Saves 14 to 18 load (a legacy18 flag, like 14 to 17 before it); `THREATS` under any older header is refused. A threat battle is an ordinary `CombatReport` with
`secondOwner = 9` (the validator accepts that side and bounds its shields by the kind's record). The bridge's `view` carries `threats` (actors in sensor range),
`assimilated` and `threatSetup`; `meta.threatKinds`, `meta.threatFrequencies` and each preset's `options.threats` feed the new-game screen; `newCampaign` takes `threats`.
Client: actors are markers on the galaxy map (existing insignia where a family has one, otherwise a hull glyph), captain's-log lines come from the turn report, and the
New Game screen has a Threats group (monsters, Borg, frequency, replacements, per-actor toggles). Models and **stand-ins**: Borg cube, sphere, Species 8472 and DS9
use their own roster models; Husnock (Krenim warship), Edo Guardian (Krenim timeship), Chodak (Klingon Negh'Var), Crystalline Entity (Xindi weapon), Gomtuu (Xindi aquatic),
Tarellian (Malon freighter), Calamarain (the Species 8472 bioship) and Echo Papa (Borg probe) are stand-ins; no new art was made.
