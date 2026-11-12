# MOD-7: balance and AI overlays (schema 24)

Lane MOD-7 (Barclay, 12 Nov 2026). Outranked by `AI_stuff\standing\simon\botf-rulings.md` (11 Nov: every vanilla feature plus the popular mod packs as new-game presets; 7 to 10 Nov: combat feel is ours; 12 Nov: two lanes at a time). Sources: `research/MOD-PARITY.md` rows E09, N05, X04, X05, X06, X10, X11, X12, X22, X23; `PACKS-AND-PRESETS.md`; `GALAXY-GENERATOR.md` (difficulty); `ERAS.md`; `DOMINION.md`; `THREAT-ACTORS.md`. Private; not for distribution.

**Honesty first.** The mods' real numbers are not documented anywhere I could read (MOD-PARITY.md section 1.4: only opening posts were read). Every figure below is a **design value** chosen to match what the mod's thread says it did (longer battles, firepower cut, stronger batteries, costlier gifts), not data lifted from a mod. Simon can overturn any of them; they are all one-line edits in `data/overlays/*.overlay.json`. Combat is untouched apart from one scale on weapon damage under the presets that ask for it (the 7 to 10 Nov rulings stand: the feel, the WEGO flow, facings and the bursts and spreads are ours).

## 1. What the earlier lanes had wired

M2-GEN (schema 15) already made the AI read difficulty: `difficultyProfile()` (Rules.cpp) scales a rival's colony income, industry and research (`advance()`), its starting treasury (`newCampaign`), the military power it sees in itself when weighing a war (`planAiWarDeclarations`) and the spy-op threshold (`difficultyModifier` in the intelligence turn). The client passes `difficulty` through `meta`/`newCampaign`. What was missing: the **Expert** tier, any AI *behaviour* that changes with difficulty, and every balance value a mod preset could retune beyond research and ship cost percentages. The old note in GALAXY-GENERATOR.md section 2 ("Expert not built here") is closed by this lane.

## 2. Balance values (`balance` block of an overlay)

All optional, all identity in the base pack (so the base bytes and checksum, and every Classic and Remastered game, are unchanged). Each listed value replaces the stack's; the last overlay wins. Ranges: percentages 10 to 1000, flags 0 or 1.

| Key | Identity | Effect |
|---|---|---|
| `researchCostPercent` | 100 | Flat scale on every research cost (MOD-1) |
| `researchCurveLow`, `researchCurveHigh` | 100, 100 | Extra research-cost percent at level 2 and at level 10, linear between (levels above 10 take the high end). A cost curve, not just a scale |
| `shipCostPercent` | 100 | Every ship's industry cost (MOD-1). The build list now shows the scaled cost (it showed the raw cost before) |
| `structureCostPercent` | 100 | Industry cost of the basic colony structures (farm to refinery, battery) |
| `batteryDefensePercent` | 100 | What a powered orbital battery adds to ground defence (vanilla 12 each), in the invasion roll and in the AI's estimate of it |
| `shieldDefensePercent` | 100 | The planetary shield grid: scales a colony's whole ground defence |
| `populationCapacityPercent`, `populationGrowthPercent` | 100, 100 | Population every system carries (current, base and maximum) and the growth rate (basis points, capped at 100%) |
| `weaponOutputPercent` | 100 | Damage of every ship's weapons in tactical combat (applied once, where a ship's attack is derived, so the rules-2 resolver and the flight and WEGO mounts agree) |
| `minorGiftCost`, `minorGiftAttitude` | 50, 20 | Credits a gift to a minor costs, and the attitude it gives (N05) |
| `intelOutputPercent`, `aiIntelPercent` | 100, 100 | Spy output of every empire, and an extra scale on the AI empires only (X12) |
| `aiAlwaysStarbases`, `aiNewestShips`, `aiStandardBuild`, `aiAggressiveExpansion` | 0 | AI policy flags (section 4) |
| `starTypesPerRace` | 0 | Race home stars take their race's star type (section 7) |

## 3. Per-empire traits (E09), minor gifts (N05), intel (X12)

