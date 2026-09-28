# Remaster gameplay layer (save schema 13)

Lane D adds a modern gameplay layer on top of the deterministic core:

- ruleset presets
- outposts and starbases
- galaxy features and anomalies
- curated galactic events
- alternative victories with a progress query
- a structured turn chronicle
- build-queue templates

All outcome math is integer, and GCC and Clang produce identical campaign hashes. Every layer is validated, saved,
fingerprinted, used by the rival AI and covered by `RemasterGameplayTests` and the soak harness.

## Code layout

| File | Contents |
|---|---|
| `Simulation.h` | Public types, fields, commands and queries. They are marked "Schema-13 gameplay layer". |
| `GameplayInternal.h` | The internal interface between `Simulation.cpp` and the modules. It is not a client API. |
| `Rules.cpp` | Presets, `newCampaign`, gameplay command plumbing, the `GAMEPLAY_LAYER` save section, and the turn hooks. |
| `Stations.cpp` | Outposts, starbases, station combat, captures, and the station AI. |
| `GalaxyFeatures.cpp` | Anomalies, pathfinding, supply, sensors, hazards, surveys and research grants. |
| `GalacticEvents.cpp` | The event deck. |
| `BuildTemplates.cpp` | Build-queue templates. |
| `Victory.cpp` | Megaprojects, the diplomatic victory hook and `victoryProgress`. |
| `Chronicle.cpp` | Turn Briefing entries. |

`Simulation.cpp` only gained small hook calls. `TacticalCombat.h` gained `Ship::immobile` and an optional `FIXED`
encounter section.

Every new `.cpp` must be listed in three places:

- `BOTF_CORE_SOURCES` in `remaster/CMakeLists.txt`
- `game/Source/BOTFRemake/Private/SimulationBridge.cpp`, which is a unity include
- `$taskCoreSources` in `game/Build-Simulation.ps1`, which has a new `-Gameplay` test switch

The modules use named namespaces only, with no namespace-scope `using` directives, so the unity build stays clean. It
compiles warning-free as one translation unit under both GCC and Clang.

## 1. Ruleset presets

`GameState::rules` (`GameplayRules`) holds a `preset` plus one flag per layer.

| Preset | stations | galaxyFeatures | curatedEvents | alternativeVictories |
|---|---|---|---|---|
| `Remastered` (default for new games) | yes | yes | yes | yes |
| `Classic` (original-like) | yes | – | – (original random events) | – (conquest/domination only) |
| Loaded from schema 4–12 / pre-13 generators | – | – | – | – |

Outposts and starbases are an original BOTF feature, so `Classic` keeps them.

Saves from schema 12 and older load as `Classic` with every flag off, and they continue exactly as before. This was
verified by continuing schema-12 saves written by the previous build with both binaries: 150-turn reports were
identical. `generateCampaign()` and `generateReferenceCampaign()` still produce that baseline, which keeps existing
fixtures and tests stable. New games use `newCampaign(CampaignSetup)`, and its preset defaults to `Remastered`.

`validate()` enforces the following:

- `Remastered` requires every flag to be on.
- `Classic` forbids anomalies, curated events and alternative victories.
- Stations require `unifiedCombat`.
- A layer that is switched off must hold no data.

Anomalies are generated from a dedicated stream derived from the seed. A Classic game and a Remastered game from the
same seed therefore have the same stars, homes and starting fleets.

`applyRulesetPreset(state, preset)` is only valid at turn 0 with no pending commands.

The CLI takes `--ruleset classic|remastered`, which needs `--empires`. The default for new `--empires` campaigns is
`remastered`.

## 2. Outposts and starbases

### Building an outpost

A troop transport (`Building::Transport`) builds an outpost with `BuildOutpost(fleetId)`. The site is the sector the
transport stands in. It must satisfy all of these:

- The sector is explored and passable.
- No other station is there.
- It is not a minor race's system or a foreign colony's system. An own colony system is allowed.
- The empire has fewer than 12 stations plus builders, and the galaxy has fewer than 128.

Assembly takes 3 turns, or 2 turns at Construction 3 or higher. The transport becomes the outpost, which is consumed.
Any new order to the transport abandons the assembly: a move, attack, retreat, invasion, task force or terraform. The
check runs before movement. `outpostBlocker(state, fleetId)` returns the reason an order would be refused.

