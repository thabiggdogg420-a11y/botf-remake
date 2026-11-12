# MOD-8: scenarios, secession, the Mirror Universe, flagships and the vendetta victory

Lane MOD-8 (Barclay, 12 Nov 2026). Outranked by `AI_stuff\standing\simon\botf-rulings.md` (11 Nov: every vanilla feature plus the most popular mod packs, each a new-game preset; 12 Nov: at most two lanes). Sources: `research/MOD-PARITY.md` rows E11 (Mirror Universe pack), E12 (Klingon civil war), E13 (flagships and hero ships), X21 (Dominion War scenario) and the honourable mentions in section 1.3; `research/PARITY-MATRIX.md` (the vanilla Vendetta victory, listed in the enum and never evaluated). Code: `game/Source/BotfSim/Scenarios.h` and `Scenarios.cpp` (new), hooks in `Simulation.cpp`, `Rules.cpp`, `Victory.cpp`, `Content.h`; data in `data/scenarios`, `data/overlays` (`flagships`, `mirror`, `klingon-civil-war`) and `data/presets` (`mirror-universe`, `klingon-civil-war`). Tests: `tests/scenarios/ScenarioTests.cpp`.

One idea: a **preset** says which rules and content a game plays on; a **scenario** says how that game starts and how it can be won. Scenarios are data, deterministic, and sit on a preset. A free game (no scenario) plays and saves exactly as before.

## 1. The scenario format

`data/scenarios/<id>.scenario.json`, embedded in the core like presets (`tools/content/build_presets.py`, `--check` for staleness). A host registers more with the bridge call `registerScenario {"bytes"}` (its preset and overlays must be registered first). Parsed strictly: unknown keys, bad ranges and unknown references are refused with the offending path.

```json
{"format":1,"id":"dominion-war","name":"The Dominion War","blurb":"…","order":10,
 "preset":"dominion-war","overlays":["flagships"],
 "map":{"width":28,"height":20,"systems":80,"seed":2373,"shape":"irregular","minorDensity":"many"},
 "lineup":["federation","klingon","romulan","cardassian","dominion"],"player":"federation","era":3,"year":2373,
 "briefing":["…"],
 "setup":[{"do":"war","a":"dominion","b":"federation"}, …],
 "triggers":[{"id":"romulan-pact","text":"…","when":{"turn":15},"do":[…]}],
 "victory":[{"id":"…","name":"…","kind":"eliminate","empires":["dominion"],"text":"…"}]}
```

