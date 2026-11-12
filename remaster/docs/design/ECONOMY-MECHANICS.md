# MOD-6: economy and rules mechanics

Lane MOD-6 (Barclay, 12 Nov 2026). Outranked by `AI_stuff\standing\simon\botf-rulings.md` (11 Nov: every vanilla feature plus the popular packs as new-game presets; 12 Nov: two lanes at a time). Sources: `research/MOD-PARITY.md` rows E10, N06, X01, X02, X08, X09, X18, X19; `research/PARITY-MATRIX.md` (strategic intercept and raid order rows); `EMPIRE-RULES.md`; `DOMINION.md`. Private; not for distribution. Code: `game/Source/BotfSim/EconomyRules.h`, `StructureUpgrades.h`, `Crew.h` (new, header-only), hooks in `Simulation.cpp`, `Rules.cpp`, `EmpireRules.h`, `Structures.h`, `TacticalCombat.h`. Data: `data/overlays/economy.overlay.json`, the preset files.

**Honesty note.** Numbers below are our design values (marked *design*), not read from the 1999 executable or from the mods' binaries; Simon can overturn any of them. Where a mod's exact rule is unknown the text says what we did instead.

## 1. What was already closed, and what this lane closes

| Row | Status before | This lane |
|---|---|---|
| N06 dilithium cap | The construction cap (ship slots = powered refineries) closed in MOD "M2-EMPIRE" (EMPIRE-RULES section b). "Every minor has dilithium" was not true: deposits were a 45 percent hash. | **Closed**: `allDilithium` flag, every minor homeworld has a deposit. |
| X02 Protectorate HQ | The Liberate / Subjugate / Exterminate choice was closed in M2-EMPIRE (section c). No Protectorate. | **Closed**: Subjugate marks the world; a Protectorate HQ structure changes what that means. |
| E10 / X08 upgrades | Not present (`ScrapStructure` and `UpgradeStation` only). | **Closed**: tiered upgrades as build-list projects, an automatic cheaper variant, a no-upgrades variant. |
| X09 exploits | `ScrapStructure` already refunds nothing and has no regression test. | **Closed**: tests pin no refund, scrap takes its share of upgrade steps, a queued upgrade cannot outlive its units; terraform capped. |
| X01 shared morale | Per-colony morale only. | **Closed**: a shared morale structure lifts every colony of its empire. |
| X18 raid and intercept | No orders; AI hunted by attack target only. | **Closed**: both orders for player and AI; Orion and Maquis raid bonus. |
| X19 crew experience | Tactical crew rating was a hash of the fleet id; academies were data. | **Closed**: experience on `Fleet`, academies and battles feed it, it sets the crew rating and shows in the ship systems panel. |
| E10 reorganised buildings in general | The Dominion ladder (MOD-3) already covers race structures. | Left to MOD-7 (full per-mod building tables). |

## 2. Rule flags and presets (item 7)

`GameplayRules` gains eleven flags, saved as one bit set (section `ECONOMY_RULES`). All start off. The Remastered *preset* (`data/presets/remastered.preset.json`) turns on the vanilla-order set (`interceptOrders`, `raidOrders`, `crewExperience`, `structureUpgrades`, `exploitFixes`); `rulesForPreset` itself leaves every flag off so the CLI, the tests and old saves are untouched; **the Classic preset keeps every flag off, so the default Classic five-empire game plays exactly as before** (see section 9). The mod-only flags come from a preset's `rules.flags`:

| Preset | Flags added to its base | Overlay |
|---|---|---|
| Classic | none | none |
| Remastered | the vanilla set (from the preset file) | none |
| Dominion War (UDM) | vanilla set, `sharedMorale`, `protectorateHq`, `allDilithium`, `raidBonus` | `dominion`, `dominion-war`, `economy` |
| Galaxies | vanilla set | `galaxies` |
| Ages | vanilla set | `ages` |
| Balance of Power | vanilla set, `autoUpgrades` (classic base) | `balance-of-power` |
| Ultimate (UM5) | vanilla set, `autoUpgrades` | `ultimate` |

`noUpgrades` (the "no upgrades" variant) is on no shipped preset; a user preset sets it. `Presets.h` validates at load: `noUpgrades` excludes `autoUpgrades`, `raidBonus` needs `raidOrders`.

## 3. Structure upgrades (E10, X08, X09)

* Three kinds: farms (Biotech), factories (Construction), laboratories (Computers). Tier *n* needs the technology at level 2n, to a maximum of tier 3 (*design*).
* An upgrade is a build-list project (`Building::Structure`, reserved record ids from 900000, see `StructureUpgrades.h`): it takes industry like any project and is paid for before it acts. Manual cost is 60 percent of the unit's own cost, the automatic variant 40 percent (*design*). One project may be queued per open step (units x available tier minus steps taken).
* Effect: `colony.upgradeSteps[kind]` counts steps; the kind's output gains `20 x steps / units` percent (all units at tier *t* give +20t percent). Applied in `economy()`.
* Automatic (`autoUpgrades`, BOP and UM5): at the end of a turn a colony with an empty queue queues the automatic upgrade for its first open step. `noUpgrades`: nothing can be queued; an existing project reports the blocker.
* AI: with manual upgrades on it queues an upgrade when its other rules have nothing to build and the treasury holds 120 credits.
* **Exploit fixes (`exploitFixes`).** Scrapping a unit refunds nothing (it never did) and now takes its share of steps (`steps x (units-1) / units`), so no unit keeps a step the others paid for; a project whose last unit was scrapped finishes with no effect; terraforming adds at most `min(300, ceil(needed / 2))` work to a planet per turn however many colony ships work it, so it never completes in one turn (for planets needing two work or more) and ships do not stack. Tests: `EconomyRulesTests.cpp`.