### Upgrading to a starbase

`UpgradeStation(stationId, fundingSystemId)` upgrades an outpost to a starbase. It requires all of these:

- An own colony within 6 sectors of the outpost, which funds the upgrade.
- Construction level 2 or higher.
- The credit cost, paid up front.

The funding colony then diverts all of its industry to the upgrade each turn, and its construction queue pauses. This
is recorded in `StationUpgrade` and reported per turn. The station becomes a starbase at full integrity when the work
completes.

The upgrade is dropped if the station or the colony changes hands. A colony funds at most one upgrade or megaproject at
a time. `stationUpgradeCost()` and `stationUpgradeBlocker()` expose the cost and the reason an upgrade would be refused.

### Station statistics

`stationStats(state, owner, type)` returns these values:

| | Prototype rules | Reference rules |
|---|---|---|
| Outpost hull / shields | 40+4·Construction / 10+3·Energy | Catalogue "Outpost" of the race (II once its tech is met), e.g. Federation 127/330 |
| Starbase hull / shields | 120+10·Construction / 40+6·Energy | Catalogue "Starbase", e.g. Federation 282/780 |
| Outpost / Starbase attack | 7+W / 18+2·W | 14+2·W / 40+4·W (W = Weapons level; 55% beam, 45% torpedo) |
| Upkeep per turn | 3 / 8 | Catalogue maintenance, 12 / 104 |
| Upgrade cost | 300 credits + 240 industry | 1500 credits + 400 industry |

These values apply in both rulesets:

- **Supply radius:** 3 for an outpost, 5 for a starbase. A sector within the radius counts as supplied for every ship
  of the owner, alongside powered shipyards.
- **Sensor radius:** 2 for an outpost, 3 for a starbase. Ships and colonies see 1 sector.
- **Repairs:** a starbase repairs friendly ships in its sector by 20% of maximum hull per turn and restores their
  shields. It only does so for ships that did not fight that turn, and only when no enemy at war is present.

Stations regenerate 10% hull and full shields on turns without battle. Upkeep is added to
`TurnReport::fleetUpkeep`, which now means military upkeep.

### Combat

A station fights for its owner whenever an empire at war with the owner has ships in its sector. This happens even if
the owner has no ships there. The encounter uses `resolveTacticalBattles`, so headless play and the session behave the
same.

The station joins the encounter as fleet ID `StationCombatantBase + station.id`, where `StationCombatantBase` is
2,000,000. `isStationCombatant()` and `stationIdForCombatant()` convert between the two. In the encounter the station
has these properties:

- `immobile = true`. It never moves, whatever order it is given.
- `canRetreat = false` and `canCloak = false`.
- A standing order of `Hold`, which braces it for −20% incoming damage while it fires normally.
- Its own "Station Group".
- A position mid-arena on its owner's edge.

The encounter serializes the `FIXED` section only when an immobile unit is present, so older encounters stay
byte-identical.

After the battle, fleet results apply as before, and the station's hull and shields are written back. A station
reduced to 0 hull is destroyed. Battle records validate station participants against station bounds.

A player station battle pauses the campaign session like any player battle. It replays exactly, and the soak checks
this in lockstep.

A station also blocks invasions of a colony in its sector, in the same way that armed defending ships do.

### Capture

On arrival, before combat, an enemy troop transport boards a station that meets both conditions:

- Its hull is at or below 50%.
- No armed ship of its owner is in the sector.

The transport is consumed, the station changes owner with its shields down, and any pending upgrade is cancelled. The
intended sequence is to damage the station in one turn and board it the next.

### Fog of war

`visibleStations(state, viewer)` returns the viewer's own stations plus any in sectors it currently has sensor
coverage of. The chronicle reports rival outposts only when they are detected.

### AI

Each rival AI does the following:

- **Builds outposts.** At peace it wants `min(4, 1 + colonies/2)` stations. Idle transports head to the best supplied
  site, which it scores by these factors:
  - newly supplied sectors
  - an unclaimed star system (+30)
  - a wormhole next to the site (+25)
  - a contacted foreign colony within 5 sectors (+15)
  - distance (a penalty)
