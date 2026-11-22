# Diplomacy & intelligence rules (remaster)

This document records the modern diplomacy and intelligence rules implemented in
`game/Source/BotfSim/Diplomacy.h`, `Intelligence.h` and the diplomacy/intel regions of
`Simulation.cpp`. Every number below is the one in the code.

Scope:

- The modern rules apply to **modern campaigns** (`campaignExpansionEnabled && modernEnginesActive`,
  which is every `generateReferenceCampaign*`).
- Legacy campaigns keep the legacy minor and major offers. They get only the AI war fix (§3) and
  the rejection of modern orders (§10).
- All logic is deterministic and integer-only. There is no floating point in either engine, so GCC,
  Clang, MSVC and Emscripten produce identical results.
- AI randomness comes from hashing `seed`, turn and participants. It never consumes the campaign
  RNG.

## 1. Personalities

Empire slots map to personalities as follows: 1 Federation, 2 Klingon, 3 Romulan, 4 Cardassian,
5 Ferengi (`personalityForEmpire`).

| | Federation | Klingon | Romulan | Cardassian | Ferengi |
|---|---|---|---|---|---|
| Theme | cooperation | honor/aggression | suspicion | order/expansion | profit |
| War aggression | 0 | 45 | 25 | 40 | 10 |
| Max wars it opens | 1 | 2 | 1 | 1 | 1 |
| Min war turns before suing for peace | 3 | 15 | 10 | 12 | 4 |
| Peacetime trust baseline | 60 | 40 | 35 | 40 | 50 |
| Breaks treaties | never | never | when ≥300% power and trust < 20 | same as Romulan | never |
| Demands tribute | – | yes | – | yes | – |
| Goodwill gifts | yes | – | – | – | – |

### 1.1 Acceptance thresholds

These are the thresholds an AI uses when deciding on a proposal (`thresholdsFor`):

| Treaty | Fed | Kli | Rom | Car | Fer |
|---|---|---|---|---|---|
| Peace | 40 | 60 | 55 | 50 | 45 |
| Non-aggression | 40 | 55 (+5 unless the proposer is stronger) | 55 | 50 | 45 |
| Friendship | 50 | 60 | 65 | 60 | 55 |
| Affiliation | 60 | 70 | 75 | 70 | 65 |
| Alliance | 70 | 75 | 85 | 80 | 80 |
| Trade pact | 45 | 55 | 55 | 50 | 35 |
| Tribute demanded of it | 50 | 75 | 60 | 65 | 60 |

Minor races use Friendship 50, Affiliation 60, Trade 45, Membership 80 and the Federation
non-aggression threshold. A gift is always accepted.

### 1.2 How a proposal is scored

The score starts at the target's trust in the proposer, or at the attitude for a minor. The
following adjustments are then applied:

- **Relative fleet power.** A proposer with more than twice the target's power gets +10; one with
  less than half gets −10. For Klingons the values are +15/−20, and for Cardassians +10/−15.
- **Tone.**
  - Cordial: +10 (Klingons and Romulans: +5).
  - Demanding: −15 (Klingons and Cardassians: −25; Ferengi: −10).
  - Hostile: −30.
- **Credits offered.** +min(30, offer/100); for the Ferengi, +min(50, offer/60).
- **Credits demanded.** −min(40, demand/80); for the Ferengi, −min(60, demand/40).
- **History.** −10 per treaty the proposer has betrayed (at most −30). −3 per grievance the target
  holds against the proposer (at most −15).
- **Shared enemies.** For cooperation treaties, +10 per shared enemy (at most +20).
- **Peace proposals.** War weariness adds +duration/2 (at most +20).
- **Variance.** A deterministic ±5.

The target accepts if score ≥ threshold, ignores the proposal if score < threshold − 20, and
otherwise rejects it.

## 2. Proposals

- **Lifecycle.** A proposal goes Pending → Accepted, Rejected, Ignored, Expired, VoidedByWar or
  Cancelled.
  - Resolved proposals are purged at the start of the next resolution; the event log keeps their
    record. This fixes a permanent lock-out after 128 rows.
  - At most 128 proposals can be open at once, and at most 16 per proposer.
