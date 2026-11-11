# MOD-2: empire-slot generalisation (two to eight majors)

Lane MOD-2 (Barclay, 12 Nov 2026). Outranked by `AI_stuff\standing\simon\botf-rulings.md` (11 Nov: every vanilla feature plus the five mod packs as
new-game presets; the Dominion and the Borg are "Playable too"). Sources: `research/MOD-PARITY.md` rows E01, E02, E04 and section 3's note on
fixed-width arrays. Code: `game/Source/BotfSim/EmpireSlots.h` (new), `Content.h`, `Simulation.cpp`, `web/bridge/WebBridge.cpp`, `godot/`.

## 1. Where five majors were assumed

| Area | Assumption found (before) | Now |
|---|---|---|
| Core, ids | `empire id 1..5` hard-coded in about 45 `require`/guard lines of `Simulation.cpp` (command targets, colony owners, fleets, task forces, relations, battle owners, winner), `Chronicle.cpp`, `EmpireRules.h`, `TacticalCombat.h` (four save validators) | one constant `MaxMajors = 8`; ids are race slots |
| Core, names and homes | `empireName()` and the capitals were two literal arrays of five (`Simulation.cpp:3635`, `:4276`) | read from the `major` block of the race records |
| Core, ship ownership | `catalogueRace()` was the literal map `{-1,1,3,4,0,2}`; `playableShip()` accepted race ids 0..4 only | race table lookup; any race with an empire slot is a major |
| Core, `Minor` | `contact`, `attitude`, `friendshipTurn` were `array<_, 6>` (`Simulation.h:309`); save and load looped to 6 | nine slots (0 unused, 1..8 majors) |
| Core, minors | `MinorRaceRecord::affinity` six values; roster loops `for e <= 5` (`MinorRaces.h`) | nine, data gives slots 0..5 and the rest load as 0 |
| Core, galaxy | `Colony` homes named from `Homes[5]`, generator seated ids `1..empires` (`Simulation.cpp:4120`, relations loop `:4157`) | seats a lineup of ids; temporary seats 1..N keep every random draw, then map to slots |
| Core, features | `GalaxyFeature::surveyedMask` bit per empire, validator `< 64` | `< 512` (bit `1 << id`, ids to 8) |
| Diplomacy | `MajorPersonality` enum 0..4, `personalityForEmpire` (`id - 1`, 1..5), thresholds, profile, defence-call and affinity tables, the engine save's `validMajor` | enum 0..7 (Dominion, Borg, Breen added with profiles), affinity pairs, `validMajor 1..8` |
| Intelligence | `IntelDoctrine` 0..4; budget allows at most 4 rivals (`isValid`, `setDefault`, save `rivalCount`, `extraRivals`) | doctrine 0..7; at most 7 rivals |
| AI | planners iterate `s.rivals` (already count-free); personality and doctrine come from the id | no change beyond the tables above |
| Trade, victory | routes and victory read `s.rivals` and ownership sets | no change (checked) |
| Tactical combat | cloak for legacy ships by `ownerId == 2 or 3`; encounter owner bounds `1..5` in the save validators | bounds 1..8; cloak rule unchanged (roster classes carry their own flag) |
| Save | `RIVALS` count <= 4, `MINORS` six relation triples per minor, no lineup | `EMPIRE_SLOTS` section and nine triples, only when a slot above five is in play |
| Bridge / wasm API | `catalogueRaceOf` literal map, `meta.empires` loop 1..5 with a `Homes` array, `galaxyPreview` kind 6 = minor, `formerOwner <= 5`, `newCampaign` took only `empires` | `meta.empires` lists the 8 slots (key, capital, traits, donor); preview kind 9 = minor; `newCampaign` takes `lineup` (race keys) |
| Godot client | `Lcars.PALETTES`/`COLORBLIND`/`FACTIONS` ids 1..5, the new-game power picker ("Powers in play" 2..5, canonical order), `title_vm` and `app.gd` token maps, `database_vm` race maps, `galaxy_preview` kinds, screenshot loop `id <= 5` | tables extended to 8, lineup picker, stand-ins (section 5) |
| Fixtures | `godot/fixtures/*.json` carry `saveVersion 16`/`contentVersion` and five-empire meta | regenerated |

## 2. The model

* **Empire id = race slot.** 1 Federation, 2 Klingon, 3 Romulan, 4 Cardassian, 5 Ferengi (unchanged), 6 Dominion, 7 Borg, 8 Breen. The id is the stable race
  key's index, so every id-keyed table (relations, treaties, intel, diplomacy engine, saves) keeps meaning. Slot 8 is Breen so a full eight-major game
  exists; its content is a stand-in (one roster ship, generic profile).
* **Race records are data.** Races 0..4 gain an optional `major` block in `data/base/races.json`; new records 35 Dominion, 36 Borg, 37 Breen:
  `{empireId, key, empireName, capital, shipDonor, traits[], startingFleet[]}`. `EmpireSlots.h` reads them (`slots::name`, `capital`, `key`, `shipDonor`).
  A pack must define slots 1..5. `tools/content/build_pack.py` accepts the optional key.
