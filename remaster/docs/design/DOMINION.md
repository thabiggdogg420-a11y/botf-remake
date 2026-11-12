# MOD-3: Dominion content and new minors

Lane MOD-3 (Barclay, 12 Nov 2026). Outranked by `AI_stuff\standing\simon\botf-rulings.md` (11 Nov: every vanilla feature plus the popular packs, Ultimate Dominion Mod by name, each a new-game preset; 12 Nov: two lanes at a time). Sources: `research/MOD-PARITY.md` rows E01, E03, N01 to N03, X03; `EMPIRE-SLOTS.md` (the Dominion is slot 6); `PACKS-AND-PRESETS.md`. Private; not for distribution. Code: `game/Source/BotfSim/Structures.h` (new), `Content.h`, `MinorRaces.h`, `Simulation.cpp`, `Intelligence.h`; data: `data/overlays/dominion.overlay.json`, `dominion-war.overlay.json`, `data/presets/dominion-war.preset.json`; tests: `tests/scenarios/DominionTests.cpp`.

## 1. What was missing

`buildings.json` holds 318 vanilla structures, but the core never reads it: a colony builds the fixed `Building` enum (farm to refinery). So "structure ladder as pack data" needed one small piece of core: a generic **race structure**. A pack record with `buildable: true` becomes something a colony can queue; nothing else in the base pack does, so a game on the base pack (Classic, Remastered, any stock preset) is unchanged.

## 2. Race structures (core, `Structures.h`)

* `Building::Structure` is the queue product; `Construction::referenceId = RefBase (100000) + buildings.json record id`. `Colony::structures` holds the built record ids, ascending.
* Record fields (all optional, absent in the base pack): `buildable`, `startsBuilt` (in every eligible capital at turn 0), `requires` (record id that must stand **in the same colony**: the ladder), `perEmpireLimit`. `excludedEmpireMask` is now 64 bits wide (bit N = race N, the `RaceCatalogue` id, so the Dominion is bit 35; the five-bit vanilla values read the same).
* Effects by `bonusType`, the vanilla meanings: 14 industry %, 30 research %, 42 food %, 5 credit income %, 20 morale, 37 ground defence %, 11 internal-security output %. New: **90** Shapeshifter Network (sabotage output % = values[0], sabotage damage bonus = values[1]) and **91** Ketracel-White (the roster classes named in `bonusValues` need one such structure in the empire; enforced in `constructionTechBlocker`, so the player list and the AI both obey it).
* A structure draws no colony energy (design choice; the data keeps `energyCost` 0) and costs credit upkeep of `industryCost / 400`, minimum 1. A captured colony keeps its structures and they work for the new owner (the queue is cleared on invasion, as before).
* AI: after the shipyard and refinery rules a colony with a yard and credits >= 150 queues the lowest-id rung it may start whose cost is at most 100 x its industry (`aiNext`).
* Save: schema 21, optional `STRUCTURES 1 / <n> / <systemId> <count> <ids>` rows after `EMPIRE_RULES`, written only while a colony holds one.

## 3. The Dominion overlay (`dominion`, data)

Ten appended records, ids 318 to 327, usable by the Dominion only. Costs and numbers are first-pass design values; Simon can overturn them.

| Id | Structure | Requires | Does |
|---|---|---|---|
| 318 | Vorta Administrative Center (starts built) | none | +6 morale here |
| 319 | Ketracel-White Facility (starts built) | 318 | gate for the three Jem'Hadar warships |
| 320 | Jem'Hadar Barracks | 318 | +50% ground defence here |
| 321 | Vorta Commerce Bureau | 318 | +25% credit income here |
| 322 | Dominion Foundry | 318 | +30% industry here |
| 323 | Cloning Vats | 319 | +40% food here |
| 324 | Genetic Research Station | 319 | +40% research here |
| 325 | Vorta Intelligence Directorate | 318 | +40% internal-security output (empire) |
| 326 | Founders' Sanctuary (2 per empire) | 324 | +10 morale here; unlocks the network |
| 327 | Changeling Shapeshifter Network (2 per empire) | 326 | +30% sabotage output, +1 sabotage damage (empire) |

**Founders and Vorta as traits.** Race records gain an optional `major.traitEffects` (morale, creditsPct, groundDefensePct, securityPct; zero when absent). The overlay sets the Dominion to morale +5 (Founders' will), credits +10% (Vorta administrators) and ground defence +25% (Jem'Hadar shock troops), folded into `minors::memberBonuses`, so every consumer (economy, morale target, invasion) sees them. `memberBonuses` also stopped ignoring empires above slot 5, which means a Dominion, Borg or Breen player now gets member-minor bonuses (a MOD-2 gap).

**Shapeshifter Network feeds intel.** `TurnContext` gains `securityBonusPercent` and `sabotageBonusPercent`, applied to those two areas only; the sum over an empire's structures goes in each turn. The Network's damage bonus adds to the production-sabotage hit (25 industry per severity point). Intel output, budget, focus departments and the 20-turn history are untouched.

**Jem'Hadar line.** Existing roster ships only: Attack Ship 1500 (tech 3/4/4/4/4/5, cost 3600), Battle Cruiser 1501 (5/6), Battleship 1502 (7/8), all behind the Ketracel-White gate. Gaps (no model, so none added): a Jem'Hadar fighter, a Vorta cruiser, a Dominion scout, troop and colony ship (the Cardassian donor hulls stand in, as MOD-2 set up), a Founder or Karemma trade ship.

## 4. Minors (`dominion-war`, data plus small placement rules)

Optional minor fields: `available` (false = never placed), `featured` (placed before the seeded draw), `majorRaceId` (skipped while that race is seated as a major); `affinity` may now carry nine values (slots 6 to 8: Dominion, Borg, Breen); `signatureShipId` may name a roster class, so a minor's own roster ship guards its homeworld.

* **Bajor replaces Edo**: the Edo are `available: false`; the Bajorans are `featured` (always on the map), tech 5, population 80, morale bonus 8, wary of the Dominion.
* New minors, ids 31 to 38: Breen (a minor only while the Breen are not a major; Warship guards), Ferengi (a minor only while the Ferengi are not a major, which is the Dominion War lineup), Son'a (roster Battleship), Maquis (Raider), Miradorn, Karemma, Kazon (Raider), Orion Syndicate (Interceptor). Six new race records (38 to 43) carry them; the portrait icon names are stand-ins, the client does not read them.
* Where a roster ship has a minor, the roster record's `minorRaceId` points at it (Son'a, Maquis, Kazon, Orion).
* **Preset.** Dominion War stacks `dominion` then `dominion-war`; its blurb names the content; `needs` is now only MOD-8 (the scripted scenario). The Galaxies preset also seats the Dominion; adding `dominion` to its overlay list is a one-line change left to the merge (this lane does not touch that preset).

## 5. Not done, and risks

* No scrap order for race structures (the Installed tab lists them read-only); no energy use; no AI tuning beyond the rule above (a Dominion AI built about two rungs in 50 turns).
* The Founders themselves (a Founder leader or hero) and the Dominion-specific diplomacy (Vorta ambassadors, the Alliance with Cardassia and Breen) belong to MOD-7 and MOD-8.
* Merge: schema 21 here; MOD-5 uses 20, so the renumber touches `SaveVersion`, `ContentVersion` and the `legacy`/`current` flags only. The overlay-patch whitelist (`optionalField`) names the new optional keys so a patch can add them to a base record.