- **Response windows.** A human target has 1 resolution to answer a proposal by default.
  AI-initiated proposals to the player allow 2 (`submitProposal(..., responseTurns)`). After the
  window, the proposal is Ignored. AI targets answer at the next resolution.
- **War is unilateral.** `DeclareWar` takes effect at once. A `SubmitDiplomaticProposal` of type
  War takes effect at the next resolution whatever the target does. A declaration cannot be
  answered and cannot carry credits.
- **Gifts and tribute.** `TreatyType::None` with a credit offer is a gift. With a credit demand it
  is a tribute demand, which is only allowed between majors. On acceptance:
  - A gift raises trust by min(10, max(1, credits/50)).
  - Paid tribute costs the payer 5 trust and gives the payer +1 grievance.
  - A refused tribute gives the demander +2 grievances and costs 3 trust. An ignored demand gives
    the demander +1 grievance.
- **Signing.** Signing any treaty between majors gives +5 trust.
- **Treaty cap.** At most 128 treaties can be active. A proposal that would exceed the cap is
  rejected before any credits or effects are emitted. `DiplomacyEngine::establishTreaty` enforces
  the same cap for legacy acceptances.
- **Supersession.** Cooperation treaties (non-aggression < friendship < affiliation < alliance)
  replace weaker ones. Trade pacts coexist with them. A timed Peace treaty is replaced by any
  cooperation treaty.

## 3. War and peace

### 3.1 AI war decisions

`assessWarDeclaration` is evaluated for each AI in `planRivals`, against each contacted, surviving
neighbour. An AI opens at most one new front per turn, and does so only if `aiAggressionEnabled`
is set. A candidate is ruled out in any of these cases:

- The turn is before 30 plus the personality's patience (Klingon, Dominion, Borg 0; Cardassian 5; Romulan 12; Ferengi 20; each
  plus a seeded 0..8 per pair). Early-game peace is turn 30 (rivalry update, 16 Nov 2026; it was 25).
- Fewer than 20 turns have passed since the last war/peace transition between the pair (the
  cool-down). The transition turn is `warStartTurn`, which now also records the turn peace was
  signed, or the engine's `lastWarTurn`/`lastPeaceTurn`.
- The AI is already fighting its maximum number of wars (Klingon 3, Romulan 2, Cardassian 2, Dominion 3, Borg 3, others 1; one
  extra front against the human is always open), or the human already faces its quota of AI wars: 1, 2 from turn 40, 3 from turn 80.
- A non-aggression pact or stronger, or a timed Peace treaty, is in force. The exception is a
  treacherous personality that is overwhelmingly stronger and distrustful (see §1).
- The personality's aggression is 0 (the Federation). It only declares war with at least 10
  grievances and trust below 15.

Otherwise the AI computes this score:

```
score = aggression + (50 - trust) + 6*min(grievances, 10) + min(15, 5*targetBetrayals)
      + (ratio >= 100% ? min(30, (ratio-100)/5) : -min(40, (100-ratio)/2))
      + (aggression >= 20 ? min(30, turn-25) : 0)          // restlessness (was min(15, (turn-25)/5))
      + rivalry pressure (0..50, section 3.5)
      - (trade pact ? 10 (Ferengi 20) : 0) ± 5
```

It declares war if the score is 60 or more.

- `ratio` compares the aggressor's fleet power with the target's, including the target's full
  allies.
- Old behaviour: empires 2 and 4 declared war on the player every turn after turn 25, which made
  peace impossible.

### 3.2 Declaring war

Declaring war, by any route, has these effects:

- The relation is set to war.
- Trade routes and pending legacy offers between the two are dropped.
- `warStartTurn` is set to the current turn.
- The declarer loses 25 trust (advanced rules).

In modern campaigns it also:

- cancels the pair's open proposals (they are resolved, so they are never re-reported);
- breaks every treaty between them;
- calls the defender's allies to arms (§4).

