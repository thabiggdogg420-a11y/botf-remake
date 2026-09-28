# Remaster combat rules (save schema 12, tactical rules 2)

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
- any header other than V1 or V2.