- **Orders transports.** It queues one outpost transport at the front of the queue in the colony that holds the
  empire's one-ship-at-a-time shipyard slot. If a newer colony takes the slot, it moves the starved project there.
- **Upgrades outposts.** When its credits are at least 3× the upgrade cost, it upgrades one outpost at a time. It
  prefers the outpost closest to a foreign colony and funds it from its highest-industry colony in range.
- **Raids stations at war.** It attacks visible hostile stations with up to 5 nearby warships when their estimated
  firepower is enough: `(attack+W)·6 ≥ hull+shields`. It sends a transport to board a weakened, undefended station.
- **Pins builders.** Its assembling transports keep their post whatever the general fleet plan says.

## 3. Galaxy features and anomalies

These are generated for Remastered games only (`GalaxyFeature`, `GameState::galaxyFeatures`). The table shows the
counts for a 16×12 map.

| Feature | Count | Placement | Effect |
|---|---|---|---|
| Nebula | 2 clouds of 3–5 cells (1 cloud below 150 sectors) | Off systems and away from homes | **Sensor-blind.** A nebula sector is only visible from inside it, and ships inside see only their own sector, so fleets hide. **Shields fail** (the pick: Mutara-style): ships and stations in a nebula have shields drained to 0 before combat, so nebula battles are hull-only. |
| Black hole | 1 (2 from 300 sectors) | Interior, with no system or other anomaly adjacent, so the 8-connected map stays connected | **Impassable.** |
| Neutron star | 1 (2 from 250 sectors) | Off systems | **Damaging.** 10% of maximum hull per turn and half the shields. It never goes below 1 hull. |
| Wormhole pair | 1 pair (2 from 300) | Endpoints at least `max(5, max(w,h)/2)` apart | **Instant link.** Once an empire has explored an entry endpoint, routing may step from it straight to the other endpoint. The exit and its neighbouring sectors count as supplied when the entry is supplied by colonies or stations. |
| Ion storm | 1 (2 from 300) | Plus-shaped front of 5 cells | **Drifts.** It moves 1 sector along its direction every 2 turns and bounces off edges and impassable cells. Ships inside lose their shields and 5% hull per turn, never going below 1. |
| Subspace rift | Spawned by an event | – | Impassable for 10 turns. |

### Pathfinding

`route()` and the task-force pathfinder use `findPath` whenever features exist. It works as follows:

- Impassable cells are skipped.
- Known wormhole links are extra edges.
- A first pass avoids hazards, meaning neutron stars and storm cells, unless the destination itself is one.
- If no hazard-free path exists, a second pass allows them.
- Supply lookups are memoised per call.

Campaigns without features use the original breadth-first search unchanged.

### Surveys

A Scout-class ship that ends its turn within 1 sector of an anomaly surveys it once for its empire. For a nebula the
whole cloud counts as one survey. For a wormhole the survey covers both ends and also charts the exit.

The reward is research equal to a percentage of the cost of the next level in a field, with a minimum floor. It goes
through `grantResearch`, which keeps the research ledger reconciled.

| Anomaly | Research reward | Other |
|---|---|---|
| Nebula | Energy 25% (≥40) | – |
| Wormhole | Propulsion 20% (≥40) | +60 intelligence |
| Black hole | Propulsion 30% (≥50) | – |
| Neutron star | Construction 25% (≥40) | – |
| Ion storm | Energy 20% (≥30) | – |
| Subspace rift | Computers 35% (≥60) | – |

Rival AI scouts head for unsurveyed anomalies they know about.

`knownFeatures(state, viewer)` returns the features on sectors the viewer has explored.

## 4. Curated galactic events

In Remastered games the event deck replaces the original ±morale/+25-credit random events. Classic games keep the
original events.

The deck holds 14 cards, two of each kind, shuffled from the seed and reshuffled with the campaign RNG when exhausted.
Drawing works as follows:

- The first draw is on turn 6, and there is one draw every 7 turns after that.
- Each draw targets one campaign empire chosen at random.
- If the card does not apply to that empire, the next card is tried.
- At most 16 events are active at once.
- `eventsEnabled = false` stops new draws, but active events still resolve.

An event with a decision offers 2–3 options, listed by `eventChoices(kind)` with a label, an effect text and the
default. The empire has 2 turns to decide:

- The player decides with `ResolveEventChoice(eventId, option)`.
- If no decision arrives by `choiceDeadline`, the default applies on the next turn.
- Rival AIs decide when the event starts, deterministically by situation.

A decision takes effect at the start of the next turn's event phase. `describeEvent()` gives the cause-and-effect text.

| Event | Target | Options (default in bold) | AI choice |
|---|---|---|---|
| Plague | A colony with 15+ population. It loses `clamp(8 − Biotech, 2, 8)`% per turn for 4 turns. | Quarantine: −10 morale, losses halved, ends next turn · Emergency cure: 150 credits (60 in prototype), ends now · **Let it run its course** | Cure if credits ≥ 2× the cost, otherwise quarantine |
| Supernova warning | A non-capital colony of an empire with 2 or more colonies. The star detonates in 6 turns: −75% population, −25 morale, and ships and stations in the sector take half their maximum hull as damage (never lethal). | Evacuate: half the colonists move to the nearest colonies with room · Stellar shielding: 400 credits (150), losses 30% · **Trust the scientists** | Shielding if credits ≥ 2× the cost, otherwise evacuate |
| Subspace rift | Galaxy-wide, in a free interior sector | – (impassable for 10 turns; survey reward) | – |
| Trade boom | An empire | – (+10 credits per colony per turn, capped at 150, for 5 turns) | – |
| Derelict vessel | An empire, at one of its fleets outside its colonies | Salvage technology: 40% of the next level in the weakest field · **Strip it for credits**: +250 (100) · Study sensor logs: +60 intelligence and charts the 5×5 area | Salvage while any field is below the scientific threshold, otherwise credits |
| Labor unrest | The colony with the lowest morale, if it is ≤60 | Meet demands: 20+population credits, +15 morale · Martial law: −10 morale, ends now · **Ignore**: −5 morale and 20 construction work lost per turn for 4 turns | Meet demands if affordable twice over, otherwise martial law |
| Scientific breakthrough | An empire | **Theoretical**: 50% of the next level in the weakest field · Applied: Construction · Weapons | Weapons at war, otherwise theoretical |

The player's briefing only ever shows the player's own events, plus rifts and detonations in sectors it has explored.

## 5. Victory conditions and progress

Conquest and domination are unchanged. Conquest means only you hold colonies. Domination means holding at least 60%
of systems after turn 20. Both still apply to the player only, as before.

The Remastered layer adds two more conditions.

**Scientific victory.** An empire must meet all of these:

- Every one of the six fields is at the threshold: `min(10, eraBaseTechLevel + 3)`. That is level 4 for Early reference
  games, 3 for Early prototype games, 6 for Developed, 8 for Expanded, and 10 for Advanced and Supreme.
- It is turn 50 or later.
- It can pay the credits: 500·era in reference rules, 150·era in prototype rules.

It then starts the empire megaproject at one of its colonies with `StartMegaproject(systemId)`. The project absorbs up
to `30 + 5·Construction` industry per turn (`megaprojectWorkPerTurn`) from that colony, and the colony builds with the
rest. The project costs `4000 + 1000·(era−1)` industry. Completing it wins.

The project is lost if the colony falls. `CancelMegaproject` abandons it with no refund.

The megaproject is named after the starting era:

| Era | Megaproject |
|---|---|
| Early | Genesis Initiative |
| Developed | Subspace Relay Array |
| Expanded | Transwarp Network |
| Advanced | Iconian Gateway Restoration |
| Supreme | Omega Stabilization Project |

When a rival completes its megaproject the player is defeated: `outcome = Defeat` and `winnerEmpire` is the rival.
Rival AIs start the megaproject when they are eligible and hold 2× the credit cost. The start is announced to the
player as galactic news.

In Supreme-era games every field starts at level 10, so the megaproject race decides those games at around turn 150.
In the soak, 10 of 55 Remastered campaigns ended this way, all of them Supreme or fast prototype games.

**Diplomatic victory.** `bool diplomaticVictoryAchieved(const GameState&, int empireId)` in `Victory.cpp` is a clearly
marked **placeholder** that returns `false`. Lane B replaces its body with the predicate "this empire's alliances and
memberships control a majority of galactic population". `evaluateVictory` already awards the victory when the
predicate returns true.