### 3.3 Betrayal

Declaring war while a non-aggression pact or stronger, or a timed Peace, is in force is a
betrayal. It has these effects:

- The victim loses an extra 15 trust and gains 3 grievances.
- Every other contacted surviving major loses 10 trust with the traitor.
- The traitor's standing records the betrayal. It weighs on every future proposal (−10 each) and
  makes it a war target (+5 each).

### 3.4 Peace

- **AI proposals.** An AI proposes peace once the war has lasted its minimum number of turns and
  it is losing (below 80% power), weary (3× its minimum war length), or trust has recovered to 35.
- **Weariness.** After 10 turns of war, trust below 30 recovers by +1 every 2 turns.
- **Legacy peace.** Legacy peace offers still use the legacy acceptance rule. In modern campaigns
  they also create a 25-turn Peace treaty through the engine. That treaty binds honourable AIs.

### 3.5 Rivalry (16 Nov 2026, playtest "no war")

A friend played 40 minutes of the browser build and met only non-aggression pacts. Cause: an AI signed a pact the moment its
desire passed the threshold, a pact binds every honourable personality, and the war score had no reason to rise (no friction, a
slow restlessness ramp, AIs that kept one or two ships). The rivalry layer applies to modern campaigns only.

- **Rivalry pressure** (`rivalryBreakdown`, 0..50, from the AI's point of view), each part weighted by personality
  (`rivalryProfileFor`: territorial / expansionist / opportunist / profit):
  - friction: 4 per pair of its colony and the other's colony within 5 sectors (max 20), 3 per foreign ship within 3 sectors of
    its colonies (max 9);
  - expansion: 3 per colony the other holds beyond its own (max 12);
  - opportunity: +12 if the other's fleet is under half its own, +8 if the other is at war with a third power;
  - profit: +10 if the other holds 300+ more credits and has no trade pact with it.
  Weights (%): Klingon 100/80/40/0, Romulan 80/80/100/0, Cardassian 100/150/40/0, Ferengi 40/40/0/100, Dominion 100/100/60/0,
  Borg 100/100/0/0, Federation none.
- **Pacts are vetoed by the war score.** The treaty planner and the proposal evaluator read the same `hostilityScore` the war
  assessment uses. At or above the personality's pact ceiling (Federation 100, Ferengi 55, Romulan 45, Klingon and Cardassian 40,
  Dominion 35, Borg 20) the AI offers no non-aggression pact, friendship, affiliation or alliance and refuses the player's
  (penalty = score above the ceiling + 15). Trade pacts are not vetoed. A weak AI or one that trusts the player still signs, so
  pacts remain possible, just not with everyone. The Ferengi sign a non-aggression pact only after ten turns of trade.
- **Relations sour.** Every second turn (fourth inside a plain non-aggression pact) an AI with pressure of 10 or more loses 1 trust
  with that neighbour (2 at 25 or more), to a floor of 10; every tenth turn at 25 or more it adds a grievance, and the player is
  told why ("... is uneasy about your ships and colonies on its border. Relations are cooling."). Affiliation and alliance are spared.
- **Standing fleets.** Klingon, Dominion and Borg keep min(5, 2 + turn/25) destroyers in peacetime, Cardassian min(4, 1 + turn/30),
  Romulan min(3, 1 + turn/40), others 1; at war an AI keeps 3. These now rank ahead of its upgrade ladder in the build order.
- **Personalities.** Dominion aggression 45 (was 35) and Borg 40 (was 5): hostile by default. Klingon is ready from turn 30, the
  Ferengi are mercenaries (friendly while the trade is good, war only against a weak, rich neighbour).
- **Wars no longer starve the rival.** With nearly every AI at war from turn 30, two things that used to need peace now run at war on the
  modern engines: outposts (a war used to stop them, so no rival ever fortified) and construction research (15% of a war budget, so
  starbases can still be commissioned). The standing fleet applies in peacetime only; the old build order stands at war.
- **Restlessness** is scaled by relative power: full at 60% of the target's fleet, half from 40%, none below, and halved again above trust 50 (none above 70), so a hopelessly
  outmatched AI still sits still.
