# Campaign eras and research levels 11 to 14 (lane MOD-4, schema 22)

Sources that outrank this file: `C:\Users\BiggDogg\AI_stuff\standing\simon\botf-rulings.md` (11 Nov 2026: every vanilla feature plus the most popular mod packs as new-game presets, "Yes, presets"; 12 Nov: two lanes at a time), then `docs/research/MOD-PARITY.md` rows E06, E07, E08 and the GALM and AAM entries in section 2, and `docs/design/PACKS-AND-PRESETS.md` (overlays and presets). Private; not for distribution.

## 1. What an era is now

Before this lane `StartingEra` (Early to Supreme) was only a five-step tech level plus start credits, colony size and home fleet. It still is, and **Classic and Remastered keep exactly that** (cap 10, the same costs, no calendar). Under the new **eras rule** the same five starts become a timeline:

| Start | Year | Ages start level | Galaxies start level | The period |
|---|---|---|---|---|
| Early | 2240 | 1 | 1 | NX line retiring, Constitution from 2245 |
| Developed | 2280 | 3 | 4 | Films: refits, Excelsior from 2285 |
| Expanded | 2320 | 5 | 7 | Constitution retired, next generation waits for 2340 |
| Advanced | 2350 | 8 | 10 | Challenger and Cheyenne lines; Dominion War ships from 2368 |
| Supreme | 2380 | 11 | 14 | Nemesis-era fleets, Prometheus, Mogai, Negh'Tev |

The year is `start year + turn / 4` (`turnsPerYear` is pack data). Galaxies' 1 / 4 / 7 / 10 / 14 is the Galaxies Mod's stated stage table (MOD-PARITY.md section 2, P2); Ages uses a gentler 1 / 3 / 5 / 8 / 11 so a 2380 start still has research left. Both are data and Simon can change them.

**Era gate.** A roster class (`data/base/roster.json`, ids 1000 to 1999) is buildable only while the timeline year is inside its years in service. Its first year comes from its existing `era` tag (`tagYears`, in `EraNames` order: Enterprise 2150, Original 2245, Movie 2270, NextGeneration 2340, DominionWar 2368, Nemesis 2379, Kelvin 2380, Future 2400) unless the pack lists the class in `classes` with its own `from` and an optional retirement `until` (0 = none). The gate sits in `constructionTechBlocker`, so the build list shows the reason ("Enters service in 2285", "Retired in 2255"), a queue command is refused, and the AI's pick (`queueMilitaryShip`) skips the class. Retirement only stops new builds: fleets and queue entries that exist stay valid. The start fleets prefer the era's own canon classes. **The legacy 125-ship catalogue is on the timeline too.** The same `classes` list of the pack takes a catalogue ship id (0 to 124) as well as a roster class id, with its own `from` year (data in the overlay, not core constants). Windows give each vanilla class its canon decade: Miranda 2275, Constellation 2285, Ambassador 2335, Galaxy and Nebula 2360, Defiant 2370, Sovereign 2370 (the refit `II` hulls 10 to 40 years after the first), the Klingon K't'inga 2265, Vor'cha and K'Vort 2350, Negh'Var 2370, the Romulan D'Deridex 2350 and D'dredar 2360, the Cardassian Galor 2350 and Keldon 2360, the Ferengi D'Kora and Ooron 2360. A catalogue ship the pack lists no window for stays available in every era: the basic tier I utility hulls (scout, colony ship, outpost, troop transport, starbase) and, for the Cardassians, who have no roster hull before 2340, the Netel and Neterok. So a 2240 Federation start offers the NX line, Daedalus and Constitution (2245) plus utility hulls, never a Nebula, Galaxy or Sovereign; every race keeps hulls for every role. With the eras rule off the gate returns at once and nothing changes.

## 2. Research levels 11 to 14

`MaxTechLevel` stays 10 (the classic cap and what `technologies.json` holds). New `HighestTechLevel = 14` and `techCap(state)`: 10, or the pack's `techCap` under the eras rule. Every research loop (turn settlement, grants from events and survey rewards, the validator, the scientific-victory threshold `min(cap, start tech + 3)`, the bridge's research view and the Godot and web research screens) reads `techCap(state)`. Levels 11 to 14 of the six fields are 24 records in the pack's `eras.highTechnologies` (original names and text; costs extend each field's level 5 to 10 curve by its own second difference, so they keep rising smoothly). The research ladder shows 15 rungs; "Level n of 14". No ship requires level 11 to 14 yet: they give the existing linear bonuses and the Galaxies end-state (see section 6).

