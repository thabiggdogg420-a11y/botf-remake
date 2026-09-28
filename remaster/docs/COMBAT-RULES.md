# Remaster combat rules (save schema 14, tactical rules 2 and 3)

All outcome math is integer. GCC and Clang produce identical campaign hashes.

## One resolver

`botf::resolveTacticalBattles` (declared in `Simulation.h`, implemented in `Simulation.cpp`) resolves every battle.
Two callers use it:

- `advance()`, for campaigns with `GameState::unifiedCombat` set. Every new campaign sets it.
- `session::resume`, for every campaign.

`advance()` auto-resolves every encounter, including player battles. The session pauses on player encounters and
verifies recorded results by replaying them. An auto-resolved session and headless `advance()` commit the same
fingerprint. `SoakTests` and `CampaignSessionTests` check this in lockstep.

Saves from schemas 4 to 11 load with `unifiedCombat = false`. In headless `advance()` they keep the legacy strategic
resolver, so an old save continued from the CLI plays exactly as it did before. The session path always used the
tactical engine and still does.

## Which ships fight

Battles are pairwise. The resolver scans sectors in row-major order. Within each sector it takes every
`MajorRelation` at war, in empire-id order: (1,2), (1,3), ... (4,5). Schema 14 skips a pair while a non-aggression
pact or stronger, or a timed Peace, is in force (`combatAllowed`, DIPLOMACY.md section 4); declaring war breaks those
treaties first. For each relation, the two empires' surviving
ships fight one encounter. A ship that withdrew this turn takes part in no further duels. A meeting of three or more
empires therefore resolves as a deterministic sequence of duels, and each duel produces a two-owner `CombatReport`.

## Battle records (schema 14)

`toCombatReport` turns each finished encounter into the `CombatReport` kept in `TurnReport::battles` and
`GameState::latestBattles`. Since schema 14 every `CombatRound` also carries `orders`: one `CombatShipOrder` per ship
still in the battle at the start of the round, with the maneuver and target it executed and the flags
`aiControlled`, `decloaked` (ambush volley), `disengaging` (rules 2 retreat not yet complete), `hailed` and `rammed`.
They are saved in the `COMBAT_ORDERS` section (GAMEPLAY-REMASTER.md section 13), so a client can replay a battle
from a save or an older turn with the orders shown, not only the damage. Battles resolved by the legacy strategic
resolver (schema 4-11 campaigns) record no orders.

## Strategic orders become standing tactical orders