- Proof: `tests/scenarios/AiWarFoodTests.cpp`, first war on a player who accepts every pact, 10 seeds per shipped preset.

## 4. Treaty consequences (modern campaigns)

Each turn, between surviving majors:

| Treaty in force | Credits (each party) | Trust | Other |
|---|---|---|---|
| Non-aggression | – | +1 per 4 turns, up to 60 | No combat between the two empires (see "Treaties forbid attacks" below). |
| Friendship | +3 | +1 per 2 turns, up to 75 | No combat. |
| Affiliation | +5 | +1 per turn, up to 85 | No combat. Shared star charts (the explored maps are merged) and shared sensors. |
| Full Alliance | +8 | +1 per turn, up to 100 | No combat. Shared charts and sensors, mutual defence, shared supply. |
| None, at peace | – | drifts 1 per 3 turns toward the mean of both baselines | – |

- The dividend is added to `TurnReport::tradeIncome` for the player, and itemised as
  `TurnReport::ledger.treatyDividends` and `creditForecast().treatyDividends` (schema 14).
- A trade pact enables legacy trade routes. When a pact ends, its routes are dropped.

Schema 14 wires the remaining effects into the turn loop:

- **Treaties forbid attacks.** `combatAllowed(state, a, b)` is true only when the two empires are formally at
  war *and* `treatyForbidsAttack` is false (no non-aggression pact or stronger, and no timed Peace, in force). Every
  combat path uses it: battle detection in the unified resolver (and the legacy resolver), pursuit of an attack
  target, invasions at resolution, station combat, repairs and boarding, the rival AI's targeting and sector-safety
  estimates, and station raids. The player's `AttackFleet` and `Invade` orders are refused while a treaty holds
  ("declare war first"). Declaring war breaks the treaties (section 3.2), so combat can follow in the same turn.
- **Shared supply (Full Alliance).** A ship is supplied within its own supply range (`supplyRange(state, fleet)`,
  that is `shipRange + Propulsion/2` of its owner) of an allied powered shipyard, inside an allied station's supply
  radius, and at the exit of a known wormhole whose entry is supplied by any of those. `supplied()`, `route()`,
  task-force movement (`taskForceRoute`) and the AI all share the rule.
- **Shared sensors (Affiliation or Alliance).** `visible(state, empire, x, y)` also covers every sector a
  `sensorSharingEmpires` partner sees with its own ships, colonies and stations (nebula rules apply to each
  observer). `visibleStations`, first contact and the AI's targeting see through partners too.

### 4.1 Calls to arms

When an ally of the defender is at peace with the aggressor, it receives a `DefenseCall` with a
3-turn window.

- **AI allies** decide at once:
  - The ally honours the call if its fleet power relative to the aggressor is at least: Klingon
    33%, Federation 50%, Cardassian 100%, Romulan 150%, Ferengi 200%.
  - The ally refuses if it is bound by a treaty with the aggressor, unless it is Klingon. A
    Klingon honours the alliance even if that betrays the other treaty.
- **The player** declares war before the call falls due, or refuses it at once with
  `makeRefuseCallToArmsCommand(id, defenderId, aggressorId)` (`CommandType::RefuseCallToArms`, schema 14). The order
  is validated on submission (the call must be open), applies at the start of the next turn resolution and is
  reported as "Diplomacy: We refused *defender*'s call to arms against *aggressor*. *Defender*'s trust in us fell
  by 20." Letting the call lapse still counts as a refusal when it falls due.
- **Honoured:** +10 trust with the defender.
- **Refused** (lapsed or explicit, `DiplomacyEngine::refuseDefenseCall` for the explicit order): −20 trust and
  +2 grievances. An AI defender whose trust would fall below 40 cancels the alliance (the treaty's status becomes
  Dissolved).
- Calls lapse on peace or when the alliance ends.

## 5. AI-initiated diplomacy

`planAiDiplomacy` produces at most 2 intents per AI per turn. There is at most one open proposal
per direction.