## 3. Where the data lives (overlay) and where the code is

| | |
|---|---|
| **Data overlay** | `data/overlays/ages.overlay.json` and `galaxies.overlay.json`: the `eras` block (turns per year, start years, start tech, cap, tag years, per-class windows for roster classes and 77 legacy catalogue ships, the 24 research records) and `roster.patch` (`buildable: true` for the canon classes the base held back: Federation Luna, Klingon Qang and Negh'Tev, Romulan Norexan and Mogai, Cardassian Kraxon, Ferengi Nagus). Galaxies adds windows for the Jem'Hadar and Breen lines and the Borg line (Scout Vessel and Probe 2240, Sphere 2280, Cube 2340). The two presets (`ages`, `galaxies`) set the `eras` flag. The Galaxies preset stacks MOD-3's `dominion` overlay under `galaxies` (`overlays: [dominion, galaxies]`), so it plays with the Dominion structure ladder, traits and Jem'Hadar line; Ages stays Ages-only. |
| **Core code** | `Content.h`: the optional `eras` pack object (parsed and validated like `balance`, merged by the same replace-per-key rule; absent from the base pack, so its bytes and checksum are unchanged). `Simulation.cpp`: `techCap`, `campaignYear`, `eraStartTech`, `eraBlocker`, the gate in `constructionTechBlocker`, level-11+ cost and name, ERAS save section, start-fleet pick. `Rules.cpp`: the `eras` flag, validation, re-applying the era with the rule on. `Presets.h`: the flag, `erasOf` for the picker. |
| **Client** | `WebBridge.cpp` (`meta.year`, `techCap`, `erasRule`, per-preset `eras`, research view `techCap` and level 11+ text), Godot `new_campaign_vm.gd` and `new_campaign_screen.gd` (the Starting era row shows `2280  T4` under Ages and Galaxies), `research_vm.gd` and `research_screen.gd` ("of 14"), and the same in `web/src/screens/NewCampaign.tsx` and `Research.tsx`. |

An overlay without an `eras` block (Ultimate, Balance of Power, Dominion War) behaves as before. The `eras` flag needs `expandedRoster` and a pack that carries the block; a preset naming it on a Classic base is refused at load, and a state that turns it on without the data is refused by `validate`.

## 4. Save schema 22

New optional line `ERAS 1`, directly after `THREATS` and before `COMBAT_ORDERS`, written only when the eras rule is on. The pack's data comes through the existing `PACKS` and `CONTENT` lines (schema 17), so an Ages save names its overlay and the checksum of its bytes. Saves 14 to 21 load (each is the schema-22 text without the section; the loader rebuilds the canonical text for the old header). A schema-20 or 21 header over an `ERAS` line is refused. A default Classic or Remastered game differs from main (c95f1c3, schema 21) only in the header line (the fingerprint comparison is in the lane report). Schema 21 is MOD-3's optional `STRUCTURES` section; main was merged into this branch and the section order is STRUCTURES, THREATS, ERAS. MOD-6 takes schema 23.

## 5. Tests

`tests/scenarios/EraTests.cpp` (suite `BotfEraTests`): Classic and Remastered isolation (cap 10, no section, level 11 refused); the eras block; year and start tech for each of the five starts; the gate in each era including queue refusal and retirement leaving ships alone; levels 11 to 14, the cap and the ledger; the ERAS section, round trip, schema 20 migration and refusals; every refusal of a bad `eras` block; the preset flag rules; and 100-turn AI soaks per start for Ages (stock five) and Galaxies (Dominion and Borg), with the year rule checked on every ship the AI builds, two-run determinism at the extremes and save round trips.

## 6. Not here

Ship classes that need research levels 11 to 14 (new future hulls; the roster tops out at tier 10), the minor races above level 10, a turns-per-year setting on the screen, and per-empire era differences (a Klingon in 2380 against a Federation in 2240). Dominion Alliance sub-lines beyond MOD-3's content (separate Cardassian, Breen, Karemma lines) are not added.
