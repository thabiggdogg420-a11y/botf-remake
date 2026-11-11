# M2-EMPIRE: dilithium, emergency programs, invasion aftermath, minor conquest, signature ships

Lane M2-EMPIRE (Barclay, 11 Nov 2026). Outranked by `standing\simon\botf-rulings.md` (11 Nov: every vanilla feature, then the mods).
Sources: `research/PARITY-MATRIX.md` rows Dilithium, Emergency Programs, Invasion Aftermath, Independent Minor Conquest, Minor
Race Signature Ships; `research/VANILLA-DATA.md` section 2 (structure table); `data/base/buildings.json`, `data/base/ships.json`,
`data/base/minors.json`; `MINOR-RACES.md`. Code: `game/Source/BotfSim/EmpireRules.h` (header-only, like `MinorRaces.h`).

**Honesty note on numbers.** The extracted data files give costs and structure ids, not the 1999 executable's formulas. Where a
number below is marked *design* it is our own target, chosen so the rule is playable and testable; it is not read from vanilla.
Marked *vanilla* means it comes from the data files. Simon can overturn any *design* number.

## Switch, save and schema
- `GameplayRules::empireRules` turns all five rules on. `newCampaign` sets it for both presets. `generateCampaign*` and every
  save older than schema 15 keep it off and play exactly as before (one ship slot per empire, no programs, annex on conquest).
- **Schema 15** (`BOTF_SIM 15 remaster-15`). Schema 14 loads (header-only difference, no section). New optional section
  `EMPIRE_RULES 1`, written only when the rule is on: per-colony refinery/program rows, pending aftermath choices, minor guards.
  Two new commands (`SetEmergencyProgram`, `ResolveAftermath`) use `systemId` plus `eventOption`. Self-contained, so the schema number
  can be renumbered at merge by changing `SaveVersion`, `ContentVersion` and the legacy-header branch.

## (b) Dilithium as a construction constraint
- *Vanilla:* Dilithium Refinery id 6, 500 industry, 30 energy (VANILLA-DATA 2.3; `ReferenceCatalogue.h:230`). Refineries cap how many
  ships can be under construction, roughly one per refinery; dilithium systems boost Klingon yards (AFC-CAL, AFC-XAV, WIKI). Each
  home world starts with one (armadafleetcommand structures page). The exact formula is not in the data: *unverified*.
- *Today:* one ship slot per empire, hard-coded (`constructionBlocker`, "Empire shipbuilding slot used by system N").
- *Target (design):* new structure `DilithiumRefinery` (prototype scale like the shipyard: 120 industry, 5 energy, 2 credits upkeep,
  one per colony, needs a dilithium deposit). Deposit = deterministic hash of (seed, system) below 45 percent, plus any colony that
  already has a refinery. Each empire's most populous colony starts with one. **Ship slots = powered refineries of the empire**
  (zero means no ships). Colonies with a ship at the queue front claim slots in system-id order. Klingon colonies with a powered
  refinery in the same system build ships with +25 percent industry (*design*).
- *Correction:* none needed; the old single slot was a placeholder.
- *UI:* System screen: refinery in the build list with the deposit blocker; a dilithium line (slots used/total) on the system and
  empire screens; a blocked ship shows "Empire shipbuilding slots all in use (n of n)".
- *AI:* builds a refinery on any deposit colony when slots are below `min(3, colonies/2 + 1)`; its ship queues obey the same blocker.
- *Tests:* slot count follows refineries; second ship blocked then allowed after a refinery; Klingon boost; AI builds a refinery;
  old schema keeps one slot; save round trip.

## (a) Emergency programs
- *Vanilla:* five zero-cost sociology structures: Martial Law id 39, Police State 44, Tribunal 51, Inquisition 27, Festival of Fun 33
  (VANILLA-DATA 2.7), each race-locked by `excludedEmpireMask`. They "quell unrest". Magnitudes are not in the data.
- *Target (design):* a program is started on one colony by its owner while colony morale is below 70. Only the owner race's program.
  Sustained programs last 10 turns and can be cancelled; each costs `max(1, population/10)` credits a turn.

| Program (race) | On start | While active |
|---|---|---|
| Martial Law (Federation) | morale +10 | morale floor 50; research -20%; ground defense +25% |
| Police State (Klingon) | none | morale floor 50; industry +15%; population -2% a turn (not below 10) |
| Tribunal (Romulan) | none | morale floor 50; ground defense +40%; upkeep doubled |
| Inquisition (Cardassian) | none | morale floor 50; intelligence from the colony +50%; no population growth |
| Festival of Fun (Ferengi) | pays 2 credits per citizen, morale +20 | lasts 1 turn |