**Cool-down.** The base cool-down is 6 turns toward another AI and 8 toward the player. Each
consecutive refused or ignored proposal doubles it, up to 4×. An acceptance resets it.

**Desire.** The AI computes:

```
desire = trust + affinity + 10*sharedEnemies(max 2) - 5*grievances(max 4) - 10*targetBetrayals(max 3)
       + (target has > 2x our power ? 10 : 0) ± 3
```

Affinities (Fed–Kli 0, Fed–Rom −10, Fed–Car −5, Fed–Fer +5, Kli–Rom −15, Kli–Car −5, Kli–Fer −5,
Rom–Car 0, Rom–Fer 0, Car–Fer +5).

**Options.** The planner checks these options in order and proposes the first that applies:

1. Ferengi trade pact (desire ≥ 30).
2. Non-aggression pact: no treaty, and desire ≥ 45 (Fed), 60 (Kli), 55 (Rom), 55 (Car) or 50 (Fer).
3. Friendship: the non-aggression pact is at least 5 turns old, and desire ≥ 55/65/70/65/60.
4. Affiliation: the friendship is at least 8 turns old, and desire ≥ 65/75/85/75/70.
5. Alliance: the affiliation is at least 10 turns old, desire ≥ 75/80/95/85/85, and there is a
   shared enemy (the Federation does not need one).
6. Trade pact: desire ≥ 45/60/55/50/30.
7. Tribute demand (Klingon or Cardassian): no treaty, at least 200% power, trust below 50, the
   target has at least 100 credits, and 15 turns since the last demand. The demand is
   clamp(credits/10, 25, 250).
8. Goodwill gift (Federation): desire is within 15 below the non-aggression threshold and the AI
   has at least 400 credits. The gift is min(100, credits/10).

**At war.** The AI proposes peace instead (§3.4).

**The player.** Proposals to the player appear in `incomingProposals`. The player answers with
`makeRespondProposalCommand`, and the turn report announces the proposal:

"Diplomacy: Klingon Empire proposes a Non-Aggression Pact (proposal N, respond by turn T)."

## 6. Minor races

Minor councils review their relations every 5 turns (on turn % 5 == 0).

For empires the minor is not a member of:

| Situation | Attitude change |
|---|---|
| Friendship-class relation (engine Friendship/Affiliation, or legacy friendship) | +2, capped at 85 (90 with Affiliation) |
| Non-aggression pact | +1, capped at 65 |
| No treaty | 2 toward 50 |
| Proximity: an empire colony within 3 sectors | +1, capped at 70 |
| Rival influence: another empire is friendship-class with the minor and this one is not | −2, floor 30 |

For the empire the minor is a member of:

- −2 for rising expectations.
- +2 if the colony's morale is at least 70.
- −2×(wars−1) while the owner fights two or more wars.

**Secession.** A member whose attitude toward its owner falls below 30 secedes, provided the
owner keeps another colony. Its colony is removed, it becomes independent, attitude is capped at
20 and the friendship is cleared. The event is `MinorSeceded` and it is reported in the turn
report.

**Gifts.** A gift raises attitude by 4 per 1,000 credits (minimum 1, maximum 100); the formula
cannot overflow. A gift to another empire's member also lowers the member's attitude toward its
owner by half the gain (rival influence).

**Conquest.** A member minor whose colony is captured becomes a member of the conqueror, with
attitude at most 40. See integration note 2.

Membership rules are unchanged: an independent minor, contact, friendship from a prior turn and
attitude ≥ 80.

## 7. Eliminated empires

An empire is eliminated when it owns no colonies (`empireEliminated`). An eliminated empire:

- can neither propose, be proposed to, declare or be declared war on, nor be targeted by
  intelligence orders;
- has its treaties dissolved and its open proposals cancelled (status Cancelled);
- has any calls to arms involving it lapse;
- is excluded from intelligence as both attacker and defender. Every budget line against it
  returns to internal security, and the player is told.

## 8. Diplomatic victory (predicate)

