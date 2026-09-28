# Expanded ship roster

The remaster adds 98 ship classes on top of the 125 legacy reference records in `ReferenceCatalogue.h`. The new
classes cover canon Star Trek from *Enterprise* through *Nemesis*. Kelvin-timeline classes and classes taken from
mods are included but kept apart. The Remastered preset makes 43 canon major-empire classes buildable. The rest are
reference data: other powers, unique vessels, stations, mod-derived classes and Kelvin classes. Every description is
original remaster text.

Classic games, legacy generators and every existing save behave exactly as before: they never see an id of 1000 or
more. The Classic 250-turn reference hashes are unchanged (section 6).

## 1. Where things live

| File | Role |
|---|---|
| `tools/ship_roster/gen_ship_roster.py` | **Source of truth.** Class data, balancing formulas and generators. Run it after any edit; `--check` fails if the outputs are stale. |
| `game/Source/BotfSim/ShipRosterData.h` | Generated `constexpr` roster (`botf::roster::ExpandedRoster`). Do not edit by hand. |
| `game/Source/BotfSim/ShipRosterTypes.h` | `RosterClass`, `WeaponMount` and the faction, role, era, weapon, arc and source enums. |
| `game/Source/BotfSim/ShipRoster.h` | Accessors: `findShip(id)` covers legacy and expanded ids, plus `findRosterClass`, `isExpandedId` and `ExpandedShipRecords`, which are the same classes as `ShipRecord`s. |
| `docs/ship-asset-manifest.template.json` | Generated list of every `assetKey` and weapon key, for the local HD asset manifest. |
| `tests/scenarios/ShipRosterTests.cpp` | The `BotfShipRosterTests` suite. |

The roster is header-only, so no `.cpp` needs adding to CMake, `SimulationBridge.cpp` or `Build-Simulation.ps1`.

## 2. Data model

Each `RosterClass` has these fields:

- **Identity:** `id` (stable, 1000 to 1999, never reused), `assetKey`, `faction`, `raceId` and `minorRaceId`.
  - `raceId` is the `RaceCatalogue` id. It is 36 for powers outside that catalogue, the same value the legacy monsters use.
  - `minorRaceId` is the `MinorRaces.h` roster id when the faction is one of the 30 managed minors.
- **Role:** `hullRole` is descriptive (scout, science, escort, frigate, destroyer, raider, light/heavy cruiser, strike cruiser, carrier, battlecruiser, explorer, battleship, dreadnought, colony, transport, station, special). `engineRole` is the legacy `ShipRecord::role` the campaign understands:
  - 0 scout
  - 1 to 4 warship
  - 5 colony
  - 7 station
  - 8 special
  - 9 transport
- **Era and technology:** `era` (Enterprise, Original, Movie, NextGeneration, DominionWar, Nemesis, Kelvin, Future), `tier` from 0 to 10, and six `technologyRequirements` in the existing tech-tree fields (Biotech, Computers, Construction, Energy, Propulsion, Weapons). These requirements are what gate building.
- **Economy:** `industryCost`, `maintenance` (credit upkeep) and `buildTurns`.
  - `buildTurns` is nominal: turns at 600 industry per turn. The core builds from colony industry, as for every other project.
- **Combat:** `hull`, `shields`, `attack`, `torpedoSharePercent`, up to four typed `WeaponMount`s (type, count, arc), `speed` 1 to 3, `range` 0 to 2, `sensors` 1 to 5 (informational) and `cloak`.
- **Provenance:** `sourceKind` (Canon, ModDerived or Kelvin), `source` (the episode or film, or the mod note) and `buildable`.

Weapon types:

- Beams: phase cannon, phaser bank, phaser array, pulse phaser, disruptor, disruptor cannon, plasma cannon, polaron beam, cutting beam.
- Special: energy dampener, Tholian web, bio-energy, temporal weapon.
- Torpedoes: spatial, photonic, photon, quantum, plasma, tricobalt, generic.

Arcs are Forward, ForwardWide, Broadside, Aft and All (360°).

### Asset keys

`assetKey` looks like `ship.<faction>.<class>`, for example `ship.fed.intrepid` or `ship.kli.d7`. Weapon effects use
`weapon.<WeaponType>`, for example `weapon.QuantumTorpedo`. The client resolves both keys in a local manifest shaped
like `docs/ship-asset-manifest.template.json`:

- a ship key maps to its model, hull texture, engine audio and ambient audio;
- a weapon key maps to its VFX, fire audio and impact audio.

Models and audio are not in the repository. A missing key should fall back to the interim art.

## 3. Campaign integration

- **Ruleset flag.** `GameplayRules::expandedRoster`:
  - `rulesForPreset(Remastered)` sets it.
  - Classic forbids it, and `validate()` rejects a Classic game with the flag on.
  - Remastered allows it off, so Remastered saves made before the roster keep playing as they did.
- **Building.** With the flag on, `constructionChoices` lists the empire's buildable expanded classes after the legacy list. The same technology blockers apply. `constructionTechBlocker` refuses an expanded id when the flag is off. `playableShip()` refuses data-only classes.
- **Validation.** Queues, fleets and build templates holding an expanded id need the flag. Commands and templates accept any id `findShip` resolves.
- **AI** (`queueMilitaryShip`).
  - With the flag on, a rival no longer picks the cheapest legacy hull for a role. It picks the best value across both catalogues:
    `(attack/20 + (hull+shields)/10 + 10) * 100000 / industryCost`, integer math, ties to the lower id.
  - With the flag off, the old rule runs unchanged.
- **Starting fleets and stations** still use the legacy catalogue, so a Classic game and a Remastered game from the same seed start alike.
- **Tactical combat** (`TacticalCombat.h`):
  - An expanded warship's beam/torpedo split comes from `torpedoSharePercent`. An Akira fights as a torpedo boat and a D7 as a disruptor ship.
  - Cloaking follows the class `cloak` flag instead of the owner rule, so a 22nd-century Klingon hull or a D7 cannot cloak and a D12 can.
  - Legacy classes keep the original rules exactly: a 65/35 beam/torpedo split for every warship, and cloaking for every Klingon and Romulan warship.
  - The legacy role-3 "strike cruiser" branch is unreachable, because every reference warship is a `Building::Destroyer`. It was left alone to keep Classic identical.
- **Web bridge.** Every per-id lookup in `web/bridge/WebBridge.cpp` goes through `roster::findShip`, so expanded ships show their names, speed, upkeep and cost. The legacy-only listings (tech unlocks, catalogue dump, invasion picks) are unchanged. The wasm build was not rebuilt; CMake compiles the bridge natively.
- **Unreal client.** Its reference-catalogue browser still indexes the legacy array with bounds checks, so it simply does not list expanded classes yet. This is an open item.