## 4. Shared morale and Protectorate HQ (X01, X02)

The `economy` overlay patches three vanilla records that every race may use (ids 85, 73, 61 keep their ids so the patch is stack-independent) and makes them buildable at prototype cost: **Fleet Academy** (85, bonus type 92), **Unity Monument** (73, type 93, +5 morale in every colony of the empire, one per empire) and **Protectorate Headquarters** (61, type 94, one per empire, buildable only in a subjugated world). A record is refused while its rule flag is off.

* Subjugate (the existing aftermath choice, and its three-turn lapse default) marks the colony `subjugated` under `protectorateHq`; any change of owner clears it. Without an HQ a subjugated world's morale target is 10 lower. With a Protectorate HQ standing, the penalty becomes +10 morale and each subjugated world pays `population / 8` credits a turn as tribute (through `economy()`, so forecasts and ledgers agree).
* The AI builds the structures through the existing `structures::aiNext` rule; it subjugates by its existing aftermath rule.

## 5. Dilithium on every minor (N06)

`allDilithium`: `hasDeposit()` is true at every minor race's homeworld, so a conqueror or member can always build a refinery there.

## 6. Intercept and raid orders (X18)

`SetFleetOrder` (a gameplay command: `fleetId`, `eventOption` 0 stand down / 1 intercept / 2 raid, `systemId` the target colony of a raid). Only armed warships outside a task force; any move, attack or retreat order ends a standing order.

* **Intercept.** The fleet holds the sector it was in as its post. At the start of each movement phase it goes after the nearest visible hostile ship within `1 + speed` sectors (at most 4) of the post, and returns when none is. Orders end when the ship is below half hull.
* **Raid.** The fleet flies to the enemy colony and, once there after the turn's combat and with no powered batteries, no armed defender and no station in the sector, plunders `population / 4 + 3 x factories + 5` credits plus half the revenue of the victim's trade routes touching the colony. The purse moves (never more than the victim holds), the colony loses 3 morale a turn. Defended colonies repel the raid.
* **Orion and Maquis bonus (`raidBonus`).** An Orion or Maquis hull raids +50 percent, and each Orion or Maquis minor that is a member adds +25 percent.
* **AI.** At war, a third of its warships (fleet id divisible by 3) take intercept posts at home colonies and a third (id remainder 1, and every Orion or Maquis hull) raid the nearest charted enemy colony without powered batteries. Fleets holding an order are skipped by the older AI movement.

## 7. Crew experience and academies (X19)

`Fleet.experience` (25 = a fresh Regular crew, 0 to 100). Ratings: Green under 15, Regular under 40, Veteran under 65, Elite under 90, Legendary (`Crew.h`). Gains: +4 for surviving a battle, +4 more for winning it, +2 for a successful raid, academy training (a Fleet Academy gives its system's fleets +4 a turn up to 80). **The training fix (our reading; the mod's own bug is not documented):** new ships launch from a colony with an academy already trained (`+16`), and training needs no order and applies to every fleet in the sector, in a task force or not.

Effect: under WEGO rules the ship's crew rating (accuracy, repair, restore) is the fleet's rating instead of a hash of its id; under rules 2 and 3 attack changes by -10, 0, +5, +10, +15 percent. The ship systems panel's crew badge shows the rating and experience points.

## 8. Save schema 23

`BOTF_SIM 23 remaster-23`. Optional `ECONOMY_RULES 1 <flag bits>` after STRUCTURES and before THREATS, written only when a flag is on: `COLONIES n` rows `systemId farmSteps factorySteps labSteps subjugated`, `FLEETS n` rows `fleetId order postX postY raidSystemId experience`, only for non-default colonies and fleets. Schemas 14 to 22 load (21 and 22 differ from 23 only by the header). The section is refused under an older header.

## 9. Default Classic game

Classic has every new flag off, so no section is written and no rule differs: only the header changes. Nothing in this lane restores a vanilla rule in Classic (the intercept and raid orders and crew experience are vanilla features, but turning them on would change the Classic AI game; they are on in Remastered and every mod preset instead). Putting them on in Classic is a one-line change to `rulesForPreset` and Simon's call.

## 10. Not done

* The structure ladder of each mod's own building tables (MOD-7); the per-race academies of vanilla (one generic Fleet Academy stands in).
* No trade-route interdiction beyond plunder; no raid on stations; raid ships take no damage from a repelled raid.
* Tribute and morale are balanced by hand, not tuned in long AI soaks.

The academy, Unity Monument and Protectorate HQ records ship in the `economy` overlay, stacked only by the Dominion War (UDM) preset; the other mods get crew experience from combat and raids only (a later lane can stack the overlay on them).