`diplomaticVictoryStatus(state, empire)` counts two populations:

- **Bloc population:** the empire's colonies (member minors included) plus those of its surviving
  Full Alliance partners.
- **Galaxy population:** every colony plus a nominal 30 per independent minor.

`achieved` is true when the bloc holds a strict majority *and* includes at least one ally or member
minor. Otherwise a plain conquest majority would count as diplomatic.

`diplomaticVictoryAchieved` is false in legacy campaigns. The campaign victory adds pacing rules on top
(turn 150 or later, a two-thirds supermajority, at least two partners, and the claimant must lead its bloc);
see GAMEPLAY-REMASTER.md section 5.

## 9. Intelligence

### 9.1 Outputs and strength

All values are integer fixed point.

- **Area output:** `base * alloc * (100 + bonus) / 10000`. For example, 100 at 29% gives 29, where
  the old double arithmetic gave 28.
- **Defense (milli-points):** `(last-20 security sum * 1000 / 20 + stockpile * 20) * morale% / 100`.
- **Attack (milli-points):** `(last-20 espionage+sabotage sum * 1000 / 20) * 200%`. The attack is a
  per-turn average with a concentration factor.
  - Previously the attack was a 20-turn *sum* measured against a per-turn average defense, so a
    20% operation beat 80% security 11 times in 12.
  - Now parity needs a per-rival share of half the defender's security share.

### 9.2 Outcome of an operation

- **Ratio:** R = attack/defense in permille, plus 100 per computer tech level of lead.
- **Roll:** r in [0, 1000).
- **Score:** R × (600 + 0.8r) / 1000.

| Score | Outcome |
|---|---|
| < 400 | Deep failure: exposed with max(50%, detection%). Trust penalty 15 (5 at war). |
| < 800 | Failure: exposed with detection% = clamp(90 − R/12, 10, 90). Trust penalty 8 (2 at war). |
| < 1300 | Minor success |
| < 2000 | Standard success |
| ≥ 2000 | Major success |

An exposed failure is reported as `Fiasco` with `detectedByDefender`. The defender gains +1
grievance against the attacker.

### 9.3 Theft

- **Credits.** 5/15/35% of the defender's credits, clamped to MaxBalance for the attacker.
- **Research.** 50/150/400 points, taken from the defender's reserve and then from its unspent
  progress. The points are added to the attacker's lowest non-maxed field, with level-ups.
  `research` is adjusted on both sides so the research ledger always reconciles. This fixes the
  Supreme-era throw "Research ledger does not reconcile".

### 9.4 AI intelligence doctrine

`planAiIntelStrategy` re-plans the budget and focus of every AI each turn. It targets only
contacted rivals and is off when `aiAggressionEnabled` is false.

| | Fed | Kli | Rom | Car | Fer |
|---|---|---|---|---|---|
| Base internal security | 80 | 65 | 55 | 70 | 70 |
| Baseline interest in a rival | 2 | 2 | 4 | 3 | 3 |
| Extra interest in an enemy at war | 6 | 8 | 6 | 6 | 4 |
| Sabotage share, peace / war | 0/40 | 20/70 | 30/50 | 30/60 | 10/30 |
| Focus, peace / war | Science/Military | Military/Military | Science/Military | General/Military | Economic/Economic |
| Spies on allies | no | no | yes | no | no |

**Security.** The base, +5 per spy caught in the last 10 turns (at most +20), +5 at war, clamped
to 40–90.

**Interest in a rival.** Starting from the table values:

- +2 if trust is below 30.
- +2 if the rival is stronger.
- + the number of recent incidents against us (retaliation, at most 3).
- −2 per recent exposure of our own agents (at most −6; at least 1 while at war).
- Halved for Friendship.
- 0 for Affiliation or Alliance, unless the doctrine spies on allies.

The AI concentrates on its 2 most interesting rivals and splits the foreign share by largest
remainder.

## 10. Save format

The engines keep writing inside the existing sections, and old saves load and re-serialize
byte-for-byte.