**`victoryProgress(state, empireId)`** returns four `VictoryProgress` entries, one per condition: Conquest, Domination,
Scientific and Diplomatic. Each has `enabled`, `achieved`, `percent` (0–100) and an `explanation` string.

| Condition | How `percent` is computed |
|---|---|
| Conquest | Share of all colonies held |
| Domination | Colonies held against the 60% requirement |
| Scientific | 70% from tech progress toward the threshold, 30% from megaproject work |
| Diplomatic | 0 until Lane B's predicate lands |

## 6. Turn chronicle ("Turn Briefing")

`TurnReport::chronicle` has exactly one `ChronicleEntry` per line of `TurnReport::events`, in the same order.
`eventIndex` gives the index of the matching line. Each entry carries these fields:

- `category`: Construction, Research, Exploration, Colonization, Combat, Diplomacy, Intelligence, Events, Stations or
  Victory
- `text`
- `x`, `y`
- `systemId`, `fleetId`, `stationId`, `eventId`
- `empireId`, `otherEmpireId`

The gameplay layer records its entries with full references. Lines written by the older systems are classified by
pattern, and system names, sector coordinates, fleet IDs and empire names are extracted where they appear. Existing
`events` consumers keep working.

Only player-visible information reaches `events`, and therefore the chronicle. `describeChronicle(report)` renders a
text briefing, which the CLI prints when run with `--chronicle`.

## 7. Build-queue templates

A `BuildTemplate` has these parts:

- `id`, `ownerId`, `name` (1–48 printable characters)
- `items`: 1–32 `Construction` entries with product and `referenceId`
- `autoApply`

| Command | Effect |
|---|---|
| `DefineBuildTemplate(templateId, name, items)` | Creates a template when `templateId` is 0, otherwise replaces the owned template. Ships in reference campaigns need the empire's own named class. Defense batteries need advanced rules. |
| `DeleteBuildTemplate(templateId)` | Deletes the template. |
| `ApplyBuildTemplate(templateId, systemId)` | Appends the items to the colony queue in order. An item that is illegal at that moment is skipped, never reordered. Reasons include a full queue, a tech blocker, a shipyard already built or queued, batteries at capacity, and no shipyard for a ship. `Colony::queueExplanation` says what happened. |
| `SetDefaultBuildTemplate(templateId or 0)` | Sets the default template. Every colony the empire newly acquires with an empty queue gets it applied at the end of the turn. This covers founded, joined and captured colonies. It is reported under Construction. |

Limits are 16 templates per empire and 80 in the galaxy.

## 8. Save schema 13

The header is `BOTF_SIM 13 remaster-13`. Everything new is in one section, `GAMEPLAY_LAYER ... END_GAMEPLAY_LAYER`.
It follows `COLONY_GOVERNORS` and is required in schema 13. The layout is shown below; comments are not part of the
format.

```
GAMEPLAY_LAYER <preset> <stations> <features> <events> <victories>
STATIONS <nextStationId> <n>            id owner x y type hull shields builtTurn
OUTPOST_BUILDS <n>                      fleetId progress
STATION_UPGRADES <n>                    stationId fundingSystemId work
FEATURES <nextFeatureId> <n>            id type x y pairId clusterId driftX driftY expiresTurn surveyedMask
GALACTIC_EVENTS <nextEventId> <lastTurn> <deckPos> <deckSize> cards... <n>
                                        id kind empire system x y start end deadline choice applied magnitude featureId
TEMPLATES <nextTemplateId> <n>          id owner autoApply "name" count (product referenceId)...
MEGAPROJECTS <n>                        owner systemId work startTurn
END_GAMEPLAY_LAYER
```

A pending schema-13 command appends `stationId eventId eventOption templateId "templateName" count (product ref)...`
to its line. Other command types are unchanged, and `validate` requires their gameplay fields to be empty.

Migration works as follows:

- A **schema 12** save is the schema-13 payload minus this section. It loads as Classic with every layer off.
  Canonical form is still enforced, and a schema-12 header with a schema-13 section is rejected.
- **Schemas 11 and earlier** keep their existing migrations. The canonical check strips the section as well.

The fingerprint covers the whole payload. Test helpers in `SimulationTests.cpp` (`asSchemaTen`, `asSchemaEleven`) strip
the section when they forge older saves.