## 4. Save format: no schema bump

The roster adds an optional section, following the precedent of `MINOR_RACES`:

```
SHIP_ROSTER 1        (after MINOR_RACES, before COMBAT_ORDERS; only when rules.expandedRoster)
```

The `1` is the roster data revision. Any other value is rejected.

- Saves without the section load with the roster off, and their text is unchanged. This covers every schema 4 to 14 save, every Classic save and Remastered saves made before the roster.
- A save that holds expanded ids but has no section fails validation.
- Like `MINOR_RACES`, the section is accepted under a schema-13 header, whose canonical form it belongs to. Headers of schema 12 and older reject it.
- The canonical-form check covers the section, and the fingerprint covers the whole payload.

Header and `ContentVersion` stay `BOTF_SIM 14 remaster-14`.

## 5. Balancing rationale

The generator applies these formulas. All are integer and calibrated against the legacy catalogue:

- **Weapon points.** Each mount contributes `perMount × count × arc% / 100`.
  - `perMount` values: phase cannon 5, phaser bank 7, phaser array 10, pulse phaser 13, disruptor 9, disruptor cannon 11, plasma cannon 6, polaron 12, cutting beam 20, energy dampener 4, Tholian web 6, bio-energy 30, temporal 40, spatial torpedo 4, photonic 6, photon 9, quantum 14, plasma torpedo 12, tricobalt 18, generic torpedo 8.
  - Arc percentages: Forward 100, ForwardWide 110, Broadside 90, Aft 50, All 120.
- **Attack.** `points × (250 + 50t + 25t²) / 100` for tier `t`. A tier-1 cruiser lands near the legacy Ambassador or K't'inga (a few hundred). A tier-9 capital ship lands near the legacy "II" refits (4,000 to 6,000).
- **Hull and shields.** `factionBase × size% × (100 + 5t)` for hull and `(100 + 6t)` for shields.
  - Faction bases carry the legacy identity. Cardassian hulls are heavy with thin shields (250/300). Federation hulls are light with thick shields (150/400). Ferengi shields are thick (120/380). Klingon (140/340) and Romulan (135/360) sit between.
  - Sizes run from scout 30% through cruiser 100% to dreadnought 190%.
- **Cost.**
  - Base: `1800 × size% × (100 + 20t + t²) + 2 × attack`.
  - Adjustments: the faction cost percentage (Klingon and Cardassian 95, Ferengi 105), the civil-role discount (scout 50%, science 55%, colony 70%, transport 60%) and a 10% cloak premium. The result is rounded to 10.
  - The attack term makes firepower cost what it is worth.
- **Upkeep.** Taken from the legacy role table (scout 26, destroyer 59, cruiser 91, strike 73, capital 115 to 140, colony 23, transport 26), plus 10% for a cloak.
- **Technology.** Level `L = tier`, adjusted by role and clamped to 1..10:
  - warships: Biotech L-1 and Weapons L+1;
  - scouts and science ships: one level lower in Biotech, Construction, Energy and Weapons;
  - colony ships and transports: minimal Weapons.

  Enterprise-era (tier 0) hulls are therefore available at the start. They are cheap and weak and become obsolete quickly. Prometheus-class ships need tier-9 research.

This leaves a Remastered empire a real choice at every stage of the game:

- Early: NX, Daedalus or Constitution against the legacy Miranda or Ambassador.
- Middle: Excelsior, the Wolf 359 classes and Intrepid.
- Late: Akira, Steamrunner or Prometheus against the legacy Sovereign.

The AI value rule prefers dense, current designs, so rival fleets modernize instead of spamming the cheapest hull.

Balance is provisional, like the rest of the remaster. Change the constants in the generator and rerun it;
`BotfShipRosterTests` re-derives attack from the weapons, so the header and the rules cannot drift apart.

## 6. Scope and source policy

- **Canon.** The class appears on screen, or as a named model, in TOS, TAS, TNG, DS9, VOY, ENT or a TOS/TNG-era film.
  - Classes already in the legacy catalogue are not duplicated: Miranda, Oberth, Constellation, Ambassador, Galaxy, Galaxy-X, Nebula, Sovereign, Defiant, K't'inga, B'rel, K'Vort, Vor'cha, Negh'Var, D'deridex, Galor, Keldon, D'Kora, the Borg cube and the minor-race legacy ships.
  - Canon is thin for some factions: Cardassian, Ferengi and early Klingon designs. Their additions are few rather than invented.
  - TAS added no distinct class worth a slot.
- **Buildable.** Only canon classes of the five major empires in a campaign role. Everything else is data only:
  - the Dominion, Borg, Breen and other powers, which have no major-empire slot yet;
  - stations, which campaign starbases handle through the station rules;
  - unique vessels such as the Wells, the Scimitar and the Xindi weapon.