- *UI:* System screen "Emergency program" panel (button, status, turns left, cost, blocker). Empire screen lists active programs.
- *AI:* starts its program on a colony with morale under 40 when credits cover three turns of cost (Festival: six times the price),
  and cancels once morale is 65 or higher.
- *Tests:* each of the five effects; wrong race refused; morale gate; expiry; cancel; AI starts and ends one; save round trip.

## (c) Invasion aftermath
- *Vanilla:* after a conquest the player chooses Liberate, Subjugate, or Exterminate and recolonise (AFC-CAL). UDM3 adds a
  Protectorate HQ for subjugated worlds (mod, M3). *Today:* the colony is annexed at 80 percent population, morale 35.
- *Target:* a successful invasion queues an `AftermathChoice` for the player (an undecided choice stays on the default,
  Subjugate, which is today's behaviour, and lapses after 3 turns). The capture itself happens as before; the choice then reshapes it.
  - **Liberate** (only a minor race's homeworld): the colony is removed, the minor is independent again with its roster
    population; its attitude toward the liberator +30, toward every other contacted empire +5.
  - **Subjugate:** keep the colony as captured.
  - **Exterminate and recolonise** (not on a minor homeworld, see below): population becomes `max(2, pop/10)`, morale 50;
    structures stay; every contacted minor's attitude toward the conqueror -15 and every other major's trust in the conqueror -10.
- *Deferred:* exterminating a minor homeworld would need to erase a minor from the diplomacy engine. Not done; refused with a reason.
- *UI:* "Invasion aftermath" modal after the turn report names the world, the odds it was taken at, and each option's consequences.
- *AI:* decides at once, deterministically: Federation liberates minor homeworlds; Romulans and Cardassians exterminate a captured
  colony of population 12 or less; everyone else subjugates.
- *Tests:* each option's end state; default lapse; AI choice by race; save round trip with a pending choice.

## (d) Independent minors can be invaded
- *Vanilla:* independent minors could be invaded and conquered without a treaty (AFC-CAL, AFC-BSG). *Today:* immune (no colony exists).
- *Target:* `Invade` accepts an independent minor's system (no treaty with that minor). Transports in the sector pool their troops.
  Defense is `8 + population/3 + 5 + (5 + 2 x tech level)` (home guard) plus the guard ship (e). A win creates a colony for the invader,
  minor population x 80%, morale 35, minor becomes its member, then the aftermath choice. A loss costs the transports and the minor's
  attitude toward the attacker -20 (others -5). Attacking a treaty partner is refused.
- *UI:* Galaxy right-click menu enables Invade on independent minors for a selected transport.
- *AI:* a war-footing AI with a transport at an unguarded, weak independent minor (attack at least 70% of defense) invades it.
- *Tests:* win, loss, treaty refusal, pooled troops, AI invasion, determinism fixture covers a conquest.

## (e) Signature ships
- *Vanilla:* ten minors have their own ship (ships.json ids 105 to 114, minors.json `signatureShipId`; list in the data: Acamarian
  Raider, Bajoran fighter, Ktarian Raider, Pakled Transport, Sheliak Raider, Talarian Warship, Tamarian Defender, Trill Warship,
  Vulcan ship, Yridian Scout).
- *Target:* each such minor spawns a `MinorGuard` at its homeworld at game start (hull and shields from the catalogue). Guards are not
  fleets (fleets must be a major's playable class), they are map objects like stations. A guard fights the invader's armed ships
  in the sector when a transport there holds an invade order: each turn it loses the escorts' summed attack and deals its own
  attack (`(attack+9)/20`) to the first escort. While alive it blocks invasions of its system, and it adds its attack to the ground
  defense. Destroying one costs -10 attitude with its minor and -3 with all others. Damaged guards regain 10% a turn when unengaged.
  The guard stays with the minor after it joins an empire, and defends it.
- *UI:* the galaxy map marks guarded minor systems; the system panel lists the guard and its health.
- *AI:* escorts accompany an AI transport to a guarded minor before invading (AI picks the nearest armed ship at war posture).
- *Tests:* ten guards spawn on a full roster; guard blocks, is destroyed, then invasion proceeds; regen; save round trip.