## 9. Turn order (hooks in `advanceWithCombat`)

1. `applyDraft` applies gameplay commands.
2. `planRivals` runs, then `gameplay::planTurn`: station AI, raids, cancelling interrupted outposts, anomaly surveys
   by AI scouts, and AI megaprojects.
3. Fleet upkeep, then station upkeep.
4. Production. Colonies funding an upgrade or megaproject divert industry to it (`divertIndustry`).
5. Movement and contact, then shield recharge, then `beforeCombat`: outpost assembly, anomaly surveys, sector hazards
   and station boarding.
6. `resolveTacticalBattles`, which is station-aware, and repairs. Then `afterCombat`: station regeneration and starbase
   repairs.
7. Invasions (stations defend), then `carryOver`: stations, survey marks and survey rewards; upgrades and megaprojects
   are dropped for lost colonies.
8. Diplomacy, intelligence and population.
9. Events: the curated deck in Remastered games, or the original random events.
10. Conquest and domination, then alternative victories.
11. `finishTurn`: starbase commissioning, rift expiry, ion storm drift, default templates for new colonies, and the
    chronicle.

## 10. Public API added in `Simulation.h`

### Types

| Type | Members |
|---|---|
| `RulesetPreset` | Classic, Remastered |
| `GameplayRules` | `preset`, `stations`, `galaxyFeatures`, `curatedEvents`, `alternativeVictories` |
| `StationType` | Outpost, Starbase |
| `Station` | `id`, `ownerId`, `x`, `y`, `type`, `hull`, `shields`, `builtTurn` |
| `StationUpgrade` | `stationId`, `fundingSystemId`, `work` |
| `StationStats` | `maxHull`, `maxShields`, `beam`, `torpedo`, `supplyRadius`, `sensorRadius`, `upkeep`, `repairPercent` |
| `StationUpgradeCost` | `credits`, `industry` |
| `GalaxyFeatureType` | Nebula, Wormhole, BlackHole, NeutronStar, IonStorm, SubspaceRift |
| `GalaxyFeature` | `id`, `type`, `x`, `y`, `pairId`, `clusterId`, `driftX`, `driftY`, `expiresTurn`, `surveyedMask` (bit `1<<empireId`) |
| `GalacticEventKind` | Plague, SupernovaWarning, SubspaceRift, TradeBoom, DerelictVessel, LaborUnrest, ScientificBreakthrough |
| `GalacticEvent` | `id`, `kind`, `empireId`, `systemId`, `x`, `y`, `startTurn`, `endTurn`, `choiceDeadline`, `choice`, `choiceApplied`, `magnitude`, `featureId` |
| `EventChoiceInfo` | `index`, `label`, `effect`, `isDefault` |
| `BuildTemplate` | `id`, `ownerId`, `name`, `items`, `autoApply` |
| `Megaproject` | `ownerId`, `systemId`, `work`, `startTurn` |
| `VictoryCondition` | Conquest, Domination, Scientific, Diplomatic |
| `VictoryProgress` | `empireId`, `condition`, `enabled`, `achieved`, `percent`, `explanation` |
| `ChronicleCategory` | Construction … Victory (see section 6) |
| `ChronicleEntry` | See section 6 |
| `CampaignSetup` | `seed`, `empires`, `width`, `height`, `systems`, `playerEmpire`, `era`, `reference`, `preset` |

The constants are `StationCombatantBase` (2,000,000) and `IonStormDriftTurns` (2).

### Fields

| Struct | New fields |
|---|---|
| `GameState` | `rules`, `stations`, `nextStationId`, `stationUpgrades`, `galaxyFeatures`, `nextFeatureId`, `galacticEvents`, `nextEventId`, `lastCuratedEventTurn`, `eventDeck`, `eventDeckPosition`, `buildTemplates`, `nextBuildTemplateId`, `megaprojects` |
| `Fleet` | `buildingOutpost`, `outpostProgress` |
| `Command` | `stationId`, `eventId`, `eventOption`, `templateId`, `templateName`, `templateItems` |
| `TurnReport` | `chronicle` |
| `tactical::Ship` | `immobile` |

### Commands

The `CommandType` values below were appended, each with a `make…Command` factory:

- `BuildOutpost`: `makeBuildOutpostCommand(id, fleetId)`
- `UpgradeStation`: `makeUpgradeStationCommand(id, stationId, fundingSystemId)`
- `ResolveEventChoice`: `makeResolveEventChoiceCommand(id, eventId, option)`
- `DefineBuildTemplate`: `makeDefineBuildTemplateCommand(id, templateId, name, items)`
- `DeleteBuildTemplate`: `makeDeleteBuildTemplateCommand(id, templateId)`
- `ApplyBuildTemplate`: `makeApplyBuildTemplateCommand(id, templateId, systemId)`
- `SetDefaultBuildTemplate`: `makeSetDefaultBuildTemplateCommand(id, templateId)`
- `StartMegaproject`: `makeStartMegaprojectCommand(id, systemId)`
- `CancelMegaproject`: `makeCancelMegaprojectCommand(id)`

### Queries

| Area | Functions |
|---|---|
| Rules | `newCampaign`, `rulesForPreset`, `rulesetPresetName`, `parseRulesetPreset`, `applyRulesetPreset` |
| Stations | `stationById`, `stationAt`, `stationStats` (two overloads), `stationTypeName`, `stationName`, `visibleStations`, `outpostBuildTurns`, `outpostBlocker`, `stationUpgradeCost`, `stationUpgradeBlocker`, `stationUpgradeFor`, `isStationCombatant`, `stationIdForCombatant` |
| Features | `galaxyFeatureName`, `featuresAt`, `knownFeatures`, `ionStormCells`, `sectorPassable`, `sectorHazardous`, `sectorInNebula` |
| Events | `galacticEventName`, `eventChoices`, `eventDefaultChoice`, `describeEvent`, `empireEvents` |
| Templates | `empireBuildTemplates`, `buildTemplateById` |
| Victory | `victoryProgress`, `victoryConditionName`, `scientificVictoryThreshold`, `megaprojectName`, `megaprojectIndustryCost`, `megaprojectWorkPerTurn`, `megaprojectCreditCost`, `empireMegaproject`, `megaprojectBlocker`, `diplomaticVictoryAchieved` |
| Chronicle | `chronicleCategoryName`, `describeChronicle` |

The behaviour of existing functions changed only for campaigns with stations or features:

- `visible()` accounts for station sensors and nebulae.
- `supplied()` accounts for station and wormhole supply.
- `route()` accounts for impassable anomalies, hazard avoidance and wormhole links.

## 11. Verification

- **Tests.** `BotfRemasterGameplayTests` has 34 scenarios covering presets, stations, anomalies, events, victory,
  templates, persistence and the chronicle. `SimulationTests` helpers were updated for schema 13.
- **Soak.** 55 Remastered campaigns (5 eras × 5 players × 2 seeds, reference and prototype, 220 turns) plus 5 Classic
  campaigns, run with a scripted player that exercises every new command. Sessions run in lockstep on part of the set.
  Saves round-trip every 10 turns, and every 30 turns a reloaded campaign must continue identically.
- **Compilers and sanitizers.** GCC and Clang builds agree on `FINAL_HASH` for 55 campaigns: 5 seeds × 5 players ×
  2 rulesets, plus 5 prototype eras. The unity build compiles with `-Werror` under both, and ASan/UBSan runs are
  clean.

## 12. Open issues

- **Diplomatic victory** waits for Lane B's predicate. See section 5.
- **No AI-vs-AI wars.** The current AI declares war only on the player, so rival stations are only attacked or captured
  when the player fights. The raid logic already covers AI-vs-AI wars once Lane B's diplomacy AI starts them.
- **Provisional balance.** This covers station stats, costs, event magnitudes, the megaproject cost and pace, and the
  threshold. Credits accumulate quickly in current AI economies, so rivals upgrade most outposts to starbases.
- **Starbase gating.** In reference rules the catalogue gates starbases at tech 4–6. The remaster gates the upgrade at
  Construction 2 instead, to match its slower research pace, and uses the catalogue values for hulls, shields and
  upkeep.
- **Station combatants in battle records.** A station still appears as a Destroyer-class participant in
  `CombatReport`, because it must satisfy the `isShip` bound. Clients should detect stations with
  `isStationCombatant()`.
- **Balance knobs.** Supernovae do not leave a remnant anomaly, and ion storms do not slow movement.