| Fleet state | Standing order |
|---|---|
| `retreat` flag, or `Withdraw` stance (only when a safe sector exists) | Retreat |
| `Aggressive` stance | Charge (at the fleet's attack target, if it is present) |
| `Defensive` stance | Evade (at the fleet's attack target, if it is present) |
| `Balanced` stance with an attack target present | Engage that target |
| Otherwise | Tactical AI decides each round |

The player can override any standing order in a session.

## Tactical rules 2

- **Ram.** The attack is mutual when the rammer ends movement within one cell of its target:
  - The target takes damage equal to the rammer's hull.
  - The rammer takes damage equal to the target's hull plus shields before impact.

  Both hits go through shields first, then hull. Maneuver evasion and cloak do not reduce them. Usually both ships
  die.
- **Cloak.**
  - A cloaked ship cannot fire.
  - Engage, Charge, Assault, Harry, Flyby, Strafe and Ram decloak the ship. Its volley that round deals +25% damage
    (the ambush).
  - Circle, Hold, Evade, Retreat and Hail keep the cloak.
  - Cloaked ships are skipped by auto-targeting and take 50% of weapon damage.
  - A cloaked AI ship always opens with an attack order.
- **Retreat.**
  - The first round of Retreat marks the ship as disengaging. It moves toward its own edge with weapons offline,
    and incoming weapon damage is reduced by 25%. It can still be fired upon.
  - If the order is still Retreat at the next movement phase, the ship escapes before that round's fire.
  - Any other order cancels the disengage.
- **Hail.**
  - A truce ends the battle before movement and fire when every active ship on one side hails and the other side
    either:
    - also hails, or
    - is AI-controlled and accepts.
  - The AI accepts when either condition holds:
    - The empires are not formally at war.
    - Its own strength is small enough: `strength(AI) * 100 <= strength(hailer) * (100 + trust)`. Strength is the sum
      of `hull + shields + 3 * (beam + torpedo)` over the side's active ships.
  - The player's side only agrees by hailing.
  - Unarmed AI ships that cannot retreat hail.
- **Hold.** The ship stays in place and takes 20% less incoming damage. It no longer recharges shields, so a battle
  record only ever shows damage.
- **Defensive modifiers** use the order each ship actually executed that round, whether it came from the player, a
  standing order or the AI. `ShipRoundStatus::aiControlled` records which orders were AI decisions, so replay
  verification re-derives those decisions instead of treating them as player orders.

## Tactical rules 3: flight model and weapon events (Remastered)

Rules 3 is rules 2 plus ship handling, firing arcs, typed weapons and a presentation event stream. It is used only
when `GameplayRules::flightModel` is on:

- New Remastered games switch it on (`rulesForPreset`).
- It is saved as the optional `TACTICAL_FLIGHT 1` section after `SHIP_ROSTER`. No schema bump.
- Classic forbids it (`validate()`).
- Saves without the section keep rules 2. This covers Classic saves, schema 4 to 14 saves and Remastered saves made
  before rules 3. Such a save continues exactly as before.

`constructEncounter` sets `Encounter::rulesVersion = FlightTacticalRules` (3). Everything below is integer math.
Rules 1 and 2 code paths are unchanged, and so are the Classic and legacy reference hashes (SHIP-ROSTER.md section 6).

### Flight model

Each ship carries a persistent `facing` (one of eight 45-degree headings; 0 = +x, 90 = +y) and a `velocity` (cells
flown last round). A roster class supplies a `FlightProfile` from `gen_ship_roster.py`. Other ships get one from
their catalogue role.

| Profile field | Meaning | Examples |
|---|---|---|
| `turnRate` | Degrees per round, a multiple of 45 | Scout 180, escort/raider 135, cruiser 90, battleship 45, station 0 |
| `acceleration` | Maximum cells of speed gained per round | 1 (capital) to 3 (escort) |
| `lateral` | Cells of sideways thrust per round | 0 (most capital hulls) to 2 (escorts, raiders, Borg) |
| `size` | `Small`, `Medium`, `Large`, `Huge`, `Station` | Hit chance modifier for pulses and torpedoes: -15, -5, 0, +5, +15 |

Faction adjustments: Borg hulls turn 180 and have lateral 2; Species 8472 get +45 turn and +1 lateral; Tholians get +45 turn.

Deployment faces the enemy: the lower owner id faces 0, the other faces 180. Ships start at `velocity = speed`.

Movement each round:

- **Budget.** Steps = `min(speed (+1 on Flyby/Strafe), velocity + acceleration)`. Turn notches = `turnRate / 45`.
- **Closing.** A ship flies one cell per step along its facing. When the target's heading differs, it first spends a
  notch turning toward it. A turn of 135 degrees or more costs a step as well. With no notches left, it only keeps
  flying if its current heading still closes the distance; otherwise it stops. Slow capital ships therefore overshoot
  or take several rounds to come about.
- **Backing off** (closer than the maneuver's range) is reverse thrust. Each cell costs two steps, and facing is kept.
- **Lateral thrust.** Strafe, Circle and Harry, once at range, slide up to `lateral` cells perpendicular to the
  facing. Each slide picks the side that moves the ship furthest off the target's bow, and it stays within range + 1.
  After sliding, Strafe (and Flyby) keep the passing run. This is the sideways maneuver: a nimble ship slips out of a
  heavy ship's forward arc while its own bow stays on the target.
- **Bringing weapons to bear.** Leftover turn notches swing the bow toward the target. Hold, Hail and stations only turn.
- **Retreat.** The ship turns toward its own edge and flies only while its heading points there. The disengage and
  jump rules of rules 2 are unchanged.
- **Order of movement.** Ships move one at a time in encounter order (owner, then fleet id). Each move sees the
  positions left by the ships before it.

AI: an agile AI ship (lateral 2) that would Charge strafes instead.

### Weapon mounts and arcs

`equipFlight` gives every ship up to `MaxShipMounts` (6) `TacticalMount {type, count, arc, attack}`:

- Roster classes keep their typed mounts.
- Other ships get an owner-typed beam mount (arc All) and torpedo mount (arc ForwardWide):
  - Federation: PhaserArray and PhotonTorpedo.
  - Klingon, Romulan, Cardassian: Disruptor. Romulans use PlasmaTorpedo; the others use PhotonTorpedo.
  - Ferengi: PhaserBank and PhotonTorpedo.
- Stations get All-arc mounts.
- The ship's beam attack is split over its non-torpedo mounts, and its torpedo attack over its torpedo mounts. Each
  mount's share is proportional to `weaponPoints * count * arcPercent`, and the remainder goes to the pool's first
  mount. The total attack is unchanged.

A mount fires only when the target's bearing, relative to the facing, is inside its arc (|relative bearing| in degrees):

| Arc | Covers |
|---|---|
| Forward | 0 to 45 |
| ForwardWide | 0 to 90 |
| Broadside | 45 to 135 (either side) |
| Aft | 135 to 180 |
| All | everywhere |

If the ordered (or auto-picked) target is outside a mount's arc or range, that mount fires at the nearest visible
enemy that is inside both. If there is none, it holds fire.

### Weapon families

`roster::weaponFamily(type)` splits the weapon types into three families:

| Family | Types | Range (percent of mount attack at distance 1/2/3/4/5/6/7) | Hit | vs shields / vs hull |
|---|---|---|---|---|
| Beam | PhaserBank, PhaserArray, Disruptor, PolaronBeam, CuttingBeam, EnergyDampener, TholianWeb, BioEnergy, TemporalWeapon | 100/85/60/35/0 | Always, on the fire tick | 125% / 100% |
| Pulse | PhaseCannon, PulsePhaser, DisruptorCannon, PlasmaCannon | 100/90/60/0 | 3 bolts on consecutive ticks carrying 110% between them; each rolls `95 - 10*(d-1) - evasion`, clamped to 5..95 | 100% / 100% |
| Torpedo | SpatialTorpedo, PhotonicTorpedo, PhotonTorpedo, QuantumTorpedo, PlasmaTorpedo, TricobaltDevice, Torpedo | 40/100/100/100/100/80/50 | One volley rolls `90 - 5*(d-1) - evasion`, clamped to 5..95. A hit is then shot down 10% of the time if the target has a beam mount (point defence) and is not cloaked | 75% / 125% |

Details:

- **Evasion.** Evasion = maneuver bonus + 5 per lateral cell slid this round - the target's size modifier. The
  maneuver bonuses are Evade 25, Circle and Harry 15, Flyby, Strafe and Retreat 10, Charge and Hold -10.
- **Attacker modifiers** are those of rules 2: the beam table applies to beams and pulses, the torpedo table to
  torpedoes. The ambush bonus is +25%. Each shot has +/-5% dispersion.
- **Defender modifiers.** Rules 3 replaces the rules 2 table with Hold 80%, Charge 115%, Retreat 75%, Evade 85%,
  Circle 90%, others 100%. Dodging is now the hit roll. Cloaked targets take 50%.
- **Shields and hull.** A hit of raw damage R first drains up to `R * shieldPct / 100` shields. The raw damage that
  was not absorbed then hits the hull at `hullPct`.
- **Ram** is unchanged: impact and recoil are 100%/100% and ignore evasion and cloak.

### Round timeline

A round lasts `CombatTicksPerRound` (12) ticks:

| Tick | What happens |
|---|---|
| 0 | Truce, Cloak (round 1 only, for ships that start cloaked), Disengage, Retreated, ManeuverStart |
| 4 | ManeuverEnd, Decloak |
| 5 + mount index | Mount fires. A pulse's bolts follow on the next two ticks. A Ram impacts at tick 5 |
| fire + 1 + (distance + 1) / 2 | Torpedo impact, capped at tick 11 |

Within a tick, launches and same-tick beam and pulse impacts resolve before arriving torpedoes, in shooter order and
then mount order. A shot launches only if its shooter was not destroyed at an earlier tick. A ship destroyed at tick
t still fires its tick-t shots.

A shot that arrives after its target is already destroyed is recorded with `hit = false`.

`ShipRoundStatus::beamDamageDealt` and `torpedoDamageDealt` record delivered raw damage. `WeaponSalvoRecord` holds
one aggregate per attacker and defender pair.

### Event stream for client authors

`RoundResult::events` (tactical) and `CombatRound::events` (battle records, `TurnReport::battles`,
`GameState::latestBattles`) hold `botf::CombatEvent` values, declared in `Simulation.h`. They are ordered by `tick`.
Within a tick the order is: pre-move events, then ManeuverEnd and Decloak, then Fire and Ram, then Destroyed.

```cpp
inline constexpr int CombatTicksPerRound = 12;
enum class CombatEventKind : int {
    ManeuverStart = 0, ManeuverEnd = 1, Cloak = 2, Decloak = 3, Fire = 4, Ram = 5,
    Destroyed = 6, Retreated = 7, Disengage = 8, Truce = 9 };
struct CombatEvent {
    int tick;           // 0..11
    CombatEventKind kind;
    int fleetId;        // actor (0 only for Truce)
    int targetFleetId;  // Fire/Ram target, else 0
    int mountIndex;     // Fire: index into the shooter's mounts (-1 otherwise)
    int weaponType;     // Fire: roster::WeaponType; key = roster::weaponTypeKey(type) = "weapon.<Type>"
    int weaponFamily;   // Fire: 0 beam, 1 pulse, 2 torpedo
    int impactTick;     // Fire/Ram: tick the shot lands (== tick for beams and pulses)
    int projectiles;    // Fire: emitters / bolts / torpedoes to draw
    bool hit;           // Fire: connected (false = missed, intercepted or target already gone)
    bool intercepted;   // Fire: torpedo volley shot down by point defence
    int shieldDamage;   // Fire/Ram: absorbed by the target's shields -> shield-flare effect
    int hullDamage;     // Fire/Ram: hull damage -> hull-impact effect
    bool destroyed;     // Fire/Ram: this impact destroyed the target
    int x, y, facing;   // Maneuver events: pose; Fire/Ram: shooter pose at launch; others: actor pose
    int maneuver;       // Maneuver events: TacticalManeuver executed
    int value;          // ManeuverEnd: lateral cells slid (strafe); else 0
};
```

How a client plays a round:

- **Movement.** Interpolate each ship from its ManeuverStart pose to its ManeuverEnd pose over ticks 0 to 4, using
  the start and end facing. A ManeuverEnd `value > 0` means part of the move was sideways: keep the bow and translate.
- **Beams.** Draw from tick to tick + 1.
- **Pulses.** Draw one bolt per Fire event.
- **Torpedoes.** Spawn at `tick` and travel to the target so they arrive at `impactTick`.
- **Impacts.** At `impactTick`, play the shield flare when `shieldDamage > 0`, the hull hit when `hullDamage > 0`,
  a miss or fly-by when `!hit`, and a point-defence burst when `intercepted`.
- **Destruction.** Play the explosion at the Destroyed event's tick.
- **Audio.** Look up the fire and impact cues by `"weapon.<Type>"` (`roster::weaponTypeKey`). These are the same
  keys as `docs/ship-asset-manifest.template.json`.
- **Mount lookup.** Mount positions come from the ship's `Encounter` mounts (`Ship::mounts[mountIndex]`, which has
  the arc for picking a hardpoint). A saved battle record has only the event, so use `weaponType` and the ship's
  asset.

Storage:

- **Tactical text.** Rules 3 encounters write a per-round `EVENTS n` block after `SALVOS`, and a
  `FLIGHT n` section before `END`. `FLIGHT` holds one line per ship: `fid facing velocity turnRate acceleration
  lateral size mountCount` followed by `type count arc attack` for each mount. Both are required for `RULES 3` and
  rejected otherwise. Rules 1 and 2 text is byte-identical to before.
- **Campaign save.** Events go in the optional `COMBAT_EVENTS` section after `COMBAT_ORDERS`. It has one line per
  battle: the round count, then per round the event count followed by 19 integers per event, in the field order
  above, with booleans written as 0 or 1 (`tactical::writeCombatEvent` / `readCombatEvent`). It is written only when
  some round has events, and an empty section is rejected. No schema bump: a schema 14 save without it loads with no
  events. A schema 13 header may not carry it.
- **Validation.** `validate()` rejects out-of-order ticks, unknown kinds and non-participant actors or targets.

Tests: `BotfTacticalTests` covers arcs, turn rate and acceleration, strafe, the weapon families, stream determinism
and mid-battle reload. `BotfSessionTests` covers session and headless equality, player strafe replay and the
save round trip.

Balance (provisional). Three 200-turn Remastered reference campaigns had about as many battles with losses as under
rules 2. The average battle runs longer, because agile ships slip out of forward arcs and weak hulls stalemate more
often.

## Rules 1 (legacy)

`TACTICAL_ENCOUNTER_V1` text loads with `rulesVersion = 1` and reproduces the schema-11 engine exactly:

- Retreat is free.
- Cloaked ships can fire.
- Ram is one-sided.
- Hold recharges shields.
- AI ships defend as if their order were Engage.

Replay verification first tries the recorded orders as explicit player orders. If that fails, it treats each player
ship as AI-driven until the first round where its recorded order differs from the AI's decision. V1 recordings never
marked AI decisions, so this second pass is what lets old pending battles resume.

## Encounter format V2

`serializeEncounter` always writes `TACTICAL_ENCOUNTER_V2`. The V2 format adds these sections and fields:

- `RULES` and `RELATION` (at war, trust).
- `STANDING`, the standing orders. This fixes mid-battle save/load divergence.
- Per-status decloak, hail and AI flags, the intermediate position, and an integer facing.
- A per-round truce flag.
- `SALVOS`.

Empty names are written as `\e`. Loading rejects all of the following:

- out-of-range speed, attack, maneuver, class, owner, controller or cloak flags;
- references to unknown ships or groups;
- duplicate sections;
- a round count that disagrees with the current round;
- a `FLIGHT` section or `EVENTS` blocks that are missing under `RULES 3` or present under any other rules;
- any header other than V1 or V2.