- **Minor races.** The Andorian, Vulcan, Bajoran, Nausicaan and Talarian classes link to their managed-minor roster ids. They are data for a future "member worlds contribute ships" feature; the legacy signature ships stay as they were.
- **Mod-derived** (Luna, Qang, Negh'Tev, Norexan, Mogai, Kraxon, Nagus). These names recur in popular BOTF community rosters (Ultimate Universe, Galaxies at War, Vanilla+) and in licensed games.
  - Mod versions differ and we do not reproduce them. We make no claim about which mod added which class, or with what stats.
  - They are marked `ModDerived`, are data only and use remaster stats.
  - Armada 3 and Bridge Commander Remastered packs contributed no extra classes beyond the canon ones already listed.
- **Kelvin.** Kelvin, Constitution (Kelvin), Vengeance, D4 and Narada are flagged `Kelvin` and are never buildable in a prime-timeline campaign.
- **Sources.** Source tags cite a representative episode or film, not every appearance.

### Verification

Verified on 28 September 2026:

- `cmake -S remaster -B build && cmake --build build -j8 && ctest --test-dir build`. Every suite passes, including the soak and the new `BotfShipRosterTests`.
- `BotfSim --reference --seed 26713 --empires 5 --player 4 --turns 250` gives the same `FINAL_HASH` as before the roster in these runs:

  | Run | FINAL_HASH |
  |---|---|
  | `--ruleset classic` | 10322941550370757609 |
  | `--ruleset classic --era supreme` | 9777405856061224123 |

  Both plain legacy runs (`--seed 5 --turns 100`, with and without `--reference`) also give an unchanged 3306609107525559830.
- The Remastered hash changes by design, because rivals now build expanded classes. It is 4217139127677146782, and GCC and Clang agree on it.
- Tactical rules 3 (COMBAT-RULES.md) changed the Remastered hash again, by design. It is now 16105159682523617496,
  and GCC and Clang agree on it. The Classic hashes above are unchanged. A Remastered save made before rules 3 has no
  `TACTICAL_FLIGHT` section, so it continues on rules 2 exactly as before.

## 7. Full roster table

The table below is generated.

- Tech gives the six requirement levels.
- Turns is the nominal build time at 600 industry per turn.
- Spd/Rng/Sen is speed, range and sensors.
- Flight is turn rate (degrees per round) / acceleration / lateral cells, then size class. It is the tactical rules 3 flight model (COMBAT-RULES.md).
- Build shows "yes" (buildable in Remastered) or "data" (data only).

<!-- ROSTER-TABLE:BEGIN (generated by tools/ship_roster/gen_ship_roster.py) -->
| Id | Faction | Class | Role | Era | Tier | Tech (Bio/Com/Con/Ene/Pro/Wea) | Cost | Turns | Upkeep | Hull | Shields | Attack | Weapons | Spd/Rng/Sen | Flight | Cloak | Build | Source | assetKey |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1000 | Federation | NX | Light Cruiser | Enterprise | 0 | 1/1/1/1/1/1 | 1830 | 4 | 85 | 135 | 360 | 105 | 4x PhaseCannon (ForwardWide); 2x PhaseCannon (Aft); 2x PhotonicTorpedo (Forward); 1x PhotonicTorpedo (Aft) | 2/2/3 | 90/2/1 Medium | - | yes | ENT (Enterprise NX-01, 'Broken Bow' onward) | `ship.fed.nx` |
| 1001 | Federation | Daedalus | Cruiser | Enterprise | 1 | 1/1/1/1/1/2 | 2490 | 5 | 91 | 157 | 424 | 156 | 6x PhaseCannon (All); 2x PhotonicTorpedo (Forward) | 2/2/3 | 90/2/0 Large | - | yes | Model in DS9 (Sisko's office); named in TNG | `ship.fed.daedalus` |
| 1002 | Federation | Y-class | Transport | Enterprise | 0 | 1/1/1/1/1/1 | 600 | 1 | 26 | 82 | 220 | 5 | 1x PhaseCannon (Aft) | 1/1/1 | 45/1/0 Medium | - | yes | ENT ('Fortunate Son') | `ship.fed.yclass` |
| 1003 | Federation | Constitution | Heavy Cruiser | Original | 1 | 1/1/1/1/1/2 | 3160 | 6 | 100 | 196 | 530 | 221 | 6x PhaserBank (All); 2x PhotonTorpedo (Forward) | 2/2/4 | 45/1/0 Large | - | yes | TOS (USS Enterprise NCC-1701) | `ship.fed.constitution` |
| 1004 | Federation | Constitution (refit) | Heavy Cruiser | Movie | 2 | 1/2/2/2/2/3 | 4040 | 7 | 100 | 205 | 560 | 400 | 8x PhaserBank (All); 2x PhotonTorpedo (Forward); 1x PhotonTorpedo (Aft) | 2/2/4 | 45/1/0 Large | - | yes | Star Trek: The Motion Picture; films II-VI | `ship.fed.constitution_refit` |
| 1005 | Federation | Soyuz | Frigate | Movie | 2 | 1/2/2/2/2/3 | 2220 | 4 | 55 | 106 | 291 | 270 | 5x PhaserBank (All); 2x PhotonTorpedo (Forward) | 2/1/2 | 90/2/1 Medium | - | yes | TNG ('Cause and Effect', USS Bozeman) | `ship.fed.soyuz` |
| 1006 | Federation | Sydney | Transport | Movie | 2 | 1/1/2/1/2/1 | 890 | 2 | 26 | 90 | 246 | 31 | 1x PhaserBank (Forward) | 2/1/1 | 45/1/0 Medium | - | yes | TNG ('Relics', USS Jenolan) | `ship.fed.sydney` |
| 1007 | Federation | Excelsior | Heavy Cruiser | Movie | 3 | 2/3/3/3/3/4 | 5130 | 9 | 100 | 215 | 590 | 662 | 10x PhaserBank (All); 2x PhotonTorpedo (Forward); 1x PhotonTorpedo (Aft) | 2/2/4 | 45/1/0 Large | - | yes | Star Trek III: The Search for Spock; TNG, DS9, VOY | `ship.fed.excelsior` |
| 1008 | Federation | Raven | Scout | NextGeneration | 2 | 1/2/1/1/2/1 | 420 | 1 | 26 | 49 | 134 | 31 | 1x PhaserBank (Forward) | 3/2/5 | 180/3/2 Small | - | yes | VOY ('The Raven') | `ship.fed.raven` |
| 1009 | Federation | Challenger | Cruiser | NextGeneration | 4 | 3/4/4/4/4/5 | 5470 | 10 | 91 | 180 | 496 | 969 | 8x PhaserArray (All); 2x PhotonTorpedo (Forward) | 2/1/3 | 90/2/0 Large | - | yes | DS9 ('Emissary', Wolf 359) | `ship.fed.challenger` |
| 1010 | Federation | Cheyenne | Cruiser | NextGeneration | 4 | 3/4/4/4/4/5 | 5470 | 10 | 91 | 180 | 496 | 969 | 8x PhaserArray (All); 2x PhotonTorpedo (Forward) | 2/1/3 | 90/2/0 Large | - | yes | DS9 ('Emissary', Wolf 359) | `ship.fed.cheyenne` |
| 1011 | Federation | Springfield | Light Cruiser | NextGeneration | 4 | 3/4/4/4/4/5 | 4550 | 8 | 85 | 162 | 446 | 688 | 6x PhaserArray (All); 1x PhotonTorpedo (Forward) | 2/1/4 | 90/2/1 Medium | - | yes | DS9 ('Emissary', Wolf 359) | `ship.fed.springfield` |
| 1012 | Federation | Freedom | Frigate | NextGeneration | 4 | 3/4/4/4/4/5 | 3470 | 6 | 55 | 116 | 322 | 586 | 5x PhaserArray (All); 1x PhotonTorpedo (Forward) | 3/1/2 | 90/2/1 Medium | - | yes | DS9 ('Emissary', Wolf 359) | `ship.fed.freedom` |
| 1013 | Federation | Niagara | Heavy Cruiser | NextGeneration | 4 | 3/4/4/4/4/5 | 6820 | 12 | 100 | 224 | 620 | 1207 | 10x PhaserArray (All); 2x PhotonTorpedo (Forward); 1x PhotonTorpedo (Aft) | 2/1/3 | 45/1/0 Large | - | yes | DS9 ('Emissary', Wolf 359) | `ship.fed.niagara` |
| 1014 | Federation | New Orleans | Frigate | NextGeneration | 4 | 3/4/4/4/4/5 | 3820 | 7 | 55 | 116 | 322 | 765 | 6x PhaserArray (All); 2x PhotonTorpedo (Forward) | 2/1/2 | 90/2/1 Medium | - | yes | TNG (USS Kyushu, 'The Best of Both Worlds II') | `ship.fed.new_orleans` |
| 1015 | Federation | Olympic | Science Vessel | NextGeneration | 4 | 3/4/3/3/4/3 | 1100 | 2 | 30 | 80 | 223 | 204 | 2x PhaserArray (All) | 2/2/5 | 135/2/1 Small | - | yes | TNG ('All Good Things...', USS Pasteur) | `ship.fed.olympic` |
| 1016 | Federation | Centaur | Destroyer | DominionWar | 5 | 4/5/5/5/5/6 | 5260 | 9 | 59 | 140 | 390 | 1113 | 6x PhaserArray (All); 3x PhotonTorpedo (Forward) | 3/1/2 | 90/2/1 Medium | - | yes | DS9 ('A Time to Stand') | `ship.fed.centaur` |
| 1017 | Federation | Danube | Scout | DominionWar | 5 | 4/5/4/4/5/4 | 960 | 2 | 26 | 56 | 156 | 348 | 2x PhaserArray (ForwardWide); 1x PhotonTorpedo (Forward) | 3/1/4 | 180/3/2 Small | - | yes | DS9 (runabouts of Deep Space 9) | `ship.fed.danube` |
| 1018 | Federation | Nova | Science Vessel | DominionWar | 5 | 4/5/4/4/5/4 | 1710 | 3 | 30 | 83 | 234 | 641 | 4x PhaserArray (All); 1x PhotonTorpedo (Forward) | 2/2/5 | 135/2/1 Small | - | yes | VOY ('Equinox') | `ship.fed.nova` |
| 1019 | Federation | Intrepid | Light Cruiser | DominionWar | 6 | 5/6/6/6/6/7 | 8530 | 15 | 85 | 175 | 489 | 2189 | 10x PhaserArray (All); 3x PhotonTorpedo (Forward); 1x PhotonTorpedo (Aft) | 3/2/5 | 90/2/1 Medium | - | yes | VOY (USS Voyager) | `ship.fed.intrepid` |
| 1020 | Federation | Yeager | Escort | DominionWar | 6 | 5/6/6/6/6/7 | 4800 | 8 | 50 | 106 | 299 | 1131 | 5x PhaserArray (All); 2x PhotonTorpedo (Forward) | 3/1/2 | 135/3/2 Small | - | yes | DS9 ('What You Leave Behind') | `ship.fed.yeager` |
| 1021 | Federation | Curry | Frigate | DominionWar | 6 | 5/6/6/6/6/7 | 6560 | 11 | 55 | 126 | 353 | 1783 | 8x PhaserArray (All); 3x PhotonTorpedo (Forward) | 2/1/2 | 90/2/1 Medium | - | yes | DS9 ('A Time to Stand') | `ship.fed.curry` |
| 1022 | Federation | Akira | Strike Cruiser | DominionWar | 7 | 6/7/7/7/7/8 | 11550 | 20 | 73 | 202 | 568 | 3175 | 6x PhaserArray (All); 9x PhotonTorpedo (ForwardWide); 3x PhotonTorpedo (Aft) | 2/1/3 | 90/2/1 Large | - | yes | Star Trek: First Contact; DS9 | `ship.fed.akira` |
| 1023 | Federation | Steamrunner | Destroyer | DominionWar | 7 | 6/7/7/7/7/8 | 7950 | 14 | 59 | 151 | 426 | 2025 | 7x PhaserArray (All); 3x PhotonTorpedo (Forward) | 3/1/2 | 90/2/1 Medium | - | yes | Star Trek: First Contact | `ship.fed.steamrunner` |
| 1024 | Federation | Saber | Escort | DominionWar | 7 | 6/7/7/7/7/8 | 6150 | 11 | 50 | 110 | 312 | 1642 | 6x PhaserArray (All); 2x PhotonTorpedo (Forward) | 3/1/2 | 135/3/2 Small | - | yes | Star Trek: First Contact; DS9 | `ship.fed.saber` |
| 1025 | Federation | Norway | Light Cruiser | DominionWar | 7 | 6/7/7/7/7/8 | 8840 | 15 | 85 | 182 | 511 | 2080 | 8x PhaserArray (All); 2x PhotonTorpedo (Forward) | 2/1/3 | 90/2/1 Medium | - | yes | Star Trek: First Contact | `ship.fed.norway` |
| 1026 | Federation | Prometheus | Battlecruiser | Nemesis | 9 | 8/9/9/9/9/10 | 20430 | 35 | 115 | 292 | 831 | 5831 | 12x PhaserArray (All); 4x QuantumTorpedo (ForwardWide); 2x PhotonTorpedo (Aft) | 3/1/3 | 45/2/0 Large | - | yes | VOY ('Message in a Bottle') | `ship.fed.prometheus` |
| 1027 | Federation | Rhode Island | Frigate | Nemesis | 8 | 7/8/8/8/8/9 | 8290 | 14 | 55 | 135 | 384 | 2250 | 6x PhaserArray (All); 2x QuantumTorpedo (Forward) | 3/1/2 | 90/2/1 Medium | - | yes | VOY ('Endgame') | `ship.fed.rhode_island` |
| 1028 | Federation | Peregrine | Raider | NextGeneration | 4 | 3/4/4/4/4/5 | 2510 | 5 | 54 | 108 | 297 | 195 | 2x PhaserBank (Forward); 1x PhotonTorpedo (Forward) | 3/2/2 | 135/3/2 Small | - | yes | TNG ('Preemptive Strike'); VOY ('Caretaker') | `ship.fed.peregrine` |
| 1029 | Federation | Wells | Special | Future | 10 | 9/10/10/10/10/10 | 23240 | 39 | 150 | 450 | 1280 | 4420 | 8x PhaserArray (All); 1x TemporalWeapon (Forward) | 3/2/5 | 90/2/1 Large | - | data | VOY ('Relativity'; 29th-century timeship) | `ship.fed.wells` |
| 1030 | Federation | Luna | Explorer | Nemesis | 9 | 8/9/9/9/9/10 | 20370 | 34 | 115 | 326 | 924 | 5313 | 12x PhaserArray (All); 3x QuantumTorpedo (Forward); 2x PhotonTorpedo (Aft) | 2/2/5 | 45/1/1 Large | - | data | mod-derived (see notes) | `ship.fed.luna` |
| 1031 | Federation | Spacedock | Station | Movie | 3 | 2/3/3/3/3/4 | 8480 | 15 | 104 | 379 | 1038 | 893 | 12x PhaserBank (All); 4x PhotonTorpedo (All) | 0/0/5 | 0/0/0 Station | - | data | Star Trek III: The Search for Spock | `ship.fed.spacedock` |
| 1032 | Federation | K-7 | Station | Original | 1 | 1/1/1/1/1/2 | 5010 | 9 | 104 | 346 | 932 | 107 | 4x PhaserBank (All) | 0/0/5 | 0/0/0 Station | - | data | TOS ('The Trouble with Tribbles') | `ship.fed.k7` |
| 1100 | Klingon Empire | D5 | Cruiser | Enterprise | 0 | 1/1/1/1/1/1 | 2070 | 4 | 91 | 140 | 340 | 187 | 6x Disruptor (ForwardWide); 2x Torpedo (Forward) | 2/1/3 | 90/2/0 Large | - | yes | ENT (22nd-century battle cruiser) | `ship.kli.d5` |
| 1101 | Klingon Empire | Raptor | Escort | Enterprise | 0 | 1/1/1/1/1/1 | 1110 | 2 | 50 | 77 | 187 | 87 | 3x Disruptor (Forward); 1x Torpedo (Forward) | 3/1/2 | 135/3/2 Small | - | yes | ENT ('Sleeping Dogs') | `ship.kli.raptor` |
| 1102 | Klingon Empire | Bird-of-Prey (22nd c.) | Destroyer | Enterprise | 0 | 1/1/1/1/1/1 | 1480 | 3 | 59 | 105 | 255 | 102 | 3x DisruptorCannon (Forward); 1x Torpedo (Forward) | 2/1/2 | 90/2/1 Medium | - | yes | ENT ('Affliction', 'Judgment') | `ship.kli.bop_ent` |
| 1103 | Klingon Empire | D7 | Cruiser | Original | 1 | 1/1/1/1/1/2 | 2490 | 5 | 91 | 147 | 360 | 221 | 6x Disruptor (ForwardWide); 1x PhotonTorpedo (Forward) | 2/1/3 | 90/2/0 Large | - | yes | TOS ('Elaan of Troyius' and others) | `ship.kli.d7` |
| 1104 | Klingon Empire | D12 | Destroyer | NextGeneration | 4 | 3/4/4/4/4/5 | 3870 | 7 | 64 | 126 | 316 | 527 | 4x DisruptorCannon (Forward); 2x PhotonTorpedo (Forward) | 3/1/2 | 90/2/1 Medium | yes | yes | Star Trek Generations | `ship.kli.d12` |
| 1105 | Klingon Empire | Qang | Heavy Cruiser | DominionWar | 7 | 6/7/7/7/7/8 | 11480 | 20 | 110 | 236 | 603 | 2244 | 8x DisruptorCannon (ForwardWide); 3x PhotonTorpedo (Forward) | 2/1/3 | 45/1/0 Large | yes | data | mod-derived (see notes) | `ship.kli.qang` |
| 1106 | Klingon Empire | Negh'Tev | Battleship | Nemesis | 9 | 8/9/9/9/9/10 | 22650 | 38 | 143 | 324 | 837 | 5640 | 12x DisruptorCannon (All); 5x PhotonTorpedo (ForwardWide) | 1/1/3 | 45/1/0 Huge | yes | data | mod-derived (see notes) | `ship.kli.neghtev` |
| 1200 | Romulan Star Empire | Bird-of-Prey (22nd c.) | Cruiser | Enterprise | 0 | 1/1/1/1/1/1 | 2240 | 4 | 100 | 135 | 360 | 117 | 4x Disruptor (ForwardWide); 2x SpatialTorpedo (Forward) | 2/1/3 | 90/2/0 Large | yes | yes | ENT ('Minefield', 'Babel One') | `ship.rom.bop_ent` |
| 1201 | Romulan Star Empire | Drone Ship | Destroyer | Enterprise | 1 | 1/1/1/1/1/2 | 1910 | 4 | 59 | 106 | 286 | 139 | 4x Disruptor (All) | 3/1/2 | 90/2/1 Medium | - | yes | ENT ('Babel One', 'United', 'The Aenar') | `ship.rom.drone` |
| 1202 | Romulan Star Empire | Bird-of-Prey | Strike Cruiser | Original | 1 | 1/1/1/1/1/2 | 2610 | 5 | 80 | 141 | 381 | 97 | 1x PlasmaTorpedo (Forward); 2x Disruptor (Forward) | 1/1/3 | 45/1/0 Large | yes | yes | TOS ('Balance of Terror') | `ship.rom.bop_tos` |
| 1203 | Romulan Star Empire | Scout Ship | Scout | NextGeneration | 3 | 2/3/2/2/3/2 | 560 | 1 | 28 | 46 | 127 | 56 | 1x Disruptor (Forward) | 3/2/4 | 180/3/2 Small | yes | yes | TNG ('The Defector') | `ship.rom.scout` |
| 1204 | Romulan Star Empire | Science Vessel | Science Vessel | NextGeneration | 4 | 3/4/3/3/4/3 | 1170 | 2 | 33 | 72 | 200 | 178 | 2x Disruptor (All) | 2/2/5 | 135/2/1 Small | yes | yes | TNG ('The Next Phase') | `ship.rom.science` |
| 1205 | Romulan Star Empire | Valdore | Battleship | Nemesis | 9 | 8/9/9/9/9/10 | 23600 | 40 | 143 | 313 | 887 | 5531 | 14x Disruptor (All); 4x PlasmaTorpedo (ForwardWide) | 1/1/3 | 45/1/0 Huge | yes | yes | Star Trek: Nemesis | `ship.rom.valdore` |
| 1206 | Romulan Star Empire | Norexan | Destroyer | DominionWar | 7 | 6/7/7/7/7/8 | 7620 | 13 | 64 | 136 | 383 | 1514 | 6x Disruptor (ForwardWide); 2x PlasmaTorpedo (Forward) | 2/1/2 | 90/2/1 Medium | yes | data | mod-derived (see notes) | `ship.rom.norexan` |
| 1207 | Romulan Star Empire | Mogai | Heavy Cruiser | Nemesis | 9 | 8/9/9/9/9/10 | 17570 | 30 | 110 | 243 | 693 | 3924 | 10x Disruptor (All); 3x PlasmaTorpedo (Forward) | 2/1/3 | 45/1/0 Large | yes | data | mod-derived (see notes) | `ship.rom.mogai` |
| 1300 | Cardassian Union | Hideki | Escort | DominionWar | 4 | 3/4/4/4/4/5 | 2280 | 4 | 50 | 164 | 204 | 229 | 3x Disruptor (Forward) | 3/1/2 | 135/3/2 Small | - | yes | DS9 ('The Die is Cast' and others) | `ship.car.hideki` |
| 1301 | Cardassian Union | Groumall | Transport | NextGeneration | 3 | 2/2/3/2/3/1 | 1020 | 2 | 26 | 157 | 194 | 56 | 1x Disruptor (Forward) | 1/1/1 | 45/1/0 Medium | - | yes | DS9 ('Return to Grace') | `ship.car.groumall` |
| 1302 | Cardassian Union | ATR-4107 | Special | NextGeneration | 6 | 5/6/6/6/6/7 | 10320 | 18 | 150 | 650 | 816 | 826 | 2x TricobaltDevice (Forward); 2x Disruptor (All) | 3/2/4 | 90/2/1 Large | - | data | VOY ('Dreadnought') | `ship.car.atr4107` |
| 1303 | Cardassian Union | Nor | Station | NextGeneration | 4 | 3/4/4/4/4/5 | 9810 | 17 | 104 | 660 | 818 | 1283 | 10x Disruptor (All); 4x PhotonTorpedo (All) | 0/0/5 | 0/0/0 Station | - | data | DS9 (Terok Nor / Deep Space 9) | `ship.car.nor` |
| 1304 | Cardassian Union | Kraxon | Light Cruiser | DominionWar | 6 | 5/6/6/6/6/7 | 6060 | 11 | 85 | 292 | 367 | 1116 | 6x Disruptor (ForwardWide); 2x PhotonTorpedo (Forward) | 2/1/3 | 90/2/1 Medium | - | data | mod-derived (see notes) | `ship.car.kraxon` |
| 1400 | Ferengi Alliance | Pod Ship | Raider | Enterprise | 0 | 1/1/1/1/1/1 | 1190 | 2 | 54 | 72 | 228 | 25 | 2x PhaseCannon (Forward) | 3/1/2 | 135/3/2 Small | - | yes | ENT ('Acquisition') | `ship.fer.pod` |
| 1401 | Ferengi Alliance | Nagus | Battleship | Nemesis | 9 | 8/9/9/9/9/10 | 20010 | 34 | 130 | 278 | 936 | 4332 | 10x PhaserArray (All); 4x PhotonTorpedo (ForwardWide) | 1/1/3 | 45/1/0 Huge | - | data | mod-derived (see notes) | `ship.fer.nagus` |
| 1500 | Dominion | Jem'Hadar Attack Ship | Destroyer | DominionWar | 6 | 5/6/6/6/6/7 | 5600 | 10 | 59 | 175 | 408 | 1073 | 5x PolaronBeam (ForwardWide); 1x Torpedo (Forward) | 3/1/2 | 90/2/1 Medium | - | data | DS9 ('The Jem'Hadar') | `ship.dom.attack_ship` |
| 1501 | Dominion | Jem'Hadar Battle Cruiser | Battlecruiser | DominionWar | 7 | 6/7/7/7/7/8 | 13150 | 22 | 115 | 328 | 766 | 3066 | 10x PolaronBeam (All); 3x Torpedo (Forward) | 2/1/3 | 45/2/0 Large | - | data | DS9 ('Call to Arms', 'Sacrifice of Angels') | `ship.dom.battlecruiser` |
| 1502 | Dominion | Jem'Hadar Battleship | Dreadnought | DominionWar | 8 | 7/8/8/8/8/9 | 23140 | 39 | 140 | 478 | 1124 | 6030 | 16x PolaronBeam (All); 4x Torpedo (All) | 1/1/3 | 45/1/0 Huge | - | data | DS9 ('The Changing Face of Evil') | `ship.dom.battleship` |
| 1503 | Breen Confederacy | Breen Warship | Cruiser | DominionWar | 7 | 6/7/7/7/7/8 | 8080 | 14 | 91 | 229 | 539 | 1441 | 1x EnergyDampener (Forward); 6x Disruptor (ForwardWide); 2x Torpedo (Forward) | 2/1/3 | 90/2/0 Large | - | data | DS9 ('The Changing Face of Evil') | `ship.breen.breen_warship` |
| 1600 | Borg Collective | Sphere | Cruiser | NextGeneration | 7 | 6/7/7/7/7/8 | 8520 | 15 | 91 | 607 | 994 | 1660 | 3x CuttingBeam (All); 2x Torpedo (All) | 2/1/3 | 180/2/2 Large | - | data | Star Trek: First Contact; VOY | `ship.borg.sphere` |
| 1601 | Borg Collective | Probe | Scout | NextGeneration | 6 | 5/6/5/5/6/5 | 1040 | 2 | 26 | 175 | 285 | 348 | 1x CuttingBeam (All) | 3/2/5 | 180/3/2 Small | - | data | VOY ('Drone', 'Infinite Regress') | `ship.borg.probe` |
| 1602 | Borg Collective | Tactical Cube | Dreadnought | Nemesis | 10 | 9/10/10/10/10/10 | 39220 | 66 | 140 | 1282 | 2128 | 12772 | 14x CuttingBeam (All); 6x Torpedo (All) | 1/1/3 | 180/1/2 Huge | - | data | VOY ('Unimatrix Zero') | `ship.borg.tactical_cube` |
| 1603 | Borg Collective | Queen's Diamond | Special | Nemesis | 10 | 9/10/10/10/10/10 | 29350 | 49 | 150 | 1350 | 2240 | 7475 | 8x CuttingBeam (All); 4x Torpedo (All) | 2/2/4 | 180/2/2 Large | - | data | VOY ('Endgame') | `ship.borg.diamond` |
| 1604 | Borg Collective | Scout Vessel | Scout | NextGeneration | 5 | 4/5/4/4/5/4 | 830 | 2 | 26 | 168 | 273 | 225 | 1x CuttingBeam (Forward) | 3/2/4 | 180/3/2 Small | - | data | TNG ('I, Borg') | `ship.borg.scout` |
| 1700 | Tholian Assembly | Web Spinner | Destroyer | Original | 2 | 1/2/2/2/2/3 | 2200 | 4 | 59 | 106 | 302 | 126 | 2x TholianWeb (All); 2x PhaserBank (Forward) | 2/1/2 | 135/2/1 Medium | - | data | TOS ('The Tholian Web'); ENT | `ship.thol.web_spinner` |
| 1701 | Gorn Hegemony | Gorn Cruiser | Cruiser | Original | 1 | 1/1/1/1/1/2 | 2550 | 5 | 91 | 210 | 318 | 185 | 5x Disruptor (ForwardWide); 1x Torpedo (Forward) | 2/1/3 | 90/2/0 Large | - | data | TOS ('Arena') | `ship.gorn.cruiser` |
| 1702 | Orion Syndicate | Orion Interceptor | Raider | Enterprise | 0 | 1/1/1/1/1/1 | 1170 | 2 | 54 | 66 | 168 | 45 | 3x PlasmaCannon (Forward) | 3/1/2 | 135/3/2 Small | - | data | ENT ('Bound'); TOS ('Journey to Babel') | `ship.orion.interceptor` |
| 1703 | Hirogen | Hunting Vessel | Destroyer | DominionWar | 6 | 5/6/6/6/6/7 | 5310 | 9 | 59 | 184 | 346 | 928 | 4x DisruptorCannon (ForwardWide); 2x Torpedo (Forward) | 3/1/2 | 90/2/1 Medium | - | data | VOY ('Message in a Bottle', 'Hunters') | `ship.hiro.hunter` |
| 1704 | Hirogen | Hirogen Warship | Heavy Cruiser | DominionWar | 7 | 6/7/7/7/7/8 | 11210 | 19 | 100 | 319 | 603 | 2354 | 8x DisruptorCannon (All); 3x Torpedo (Forward) | 2/1/3 | 45/1/0 Large | - | data | VOY ('The Killing Game') | `ship.hiro.warship` |
| 1705 | Kazon sects | Kazon Raider | Raider | NextGeneration | 3 | 2/3/3/3/3/4 | 1940 | 4 | 54 | 103 | 184 | 168 | 3x Disruptor (Forward) | 3/1/2 | 135/3/2 Small | - | data | VOY ('Caretaker' and others) | `ship.kazon.raider` |
| 1706 | Kazon sects | Predator | Carrier | NextGeneration | 4 | 3/4/4/4/4/5 | 6320 | 11 | 120 | 270 | 483 | 867 | 8x Disruptor (All); 2x Torpedo (Forward) | 1/1/3 | 45/1/0 Huge | - | data | VOY ('Basics') | `ship.kazon.carrier` |
| 1707 | Vidiian Sodality | Vidiian Warship | Cruiser | NextGeneration | 5 | 4/5/5/5/5/6 | 5510 | 10 | 91 | 175 | 416 | 731 | 5x Disruptor (ForwardWide); 2x Torpedo (Forward) | 2/1/3 | 90/2/0 Large | - | data | VOY ('Phage', 'Deadlock') | `ship.vid.warship` |
| 1708 | Suliban Cabal | Cell Ship | Escort | Enterprise | 0 | 1/1/1/1/1/1 | 1050 | 2 | 50 | 49 | 143 | 30 | 2x PlasmaCannon (Forward) | 3/1/2 | 135/3/2 Small | - | data | ENT ('Broken Bow' and others) | `ship.sul.cell_ship` |
| 1709 | Xindi Council | Reptilian Warship | Cruiser | Enterprise | 1 | 1/1/1/1/1/2 | 2500 | 5 | 91 | 178 | 339 | 159 | 5x PlasmaCannon (ForwardWide); 2x Torpedo (Forward) | 2/1/3 | 90/2/0 Large | - | data | ENT (season 3) | `ship.xindi.reptilian` |
| 1710 | Xindi Council | Insectoid Warship | Destroyer | Enterprise | 1 | 1/1/1/1/1/2 | 1790 | 3 | 59 | 133 | 254 | 78 | 4x PlasmaCannon (Forward) | 3/1/2 | 90/2/1 Medium | - | data | ENT (season 3) | `ship.xindi.insectoid` |
| 1711 | Xindi Council | Aquatic Cruiser | Battleship | Enterprise | 1 | 1/1/1/1/1/2 | 3960 | 7 | 130 | 285 | 542 | 237 | 8x PlasmaCannon (All); 2x Torpedo (Forward) | 1/1/3 | 45/1/0 Huge | - | data | ENT (season 3) | `ship.xindi.aquatic` |
| 1712 | Xindi Council | Primate Ship | Frigate | Enterprise | 1 | 1/1/1/1/1/2 | 1580 | 3 | 55 | 115 | 220 | 84 | 3x PlasmaCannon (Forward); 1x Torpedo (Forward) | 2/1/2 | 90/2/1 Medium | - | data | ENT (season 3) | `ship.xindi.primate` |
| 1713 | Xindi Council | Xindi Weapon | Special | Enterprise | 3 | 2/3/3/3/3/4 | 6580 | 11 | 150 | 391 | 755 | 250 | 1x TemporalWeapon (Forward) | 1/2/4 | 90/2/1 Large | - | data | ENT ('Zero Hour') | `ship.xindi.weapon` |
| 1714 | Species 8472 | Bioship | Cruiser | DominionWar | 9 | 8/9/9/9/9/10 | 9770 | 17 | 91 | 551 | 924 | 1635 | 2x BioEnergy (Forward) | 3/1/3 | 135/2/1 Large | - | data | VOY ('Scorpion') | `ship.s8472.bioship` |
| 1715 | Krenim Imperium | Temporal Weapon Ship | Special | DominionWar | 9 | 8/9/9/9/9/10 | 15180 | 26 | 150 | 464 | 1047 | 1090 | 1x TemporalWeapon (Forward) | 1/2/4 | 90/2/1 Large | - | data | VOY ('Year of Hell') | `ship.krenim.timeship` |
| 1716 | Krenim Imperium | Krenim Warship | Destroyer | DominionWar | 5 | 4/5/5/5/5/6 | 4210 | 8 | 59 | 150 | 331 | 585 | 4x Disruptor (Forward); 2x Torpedo (Forward) | 2/1/2 | 90/2/1 Medium | - | data | VOY ('Before and After', 'Year of Hell') | `ship.krenim.warship` |
| 1717 | Malon | Export Vessel | Transport | DominionWar | 5 | 4/4/5/4/5/1 | 1620 | 3 | 26 | 137 | 157 | 236 | 2x Disruptor (All) | 1/1/1 | 45/1/0 Medium | - | data | VOY ('Night', 'Juggernaut') | `ship.malon.freighter` |
| 1718 | Son'a | Son'a Battleship | Battleship | Nemesis | 8 | 7/8/8/8/8/9 | 15810 | 27 | 130 | 358 | 899 | 3240 | 10x PhaserArray (All); 3x Torpedo (Forward) | 1/1/3 | 45/1/0 Huge | - | data | Star Trek: Insurrection | `ship.sona.battleship` |
| 1719 | Maquis | Maquis Raider | Raider | DominionWar | 4 | 3/4/4/4/4/5 | 2260 | 4 | 54 | 72 | 163 | 195 | 2x PhaserBank (Forward); 1x PhotonTorpedo (Forward) | 3/1/2 | 135/3/2 Small | - | data | DS9 ('The Maquis'); VOY | `ship.maquis.raider` |
| 1720 | Reman | Scimitar | Dreadnought | Nemesis | 10 | 9/10/10/10/10/10 | 41220 | 69 | 154 | 741 | 1459 | 11895 | 26x Disruptor (All); 6x PlasmaTorpedo (All) | 1/1/3 | 45/1/0 Huge | yes | data | Star Trek: Nemesis | `ship.reman.scimitar` |
| 1721 | Devore Imperium | Devore Warship | Cruiser | DominionWar | 6 | 5/6/6/6/6/7 | 6930 | 12 | 91 | 195 | 462 | 1160 | 6x Disruptor (All); 2x Torpedo (Forward) | 2/1/3 | 90/2/0 Large | - | data | VOY ('Counterpoint') | `ship.devore.warship` |
| 1800 | Andorians | Kumari | Cruiser | Enterprise | 1 | 1/1/1/1/1/2 | 2450 | 5 | 91 | 157 | 318 | 136 | 4x PlasmaCannon (ForwardWide); 2x Torpedo (Forward) | 2/1/3 | 90/2/0 Large | - | data | ENT ('Proving Ground', Kumari) | `ship.and.kumari` |
| 1801 | Vulcans | D'Kyr | Cruiser | Enterprise | 1 | 1/1/1/1/1/2 | 2410 | 5 | 91 | 157 | 381 | 117 | 4x PlasmaCannon (All); 1x Torpedo (Forward) | 2/1/4 | 90/2/0 Large | - | data | ENT ('Fusion' and others) | `ship.vul.dkyr` |
| 1802 | Vulcans | Suurok | Science Vessel | Enterprise | 1 | 1/1/1/1/1/1 | 590 | 1 | 30 | 70 | 171 | 45 | 2x PlasmaCannon (All) | 2/2/5 | 135/2/1 Small | - | data | ENT ('Breaking the Ice') | `ship.vul.suurok` |
| 1803 | Bajorans | Bajoran Assault Vessel | Destroyer | DominionWar | 4 | 3/4/4/4/4/5 | 2840 | 5 | 59 | 98 | 260 | 255 | 3x PhaserBank (Forward); 1x PhotonTorpedo (Forward) | 2/1/2 | 90/2/1 Medium | - | data | DS9 (Bajoran Militia) | `ship.baj.assault` |
| 1804 | Bajorans | Lightship | Scout | Movie | 1 | 1/1/1/1/1/1 | 290 | 1 | 26 | 34 | 89 | 0 | none | 1/2/4 | 180/3/2 Small | - | data | DS9 ('Explorers') | `ship.baj.lightship` |
| 1805 | Nausicaans | Nausicaan Raider | Raider | Enterprise | 1 | 1/1/1/1/1/2 | 1280 | 3 | 54 | 75 | 152 | 58 | 3x PlasmaCannon (Forward) | 3/1/2 | 135/3/2 Small | - | data | ENT ('Fortunate Son') | `ship.nau.raider` |
| 1806 | Talarians | Observation Craft | Scout | NextGeneration | 3 | 2/3/2/2/3/2 | 460 | 1 | 26 | 48 | 106 | 56 | 1x Disruptor (Forward) | 3/2/4 | 180/3/2 Small | - | data | TNG ('Suddenly Human') | `ship.tal.observer` |
| 1900 | Federation | Kelvin | Explorer | Kelvin | 1 | 1/1/1/1/1/2 | 3620 | 7 | 115 | 236 | 636 | 175 | 6x PhaseCannon (All); 2x PhotonTorpedo (Forward) | 2/2/4 | 45/1/1 Large | - | data | Kelvin: Star Trek (2009) | `ship.fed.kelvin_type` |
| 1901 | Federation | Constitution (Kelvin) | Heavy Cruiser | Kelvin | 3 | 2/3/3/3/3/4 | 5090 | 9 | 100 | 215 | 590 | 643 | 8x PhaserBank (All); 4x PhotonTorpedo (Forward) | 2/1/3 | 45/1/0 Large | - | data | Kelvin: Star Trek (2009); Into Darkness; Beyond | `ship.fed.constitution_kelvin` |
| 1902 | Federation | Dreadnought (Vengeance) | Dreadnought | Kelvin | 6 | 5/6/6/6/6/7 | 13370 | 23 | 140 | 370 | 1033 | 2305 | 12x PhaserBank (All); 6x PhotonTorpedo (ForwardWide) | 3/1/3 | 45/1/0 Huge | - | data | Kelvin: Star Trek Into Darkness | `ship.fed.vengeance` |
| 1903 | Klingon Empire | D4 | Destroyer | Kelvin | 3 | 2/3/3/3/3/4 | 2800 | 5 | 59 | 120 | 300 | 331 | 5x Disruptor (Forward); 1x Torpedo (Forward) | 3/1/2 | 90/2/1 Medium | - | data | Kelvin: Star Trek Into Darkness | `ship.kli.d4` |
| 1904 | Romulan Star Empire | Narada | Special | Kelvin | 8 | 7/8/8/8/8/9 | 16840 | 29 | 150 | 378 | 1065 | 2587 | 12x Torpedo (All) | 2/2/4 | 90/2/1 Large | - | data | Kelvin: Star Trek (2009) | `ship.rom.narada` |

| Faction | Classes | Buildable in Remastered |
|---|---|---|
| Federation | 36 | 29 |
| Klingon Empire | 8 | 5 |
| Romulan Star Empire | 9 | 6 |
| Cardassian Union | 5 | 2 |
| Ferengi Alliance | 2 | 1 |
| Dominion | 3 | 0 |
| Breen Confederacy | 1 | 0 |
| Borg Collective | 5 | 0 |
| Tholian Assembly | 1 | 0 |
| Gorn Hegemony | 1 | 0 |
| Orion Syndicate | 1 | 0 |
| Hirogen | 2 | 0 |
| Kazon sects | 2 | 0 |
| Vidiian Sodality | 1 | 0 |
| Suliban Cabal | 1 | 0 |
| Xindi Council | 5 | 0 |
| Species 8472 | 1 | 0 |
| Krenim Imperium | 2 | 0 |
| Malon | 1 | 0 |
| Son'a | 1 | 0 |
| Maquis | 1 | 0 |
| Reman | 1 | 0 |
| Devore Imperium | 1 | 0 |
| Andorians | 1 | 0 |
| Vulcans | 2 | 0 |
| Bajorans | 2 | 0 |
| Nausicaans | 1 | 0 |
| Talarians | 1 | 0 |
| **Total** | **98** | **43** |
<!-- ROSTER-TABLE:END -->