- `DIPLOMACY_ENGINE` (`BOTF_DIPLOMACY_V2`) gains optional sections before `END_BOTF_DIPLOMACY`.
  Each is written only when non-empty, always in this order:
  - `relationMemory N` / `rm first second lastWar lastPeace lastProposal12 lastProposal21 griev1 griev2 refusals12 refusals21`
  - `standings N` / `st empire betrayals lastBetrayalTurn`
  - `defenseCalls N` / `dc ally defender aggressor issued due`
- `TreatyStatus::Dissolved = 4` and the new event types 12–18 are appended values.
- `INTEL_ENGINE` (`BOTF_INTEL_V1`) gains `extraRivals N` / `xrival id` + `rData/espHist/sabHist`
  for per-rival state outside the budget (for example, focus set on an unbudgeted rival).
  Default-valued rows are omitted.
- Operation records are bounded on load:
  - opType 0..4
  - credits and research 0..10^12
  - severity 0..3
  - penalty 0..100
  - detected 0/1
  - attacker ≠ defender
  - no trailing tokens
- Escapes must be exactly two hex digits.
- Legacy campaigns reject `SubmitDiplomaticProposal`, `RespondDiplomaticProposal`, `SetIntelBudget`,
  `SetIntelFocus` and `RefuseCallToArms`, because their state is not serialized there.
- Schema 14: a pending `RefuseCallToArms` order appends the aggressor ID to its command line (the defender is
  `targetEmpire`). Older schemas reject it.

## 11. Integration notes (turn pipeline owner)

1. Done (schema 14): `treatyForbidsAttack` gates every combat path through `combatAllowed` (section 4).
2. The invasion merge should set `Minor::memberOf` to the conqueror when a member minor's colony
   is captured. `reconcileMinorMembership` does the same thing idempotently at the start of modern
   diplomacy resolution. Legacy campaigns still need the pipeline fix.
3. Done (schema 14): `alliedEmpires` partners share supply (section 4).
4. Done (schema 14): `sensorSharingEmpires` partners share sensors in `visible()` (section 4).
5. Done: `diplomaticVictoryAchieved` feeds the victory check with remaster pacing rules
   (GAMEPLAY-REMASTER.md section 5).

## 12. Peace deals that move worlds (E3, 22 Nov 2026; save schema 27, `remaster-27`)
- A Peace proposal may carry `proposalCedeSystems` (worlds the proposer hands over) and `proposalDemandSystems` (worlds the proposer
  asks for), at most `PeaceMaxWorlds` (3) each. Rejected: a capital, an empire's last other world, a world not held or not known to
  the proposer, a duplicate, a world already moving this turn. They resolve as `TransferColony` operations when the peace takes effect.
- `warScore(ctx, self, other)`: plus or minus 60 from the fleets' relative strength, plus clamp(4 x difference in colonies, +-40);
  -100 (losing) to +100 (winning). The bridge writes `warScore`, `ourCedable`, `theirCedable` on each relation at war.
- AI evaluation: +12 for each world offered to it (cap 36); each world it is asked for costs 25, relieved by up to 50 when it is
  losing (-ws / 2); asked for a world while not clearly losing (ws > -20) it refuses (-200). A losing AI (ws <= -30: one world,
  <= -60: two) adds border worlds to its own peace offers. The soak after this change: scripted conquest medians stay in 200 to 500
  (302 medium, 317 large) and AI churn holds (rc-captures/soak9).
- Save: the terms are written only when a proposal has them (a `proposalTerms` block, a pending-command tail); the header chain is
  26 -> 27. Schema 26 saves load unchanged.
- E2 invasion odds: `invasionOdds(state, fleetId, systemId)` (bridge `invasionOdds`) pools the transports ordered to the world and
  gives troops against defence (population / 5 plus batteries and shields); the same numbers resolve the invasion.
- E4: `RenameFlagship` (the name travels in `templateName`, 24 characters) writes a `FLAGSHIP_NAMES` section; the name shows in
  battle logs and the flagship-loss message. E5: `megaprojectName(state, empire)` gives each power its own project name.