| Field | Meaning |
|---|---|
| `preset`, `overlays` | The preset the game plays on (its overlays, rule flags, threat defaults) and extra overlays appended to its stack. |
| `map` | **Seeded**: `width`, `height`, `systems`, optional `seed` (fixed; absent takes the setup's), `shape`, `minorDensity`. **Fixed**: `fixed` lists the stars (`name`, `x`, `y`), `homes` names the capital star of lineup seat 2, 3, …; star 1 is seat 1's capital. Planets are still generated from the seed. |
| `lineup`, `player`, `era`, `year` | The empires in seating order (race keys), the default player, the start era (1..5) and the start year shown in the header (one year is four turns). The caller may choose another lineup empire as the player (`CampaignSetup::playerSet`) and sets the difficulty; the scenario fixes everything else. |
| `setup` | The scripted start: actions run once, in order, on the new campaign (turn 0). An unresolvable reference throws, so an authoring error never starts a game. |
| `triggers` | `when` (all of the listed conditions) then `do` (actions). Each fires once, at the end of the first turn it holds, announces `text` in the turn report and is recorded in the save. |
| `victory` | The scenario's own win conditions for the player, checked after the standard ones. `eliminate` (every listed empire has no colony), `hold` (the player holds every listed system), `survive` (the player still has a colony on turn N). |

**Conditions** (`when`): `turn` (this turn or later), `eliminated` (an empire), `warA` and `warB` (at war), `colonies` with `atLeast` or `atMost`, `capitalLost` (an empire no longer holds the system that was its capital).

**Actions** (`do`): `war`, `peace`, `alliance`, `contact` (two empires; `a`, `b`), `colony` (`a`, `at`, `population`), `fleet` (`a`, `at`, `ships`, `count`), `flagship` (`a`, `at`, `ship`), `placeMinor` (`name`, `at`: moves a minor race's home to a free system), `wormhole` (`at`, `to`: a paired mouth beside each), `placeThreat` (`name`, `at`: a threat actor key, moved or placed), `secede` (`a` the parent, `b` the house, `pct`), `credits` (`a`, `amount`).

**References.** An empire is `player` or a race key. A system is `capital:<race>` (the system named for that empire's capital), `minor:<minor race name>`, `system:<star name>` or `near:<n>:<reference>` (the n-th nearest star to the reference with no colony and no minor race on it, by Chebyshev distance, then straight-line, then id). Ships are roster or catalogue class names (`Defiant`, `Vor'cha`, `Jem'Hadar Battle Cruiser`) matched against what the empire can build, or `id:<ship id>`.

**At run time** an action that names an empire that is not seated, or a system that is gone, does nothing, so a secession that did not happen cannot stop the turn. During `setup` the same fault is an error.

## 2. The scenarios

| Scenario | Preset (overlays) | Map | Start | Wins |
|---|---|---|---|---|
| **The Dominion War** (X21) | `dominion-war` (`dominion`, `dominion-war`, `economy`) + `flagships` | seeded, large (28x20, 80 stars), irregular, many minors, seed 2373 | Dominion and Cardassians at war with Federation and Klingons (alliances inside each bloc); Bajor moved beside Cardassia with Deep Space Nine in its sector; a wormhole from Bajor to the Dominion's home; extra colonies and a fleet per side; flagships for Federation (a Sovereign, the Enterprise), Klingons and Dominion. Turn 15: the Romulans ally with the Federation and declare war on the Dominion. When the Cardassian Union is down to two worlds it turns on the Dominion and makes peace. | the Dominion eliminated; or survive to turn 100 |
| **The Klingon Civil War** (E12) | `klingon-civil-war` (`klingon-civil-war`) | seeded, medium (22x16, 50 stars), spiral, seed 2367 | The player is the Klingon Empire with six worlds and a flagship. Turn 4: the House of Duras secedes. Turn 8: the Romulans ally with the house. If Qo'noS falls the report says so. | the House of Duras eliminated; or survive to turn 90 |
| **The Mirror Universe** (E11) | `mirror-universe` (`mirror`) | **fixed**, 26 named stars on 16x12 | Terran Empire (the Federation slot) and the Klingon-Cardassian Alliance (the Klingon and Cardassian slots) at war, the Alliance allied; recoloured and renamed; the Enterprise a flagship. | both Alliance empires eliminated; or survive to turn 100 |

The `mirror-universe` and `klingon-civil-war` presets also work without a scenario (the New Game screen's Preset row): the mirror overlay and the house are there, nothing is scripted.

## 3. The Mirror Universe pack (E11)

A data overlay (`data/overlays/mirror.overlay.json`) over the base pack, nothing in the core: `races.patch` renames the Federation slot **Terran Empire** (capital Earth), the Klingon slot **Klingon-Cardassian Alliance** and the Cardassian slot **Cardassian Alliance** (new traits), a `flagships` block with mirror names (I.S.S. Enterprise, K.C.A. Rotarran, …), and a new `palette` block (below). Every ship, building and research table is the stock one ("from existing assets"). The preset turns on `raidBonus` and plays era 2 (2267) on a small map.

**Palette** (new optional pack object, schema 25, absent in the base pack): `{"empires":[{"empire":1,"color":"#9b1c1c","accent":"#e0b030"}]}`. The core exposes it in the game view (`palette`: empire id to colour and accent); the Godot client applies it to the owner colours of the galaxy map, the empire insignia plates and the LCARS faction tint. A pack without the block changes nothing.

## 4. Secession (E12)

`scenarios::secede(state, report, parent, house, pct)`, used by the `secede` action and callable by any host:

* **House.** An empire slot that is not seated. The Klingon Civil War overlay turns the eighth slot (the Breen's, free in that lineup) into the **House of Duras**: key `duras`, capital Ty'Gokor, ship donor the Klingons (so it builds and crews Klingon hulls), the Breen roster class made unbuildable, an orange palette entry. Nothing in the core knows the name.
* **Split.** The parent's colonies other than its capital are sorted farthest first; the house takes `pct` percent of them (at least one, never all). Its first world becomes its capital: renamed, a shipyard. Treasury: the house gets `pct` percent of the parent's credits. Technology: the house starts at the parent's levels with no progress. A parent with fewer than three worlds does not split (the call returns false and changes nothing).
* **Ships.** Every ship in a rebel sector, and every other armed ship besides (in id order), goes to the house when the house can crew its class; the parent's flagship stays loyal. Task forces are detached, standing orders cleared, stations in rebel sectors change hands, megaprojects, trade routes and station upgrades that touched a rebel world are cancelled.
* **Relations.** The house copies the parent's contact, trust and wars with everyone else, holds no treaty, and is at war with its parent. Minor races' contact and attitude toward the house copy the parent's. It joins the diplomacy and intelligence engines like any rival (`internal::seatEmpire`) and plays under the generic AI; its personality is the slot's (Breen: cold), a stand-in.
* **Record.** `GameState::secessions` (parent, house, turn) is saved, so a UI can say who split from whom. A save with a house in slot 8 uses the eight-slot layout (`EMPIRE_SLOTS`).
* **Player.** If the parent is the player's empire the player stays the loyalist side; the house is an AI.

## 5. Flagships (E13)

A pack's optional `flagships` block (`data/overlays/flagships.overlay.json` ships a stock one, the Mirror and Civil War overlays carry their own) lists, per empire, up to twelve names and the bonuses: `hullPct`, `attackPct`, `shieldsPct`, `morale`, `cost`, `cooldown`. A game whose pack has no block has no flagships.

* **One at a time.** An empire's flagship is any armed ship of its own. **Commissioning** (the `CommissionFlagship` command, `scenarios::commission`) costs `cost` credits, marks the ship (`Fleet::flagship`, the 1-based name index), gives it the next name from the list and scales its current hull and shields with the new maxima. Elite crew (experience 75) when the crew rule is on.
* **Bonuses.** `fleetMaxHull`, `fleetMaxShields` and `fleetAttack` add the percentages, so every consumer (tactical combat, upkeep, repair, the AI's power sums) sees a stronger ship; nothing else changes. While the flagship lives every colony of the empire has `morale` added to its morale target.
* **Loss.** The record keeps the serial and the loss turn; the report says the flagship was lost; a replacement may be commissioned after `cooldown` turns and takes the next name.
* **Scenario-placed.** The `flagship` action spawns a ship of a named class at a system and marks it, free.
* **AI.** A rival with at least twice the cost in credits commissions its strongest warship.
* **Save.** In the SCENARIO section (below), not in the fleet rows.
* **Not in the default games.** Classic and Remastered are plain packs; the stock `flagships` overlay adds flagships to any preset that stacks it (the Dominion War scenario does). Adding it to Ages, Galaxies or Ultimate is a one-word change in their preset files.

## 6. The vendetta victory

The vanilla victory that the enum listed and nothing evaluated: an empire wins by eliminating **both of its designated racial rivals**. `scenarios::vendettaRivals`: Federation Romulans and Cardassians (the wiki's own example), Klingons Federation and Romulans, Romulans Federation and Klingons, Cardassians, Ferengi, Dominion and Breen Federation and Klingons, Borg Federation and Romulans. Design call, Simon can overturn; the pairings are not in the data we hold. The vendetta is live only when both rivals are seated, from turn 20, and only where the alternative victories are on (Remastered), so Classic keeps conquest and domination as its text says. `victoryProgress` gains a fifth row (enabled, percent 0, 50 or 100, an explanation) and a win reads "Vendetta victory".

## 7. Save: schema 25

New optional section, written directly after `ECONOMY_RULES` and before `THREATS`, only when a scenario is running, a flagship was ever commissioned or a house has seceded:

```
SCENARIO 1 <scenario id | -> <n fired> <trigger index> …
FLAGSHIPS <n>        n rows: <empire> <fleet id | 0> <serial> <turn lost | 0>
SECESSIONS <n>       n rows: <parent> <house> <turn>
```

Saves 14 to 24 load (each is the schema-25 text without the section; the loader rebuilds the exact canonical text for the old header). A 23 or 24 header over a `SCENARIO` line is refused. `Fleet::flagship` is restored from the FLAGSHIPS rows. A scenario save needs the scenario to be registered and the overlay stack in its `PACKS` line (the scenario's preset plus its own overlays) to match, exactly like a preset. A default Classic or Remastered game differs from main (059ec08, schema 23) only in the header line; the fingerprint comparison is in the lane report. Schema 24 is the balance lane's; the loader already treats 24 as a legacy header, and the merge renumbers nothing else.

## 8. Client

* **New Campaign**: a Scenario row above the Preset row (None, then one toggle per scenario from `meta.scenarios`). Picking one fills the panel from the scenario (it fixes preset, lineup, galaxy, era), disables those controls, and shows the briefing, the start year and the victory conditions; Powers in play shows the scenario's lineup and the faction row lets the player choose any of them. `NewCampaignVM.pick_scenario`, `scenario_text`; web `NewCampaign.tsx` carries the same row.
* **Galaxy**: the galaxy preview is the generated scenario galaxy (the same `newCampaign` call), so Bajor, Deep Space Nine and the wormhole show on the chart before the game starts.
* **Fleets**: a flagship shows its name; the fleet panel has a Commission flagship button when the pack has them and the ship qualifies.
* **Victory overlay**: Vendetta joins the list; scenario objectives show under it with their progress.
* **Mirror Universe**: the palette from the core recolours owner colours and plates; names come from the core already.

## 9. Tests and proof

`BotfScenarioTests` (`tests/scenarios/ScenarioTests.cpp`): format and refusals; the three scenarios' scripted starts; triggers; the Dominion War victory; the Mirror pack; the secession (scenario, Federation player, preconditions, conservation); flagships (commission, bonuses, naming, morale, loss, cooldown, AI, absent pack); the vendetta; each scenario played 50 turns by the AI with the player's colonies on governors (determinism, a save round trip every ten turns, a reload at turn 25); save migration and forged sections. Native versus wasm determinism: the fixture check in `godot/tools/dump-fixtures.sh`. Classic and Remastered unchanged: the `BotfSim` output of this branch against main for the same seeds and turn counts (`FINAL_HASH` and the per-turn hashes), header excluded; see the lane report.

Proof as run (12 Nov 2026, after merging main 5134cc6, schema 25): all 23 ctest suites pass on the final binaries (`BotfScenarioTests` 19 of 19). Classic and Remastered: for seeds 1999, 7 and 42, 20 and 60 turns each, the saved text of this branch equals main's apart from the header line in all 12 runs, and the turn-by-turn output is identical apart from the new Vendetta row line. The turn-20 fingerprint pin in `godot/tests/run_tests.gd` moves with the header (the header is in the hashed text).
Two bounds follow the flagship bonuses: battle records accept hull and shields up to the largest flagship percentage (`scenarios::maxFlagshipPct`).

## 10. Stand-ins and deferred

* The house's AI personality, intelligence doctrine and affinities are the eighth slot's (Breen); a per-race personality field would fix it.
* The Mirror Universe's Romulans, Ferengi and the Terran-Alliance "rebels" (the Maquis analogue) are not in the preset; a Terran Empire emperor and agony booths have no mechanics, only the trait text.
* Flagships have no fleet-wide aura (a bonus to ships fighting beside them) and no unique hull models; they are the commissioned ship with bonuses and a name.
* The stock `flagships` overlay is not yet on Ages, Galaxies or Ultimate.
* Fixed-map scenarios place minor races at random stars (the roster placement runs over them); only the Mirror Universe uses a fixed map.
* Scenario text is English only, in the data.
* The Mirror Universe's Terran Empire keeps the Federation portrait and motto (existing assets only); the Cardassian Alliance and the Klingon-Cardassian Alliance both appear as separate powers, as the pack names them.
* The scenario picker sits in the middle column under the galaxy preview; it is off-screen until the column is scrolled at 1080p (the first capture shows only its top).