* **Lineup.** `CampaignSetup::lineup` is a list of race keys (two to eight, no repeats); `newCampaign` seats them in order: the first takes the origin
  star, the others are placed farthest-first, exactly as before. `playerEmpire` is an empire id and must be seated. An empty lineup means slots
  `1..empires`, byte-for-byte the old behaviour. `generateCampaign(..., lineup)` takes the ids. The MOD-1 preset field can fill `lineup` directly.
* **Ships.** Roster ids 1500 to 1503 (Dominion, Breen) and 1600 to 1604 minus the Queen's Diamond (Borg) become `buildable`; their `raceId` is now 35, 36,
  37 (the "outside the catalogue" marker moved to 99). Because the legacy catalogue has no Dominion, Borg or Breen scouts or colony ships, each race names a
  `shipDonor` (Dominion: Cardassian, Borg: Federation, Breen: Romulan). `shipUsable(ship, owner)` is true for the own race and the donor; AI and start-fleet
  picks take own hulls first and donor hulls only when no own hull is eligible. `startingFleet` ships (Dominion: Attack Ship; Borg: Scout Vessel, Probe; Breen:
  Warship) are added by `addStartingFleets` after the Remastered preset (they need the expanded roster).
* **Widened arrays.** `Minor` relations hold nine slots (`MajorSlots`). The diplomacy engine mirror (`std::map`) already had no width.
* **Generic rules only.** The new races use the generic economy, research, combat, diplomacy and intelligence. Personality, doctrine and affinity entries for
  them are first-pass numbers (*design*, Simon can overturn). Dominion structures, Shapeshifter Network, Borg assimilation belong to MOD-3 and MOD-5.

## 3. Save migration (schema 17)

* Header `BOTF_SIM 17 remaster-17`. Schemas 14, 15 and 16 load: they are schema-17 text without the new section, and the loader rebuilds the exact
  canonical text for the old header before accepting (the same trick as 14 to 16). Schemas 4 to 13 keep their older chain.
* A campaign whose empires are all `<= 5` writes **exactly the schema-16 layout** (six relation triples per minor, no section): a default five-empire save
  differs from the main-branch save only in the header line. That is also what keeps old saves loadable without a conversion step.
* A campaign with any empire above 5 writes `EMPIRE_SLOTS 1 <n> <ids ascending>` directly before `MINORS` and nine triples per minor. The loader reads the
  section if present, refuses it under a 14, 15 or 16 header, refuses a lineup that holds only ids `<= 5`, and checks the ids against the empires.
* Self-contained: renumbering at merge means `SaveVersion`, `ContentVersion`, the `legacy16`/`current17` flags and the header strings in three tests.

## 4. Tests and proof

New suite `BotfEmpireSlotsTests` (`tests/scenarios/EmpireSlotsTests.cpp`): race table and lineup parsing; seating, capitals and relations; Dominion, Borg and Breen
start, build, research, talk and fight; 50-turn AI soaks with 2, 5, 6 and 8 majors (determinism between two runs, save round trip every 10 turns, reload at turn 25);
default-save layout; migration from 14, 15 and 16; forged sections refused. Default-game proof: `BotfSim` output (per-turn state hashes and `FINAL_HASH`) of this branch
against main `6e8d24a` for the same seeds, header excluded.

## 5. Client stand-ins (present systems do not cover them)

Present and wired: insignia for Dominion, Borg and Breen (`insignia.gd` had the marks; the id table now reaches them). New rows: owner colours (normal and colour-blind),
capital, motto and traits for slots 6, 7, 8 in `Lcars.PALETTES`/`COLORBLIND`/`FACTIONS`. Stand-ins where no asset exists: leader portraits (the empire's insignia on a
colour gradient, `PortraitSlot` falls back), the title-screen films and crest (a campaign as slot 6 to 8 films a random original power), per-empire audio packs (none, the
app skips the fetch), and the trek-text voice (Dominion has its Vorta voice; Borg and Breen use the Federation's). Tactical ship models for Jem'Hadar, Borg and Breen come
from the existing roster asset keys. The browser (TypeScript) client keeps its five-power picker; only its bridge test follows the new `meta.empires`.

## 6. Risks

* **Hidden slots.** Slots 6..8 of `Minor` relation arrays hold defaults in a five-empire game and are not saved; any future code that writes them for an absent
  empire would break the round-trip equality. The loops that fill them are guarded by "slot in the campaign" (`mirrorsSlot`, `campaignEmpire`).
* **Stand-in hulls.** A donor hull is a visible oddity (a Borg player may queue Federation escorts). It is a temporary rule; MOD-3 and MOD-5 supply own utility ships.
* **Balance.** Eight majors on a 50-system map crowd the galaxy; nothing here tunes economy or AI for it.
* **Merge.** MOD-1 also bumps the schema and edits `CampaignSetup`; both edit `Content.h`/`races.json`. The migration block is self-contained and the
  `lineup` field is plain data.
* **Content hash.** `base.pack.json` and the embedded header changed (races, roster); non-base packs made on the old base need the five `major` blocks.
