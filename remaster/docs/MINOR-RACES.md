# Minor races and original content (remaster)

Lane G added two things: original descriptions for the reference catalogue, and a data-driven
minor-race roster. The rules live in `game/Source/BotfSim/MinorRaces.h` (header-only, so the web and
Unreal builds need no new source files). `Simulation.cpp` calls them through one-line hooks, each
marked `// Lane G`. Tests are in `tests/scenarios/ContentTests.cpp`.

## 1. Original catalogue text

Every `description` in `ReferenceCatalogue.h` is original remaster text:

| Table | Records | Distinct texts |
|---|---|---|
| Technologies | 77 | 77 |
| Ships | 125 | 109 |
| Buildings | 318 | 114 |

The numeric tables, ids and names are unchanged reference data. The file header says so.
`ContentTests` holds FNV-1a hashes of every normalized sentence of the original prose (939 hashes, no
text). It fails if any catalogue or roster description reuses one of those sentences, or contains a
known marker of the extracted data.

**Warning:** `research/export-reference.py` still emits the original prose. Do not run it over this
file without carrying the descriptions across. The test will catch it if you do.

## 2. Roster

`minors::Roster` holds 30 races. Each one has:

- an id (1..30; saved as `Minor::raceId` and never reused);
- a name, a homeworld and a `RaceCatalogue` portrait id;
- a temperament;
- a starting tech level and population;
- a join bonus;
- an optional signature ship (`ShipCatalogue` id);
- an affinity of -2..+2 toward each major slot (1 Fed, 2 Klingon, 3 Romulan, 4 Cardassian, 5 Ferengi);
- an original description.

Vulcans, Andorians, Bajorans, Trill and the other BOTF minors are minors here. They are not majors.

**Placement.** `generateCampaign*` keeps the starter Bynars and gives them their roster profile.
`placeRosterMinors` then adds minors until there are `rosterTarget(systems) = clamp(systems / 7, 1, 30)`
of them:

| Systems | Minors |
|---|---|
| 14 | 2 |
| 28 | 4 |
| 42 | 6 |
| 56 | 8 |

How placement works:

- The races and the systems are ordered by a hash of the seed. Placement never touches the campaign RNG.
- A minor's system must be at least 3 sectors (Chebyshev) from every empire home, relaxed to 2 if too few systems qualify. It must also be at least 2 from other minors.
- A claimed system is renamed to the homeworld and its planets are regenerated.
- Initial attitude is `60 + 5 × affinity`, minus 5 for Xenophobic races.
- Starting tech is the roster level plus (era − 1), capped at 10.

Existing saves keep exactly the minors they had.

## 3. Rules

A **managed** minor is one with `raceId != 0`. Minors from older saves or hand-built fixtures keep
`raceId 0`. They follow the legacy rules unchanged, and are matched to the roster by name only for
display and for their join bonus.

**Join bonus** (`memberBonuses`). Each member minor adds its bonus to every colony of its empire:

| Bonus | Effect |
|---|---|
| Research, Industry, Food | Percent added to the matching tech multiplier in `economy()` |
| Credits | Percent of colony income |
| Morale | Raises the morale level colonies settle toward (70 + bonus, bonuses capped at +20) |
| GroundDefense | Percent of invasion defense strength |

A minor with no roster profile keeps the legacy +10% research. The Bynars' roster bonus is exactly
that +10%. Bonuses come from current membership only, so secession or conquest removes them at once.

**Joining.** A managed minor's new colony has the minor's own population, with workers fitted to the
default facilities (`configureJoinedColony`). Legacy minors bring 30.

**Review** (`reviewMinorRaces`) runs every 5 turns, before the turn's diplomacy resolution:

- **Technology.** Progress rises by 1 per review, or 2 for Scientific races. Level L advances at L+2 progress, up to level 10. The player is told of level-ups for minors they have met.
- **Population.** Independent minors grow 3% per review (Peaceful and Spiritual 4%, Xenophobic 2%), with a minimum of +1 and a cap of twice the roster size.
- **Affinity.** For each contacted empire, an independent minor's attitude moves toward `50 + 10 × affinity` (−10 more for Xenophobic races) by `max(|affinity|, xenophobic ? 1 : 0)`. Movement downward stops once any treaty exists with that empire. Lane B's drift still applies afterward (DIPLOMACY.md §6).
- **Member loyalty by temperament.** Each review adjusts loyalty to the member's empire:

| Temperament | Loyalty change |
|---|---|
| Warlike | +1 while the empire is at war |
| Peaceful | −1 while the empire is at war |
| Scientific | +1 if the empire's Computers level ≥ the minor's tech level |
| Mercantile | +1 if the empire's credits ≥ 1000 |
| Spiritual | +1 if the minor's colony has morale ≥ 70 |
| Xenophobic | −1 |

  This feeds Lane B's secession check.

**Home defense.** A Warlike member defends its own homeworld against invasion with
+5 + 2 × tech level strength, before the empire's GroundDefense percentage (`invasionDefense`).

## 4. Save support

`Minor` gains `raceId`, `techLevel`, `population` and `progress`, all 0 for legacy minors.

There is one optional section. It comes after `COLONY_GOVERNORS` and before `CHECKSUM`, and is written
only when at least one minor is managed:

```
MINOR_RACES <count>
<systemId> <raceId> <techLevel> <population> <progress>
```

Loading enforces the following:

- The count is between 1 and the number of minors.
- Each row names an existing minor, at most once.
- The raceId is known, unique and matches the minor's name.
- The tech level is 1..10, the population is 1..1000, and progress is below the next threshold.

A save without the section loads its minors as legacy minors. No schema bump was needed. The section
is independent of Lane D's schema-13 sections, so a merge only needs to keep its position just before
`CHECKSUM`.

## 5. For clients

The web and Unreal clients can use:

- `minors::raceFor(minor)` to get a minor's profile: name, homeworld, temperament, `describeJoinBonus`, description and `signatureShipId`;
- `Minor::techLevel` and `Minor::population` for the current progression;
- `minors::memberBonuses(state, empire)` for the empire screen.

## 6. Not done yet

- **Independent minors in combat.** Independent minors cannot be attacked, so only members get the home guard. Minors defending in tactical combat need tactical-engine support.
- **AI invasion odds.** AI invasion-odds estimates (`defStr` in the AI planners) do not include `invasionDefense`, so the AI slightly underestimates minor homeworld defense.
- **Signature ships.** These are data only. They cannot be built yet.
- **Diplomatic victory.** It counts a nominal 30 per independent minor. With more minors, a majority is harder to reach in small galaxies. Consider using `Minor::population` there.