**Traits** are data: `races[].major.traitEffects`, which MOD-3 introduced (morale, creditsPct, groundDefensePct, securityPct), gains `industryPct`, `researchPct`, `foodPct` (colony output; ride on `memberBonuses`, so every economy consumer sees them), `shipCostPct` (added to the empire's ship build cost; negative is cheaper) and `intelPct` (spy output), plus `homeStar`. Ranges -50 to 100 (`intelPct` -80 to 200). All zero in the base pack.

**All The Ages (the `ages` overlay)** gives each stock empire a strength and an offsetting weakness, toned down (design values):

| Empire | Strengths | Offsets |
|---|---|---|
| Federation | research +8%, morale +3 | ground defence -10%, ships cost +5% |
| Klingon | ground defence +20%, ships cost -8% | research -10%, credits -5% |
| Romulan | spy output +20%, ships cost -3% | food -8%, morale -2 |
| Cardassian | industry +8%, security +15%, spy output +10% | food -10% |
| Ferengi | credits +15% | ships cost +8%, ground defence -15%, industry -3% |

The test checks that every empire has at least one of each.

**Ultimate (UM5)** cuts the Cardassian and Romulan spy output by 25% (`intelPct` -25) and the AI's by another 25% (`aiIntelPercent` 75): "intel adjusted so the AI frames the player less". Spy output feeds every intel area, so this also thins their internal security; that is the cost of keeping it one number.

**Dominion War (UDM)** sets `minorGiftCost` 100 and `minorGiftAttitude` 25 (costlier gifts that give more). The AI's gift rule (`planRivals`) and the refund of a gift to a minor that joined elsewhere read the same value. The attitude and membership thresholds (50 and 80) are unchanged.

## 4. AI policy (X10)

`AiPolicy aiPolicy(state)` (Rules.cpp) is the one place the AI's behaviour is decided: the four pack flags, plus the difficulty: **Hard** adds the standard build order; **Expert** and **Impossible** switch all four on. A Normal game with no flags has all four off, so the old AI plays unchanged.

* **Always builds starbases** (`Stations.cpp`, `planRivalStations`): the empire keeps building outposts at war too, wants `min(6, 2 + colonies/2)` stations instead of `min(4, 1 + colonies/2)`, and starts an outpost-to-starbase upgrade as soon as it can pay one (not three times over).
* **Prefers newest ships** (`queueMilitaryShip`): the class with the highest technology requirements wins (ties: the dearer, then the lower id); otherwise the cheapest or best-value one as before. Era gates, tech and the Ketracel-White rule still filter the candidates first.
* **Standard build order** (`planRivals`): a second laboratory (after the race structures, academy and upgrades), and a defence battery on every colony that has a rival colony within five sectors (not only at war).
* **Aggressive expansion**: up to four colony ships out or queued (two otherwise), and colony ships queued ahead of warships.

Presets: Balance of Power sets starbases, newest ships and the standard build; Ultimate sets the standard build and aggressive expansion (UM5's "smoother, more aggressive expansion"); Dominion War sets the standard build ("AI buy priorities standardised").

**AI and the vanilla rules (lead's addition, 12 Nov).** MOD-6 put intercept, raid and crew experience on in Classic and Remastered. Its AI already holds intercept posts and raids at war (`economy_rules::planOrders`; the soak shows hundreds of AI order-turns). The AI did not build its race's academy, so `structures::aiAcademy` (Structures.h) now queues the vanilla academy (records 25, 37, 43, 50) in a colony with a shipyard, 300 credits and an industry that can pay for it in 150 turns, while crew experience is on. This is the one deliberate change to a default Classic or Remastered game in this lane; a build with that rule switched off is byte-identical to main (section 9).

## 5. Difficulty (X22)

Six tiers in play order: Simple, Easy, Normal, Hard, **Expert**, Impossible (`DifficultyOrder`; `difficultyRank`). `Difficulty::Expert` is stored as 5 so Impossible keeps its 4 and old saves read unchanged.

| Tier | Economy, treasury, war confidence, spy threshold | AI policy |
|---|---|---|
| Simple, Easy | handicaps (75% and 90%) | none |
| Normal | identity | none |
| Hard | 115% | standard build order |
| **Expert** | **identity: no bonus of any kind** | all four behaviours |
| Impossible | 135% output, 140% treasury, 130% war confidence, spy readiness | all four behaviours |

Expert is "Impossible without the bonuses": it plays the way Impossible does and cheats in nothing. The AI reads the tier through `aiPolicy` at the three places above and through `difficultyProfile` as before (verified, not assumed: the soak and the tier table below run through those paths).

**Win-rate table.** `BotfDifficultyTests` plays each tier on the same eight seeds for 100 turns, four empires: three AI rivals against a baseline empire that only runs colony governors, and counts a rival as winning when its strength score (tech levels, colonies and population, fleet attack and hull, stations) beats the baseline's. It is AI against a fixed baseline, not AI against AI, because the difficulty applies to every non-player empire at once (a head-to-head between tiers would need a per-empire level, which nothing asks for). The table is printed by the suite and copied into the lane report.
Run of 12 Nov 2026 (8 seeds x 3 rivals, 100 turns, Remastered rules):

| Tier | Rival strength score | Rival win % | Rival colonies | Rival tech levels (sum of six fields) | Baseline player score |
|---|---|---|---|---|---|
| Simple | 478 | 91.7 | 4.5 | 10.0 | 238 |
| Easy | 508 | 91.7 | 4.8 | 10.4 | 237 |
| Normal | 515 | 91.7 | 4.7 | 10.8 | 236 |
| Hard | 559 | 100 | 4.5 | 12.9 | 220 |
| Expert | 634 | 100 | 4.9 | 13.0 | 239 |
| Impossible | 738 | 100 | 5.8 | 14.0 | 239 |

The strength score is strictly ordered Simple < Easy < Normal < Hard < Expert < Impossible, and Expert (no bonus at all) beats Hard (+15% output) by playing better. The win rate is coarse: a passive baseline loses to almost any AI, so it saturates at 100% from Hard on. The score is the sensitive measure, and the suite checks the score order and that the win rate never falls along the tiers.

## 6. Galaxy options (X06)

New `CampaignSetup` fields (not saved: their results are in the state): `startSystems` (0 to 8 extra colonies per empire, taken round-robin from the nearest ordinary stars that hold a habitable planet), `planetsMin` / `planetsMax` (1 to 9 planets per ordinary star; empire homes keep their five), `starTypes` (-1 the pack's default, 0 off, 1 on). `applyGalaxyOptions` (Simulation.cpp) runs at the end of `newCampaign`; with every option at its default it changes nothing (the test compares fingerprints). Extra colonies start small (population up to 20, one farm, factory and laboratory). Bridge options: `startSystems`, `planetsMin`, `planetsMax`, `starTypes`; presets carry them as option defaults (Balance of Power: 1 extra colony, 2 to 9 planets; Dominion War: 1 extra colony; Galaxies: star types on). Godot and web screens: "Planets per star" (Standard, Sparse, Rich, Wide, Crowded), "Start systems" (0, 1, 2, 3, 4, 6, 8) and a "Race star types" checkbox.

## 7. Star types per race (X23)

`System.starType` (0 yellow, 1 orange, 2 red, 3 white, 4 blue, 5 green, 6 violet; -1 not set), saved in the optional `STAR_TYPES` section. Under the option, each empire's capital takes its race's star: the race's `homeStar` if the pack sets one, else a built-in table of design values (Federation yellow, Klingon red, Romulan green, Cardassian orange, Ferengi white, Dominion blue, Borg violet, Breen white). Only capitals are set; every other star keeps the client's own deterministic draw (the five hashed types are unchanged; green and violet are never drawn by the hash). The bridge sends `starType` for explored systems; Godot (`galaxy_map.gd`, `galaxy_layout.gd`) and web (`renderer.ts`) draw it.

## 8. Armed transports and colony ships (X04)

The Cardassians already had armed colony and troop ships in vanilla (attack 48 and 208). The `dominion-war` overlay patches the other four races' records to carry weapons: tier I attack 40, tier II 160, renamed "Armed Colony Ship" and "Assault Transport" (design values, a little under the Cardassian figures because those hulls are weaker). The tactical engine already fires a transport or colony ship's attack as beams. Classic and the other presets keep unarmed hulls.

## 9. Save schema 24

* Header `BOTF_SIM 24 remaster-24`. Saves 14 to 23 load (a schema-22 or -23 save without the newer sections differs from the schema-24 text by the header alone; the loader rebuilds the canonical text for the old header). MOD-6's schema 23 is merged in (ECONOMY_RULES); saves 14 to 23 load.
* `GALAXY_SETUP` accepts difficulty 5 (Expert) only under a schema-24 header.
* `STAR_TYPES 1 <n>` then `<systemId> <type>` per typed system, after `ERAS`, written only while a system has a star type. Refused under an older header, with a wrong count, an unknown system or a type outside 0 to 6.
* The balance values and traits live in the pack data, so they reach the save through the existing `PACKS` and `CONTENT` lines (schema 17).
* A default Classic or Remastered game differs from main (059ec08, schema 23) in the header line alone, once the AI academy rule is switched off in a proof build (12 of 12 saves identical, seeds 1999, 7 and 42, 20 and 60 turns, both rulesets; with the rule on the AI builds its academy, which changes the AI's queue and so the later game).

## 10. Tests

`tests/scenarios/BalanceTests.cpp` (suite `BotfBalanceTests`): isolation, every balance key and its range, the research curve, ship and structure cost and the ship-cost trait, trait output, batteries and shields, populations, weapon output in a tactical encounter, gifts, the intel scale, armed transports, the AI policy per tier, newest ships, starbases, Expert, the galaxy options and their refusals, star types and the save section, migration from schema 22, the preset values, and a 100-turn soak per mod preset (two-run determinism, save round trip every 25 turns). `tests/scenarios/DifficultyTests.cpp` (suite `BotfDifficultyTests`): the tier table above and its order.

## 11. Design values Simon may want to overturn

* Every number in the overlays (Balance of Power: research x105 with a 85% to 125% curve, ships x90, structures x85, weapons x75, population capacity x110; Ultimate: research x85 with a 100% to 90% curve, weapons x85, AI spy output x75; Dominion War: batteries x150, shields x115, population capacity x115, gifts 100 credits for +25, research curve 90% to 110%; Ages: weapons x85 and the trait table in section 3).
* The AI policy bundles (which mod sets which flag) and the Hard tier getting the standard build order.
* Star types per race; extra colonies' size; the planet range names.
* The Expert placement between Hard and Impossible and its definition: no economy bonus, all four behaviours.

## 12. Not done

* Structure auto-upgrade, protectorate, raid and crew experience are MOD-6's.
* The AI does not use the armed transports or colony ships tactically beyond what the battle engine already does.
* Trait text on the empire-select screen; the per-empire traits show in the data, not yet in the UI.
* Per-star star types for non-capitals and for minors; a star-type effect on rules (the type is presentation).
* The mod-specific building tables of each pack (the Ultimate and Dominion structures beyond MOD-3's ladder).
