# F5 balance proposals (for Simon; nothing applied)

Outranked by: Simon's words in `AI_stuff\standing\simon\` and `botf-rulings.md`; docs/design/BALANCE-AI.md for the current values. Balance changes are rule changes, so each line below waits for Simon's yes or no. Nothing in the game has been changed.

Barclay, 20 Nov 2026. Round 2 (observer mode, the memory fix) added the same day: see "Round 2 evidence" below. Still nothing applied.

## How the evidence was made
- Tool: `tests/scenarios/AiSoakReport.cpp` (target `BotfAiSoak`, a report, not a suite). For each registered preset it starts a campaign with the preset's own new-game options (lineup, era, size, shape, minor density, difficulty, start systems, threats), leaves the player's seat passive (no orders), and lets the rival AIs play up to 400 turns. Every turn it reads `victoryProgress` for every empire still standing and records the best percent and the first turn each condition was met.
- Run: 9 presets x 8 seeds (7000 + 131k), 72 campaigns. Raw output: `Documents\ChatGPT\f5-balance\r2\<preset>-<seed>.txt`. An earlier run on default galaxy options is in `f5-balance\soak-*.txt`.
- Limit: the player's seat is passive, and a campaign ends when the player is eliminated (the core judges the end from the player's side). 56 of 72 campaigns ended that way, so the AIs rarely got to finish one another. See finding 5.

## Findings
1. **Diplomatic victory comes on the first legal turn, and it is nearly always the Ferengi.** In Remastered the Ferengi won diplomatically at turn 150 and 154 (the rule's earliest turn is 150). On the default-galaxy run it was 5 of 8 Remastered games, every one at turn 150 to 249, median 150. Ultimate: Ferengi, turn 150. Mirror Universe: the Cardassian Alliance, 3 of 8 (median turn 230). No other victory type was reached by any AI in any preset.
2. **Ferengi expansion runs away.** Average peak colonies per AI (round 2): Remastered Ferengi 16.0 against Klingon 7.1, Romulan 9.2, Cardassian 7.6. Balance of Power: Ferengi 29.5 against Klingon 9.4, Romulan 19.5, Cardassian 13.4. In Ultimate the gap is small (9.5 against 7.9 to 8.6).
3. **AIs never eliminate each other.** Across all 72 campaigns no AI empire lost its last colony. Every elimination was the passive player. Conquest and domination were never reached by an AI (best domination: Ferengi, Classic, once, turn 344).
4. **Vendetta reads as met when the passive player falls.** In Ages, Galaxies and Ultimate several AIs "reached" vendetta on the same turn (73, 90, 104 on the default-galaxy run). Their sworn rivals include the passive player, so this is mostly the harness. It may also be real: a vendetta can be settled by someone else's war.
5. **FIXED in round 2 (a bug, not balance).** The Dominion War preset ran out of memory. Seed 7655 (default galaxy) grew past 19 GB during turn 61's advance. Seed 7131 (the preset's own options) passed 4 GB after turn 50 and was stopped. The turn-61 save is `Documents\ChatGPT\f5-balance\soak-dominion-war-7655-turn61.sav`. A player would see the game hang and eat memory at end turn.

## Proposals (each one is Simon's call)
- P1 (finding 1). Make a diplomatic win hold. Option a: the bloc must keep the two-thirds share for 10 consecutive turns. Option b: raise the earliest turn from 150 to 200. Option c: require 3 allied empires or member races, not 2. a is the closest to the original's spirit and stops a turn-150 snap win.
- P2 (finding 2). Find out why the Ferengi out-expand before changing any number: their AI personality, credits or trade income, or minor-race joins. Nothing is proposed until a breakdown run says which. If it is minors, the Ferengi join bonus is the lever.
- P3 (finding 3). Have the AI finish wars. When a rival is down to its last 1 to 3 colonies and the AI is winning, the AI commits to invasion instead of making peace. This is an AI behaviour change, so it is a rule change for Simon.
- P4 (finding 4). None yet. The harness change below shows whether this is real.

## Not rule changes (next steps I can take on Simon's word)
- T1. Fix the Dominion War memory runaway (finding 5). This is a bug, starting from the turn-61 save.
- T2. Add an observer mode to the core for soaks only (the AI also plays the player's seat), so soaks can run to every victory type. It is behind a flag that no game sets, and the player-facing rules are unchanged.

## Round 2 evidence (20 Nov 2026, observer mode)

### What changed in the harness (no rule changed)
- **The memory runaway (finding 5) is fixed.** Cause: `threats::battles` (Threats.cpp) fights a threat actor (Dominion War's
  Jem'Hadar and similar) once per empire present, picking the lowest empire id that still has ships there. An empire whose ships
  neither died nor withdrew (a duel that ran to its 50-round limit) was picked again, with the same seed, forever, and every
  repeat added a battle report: 19 GB. Each empire now meets an actor once a turn, which is what the code's own comment says the
  rule is. Regression suite `BotfRunawayTests` (tests/scenarios/RunawayTests.cpp, fixture tests/fixtures/dominion-war-7655-turn61.sav):
  it fails on the old code (watchdog: 1 GB inside one advance after 6 s) and passes on the new (peak 11 MB, 0.03 s a turn; the
  passive-seat campaign ends at turn 66 with the player eliminated, the observer campaign runs to turn 120). Dominion War seeds
  7655 and 7131 (the preset's own options) both ran the full 400 turns; slowest turn 0.025 s and 0.042 s.
- **Observer mode (T2) is built.** `GameState::observerMode`, set only by the soak report (no game, screen or save sets it; it
  is not saved). The AI plans the player's seat with the same code as every rival (orders, research, diplomacy, intelligence,
  stations, surveys, megaprojects), the player is not "human" to the diplomacy engine, and the player's elimination does not end
  the campaign. The campaign ends when one empire is left or any empire meets a victory condition: the core's own scientific,
  diplomatic and vendetta checks (unchanged), and the player's domination test (three fifths of the systems from turn 20)
  applied to every empire. `BOTF_SOAK_PASSIVE=1` gives round 1's passive seat back.
- Run: 9 presets x 8 seeds, 400 turns (`f5-balance\r3\<preset>-<seed>.txt`), then Classic, Klingon Civil War and Balance of
  Power for 1,200 turns on 4 seeds (`f5-balance\r3\long\`). No campaign failed, ran away or passed the 4 GB guard; the
  slowest turn was 0.37 s in 400 turns and 0.59 s by turn 1,200 (Balance of Power, the largest map).

### Endings over 72 campaigns (400 turns)
| Preset | Endings | Winners |
|---|---|---|
| Classic | 8 turn cap | - |
| Remastered | 8 diplomatic (7 at turn 150 to 176, 1 at 399) | Ferengi 7, Federation 1 |
| Ages | 7 diplomatic (turn 150 to 166), 1 vendetta (turn 122) | Ferengi 4, Federation 4 |
| Balance of Power | 1 domination (turn 311), 7 cap | Ferengi 1 |
| Dominion War | 4 diplomatic (turn 150, 150, 150, 151), 4 cap | Federation, Klingon, Cardassian, Romulan |
| Galaxies | 1 diplomatic (turn 150), 7 cap | Federation 1 |
| Klingon Civil War | 8 cap | - |
| Mirror Universe | 2 domination (turn 395, 399), 6 cap | Terran Empire 2 |
| Ultimate | 7 diplomatic, 1 scientific (Romulan, turn 391) | Federation 5, Ferengi 2, Romulan 1 |

Victory types reached by an AI: **diplomatic, scientific, vendetta, domination. Conquest: never**, not in 400 turns on any
preset and not in 1,200 turns on Classic, Klingon Civil War or Balance of Power (12 campaigns, all at the cap). Classic has no
alternative victories, so a Classic campaign between AIs does not end.

### What the round-2 evidence says about the findings
1. Finding 1 holds with every seat played: 23 of the 27 diplomatic wins came at turn 150 to 176, 21 of them on turn 150 or
   151, the rule's first legal turn. P1 stands.
2. Finding 2 holds: average peak colonies, Remastered: Ferengi 14.1, Federation (the AI seat) 10.1, Romulan 6.9, Cardassian 5.2,
   Klingon 5.1. Balance of Power: Ferengi 36.4, Federation 18.6, Romulan 18.4, Cardassian 9.0, Klingon 8.2. P2 stands.
3. Finding 3 is softened but holds for conquest: with the player's seat played, 19 times an empire lost every colony (round 1:
   never, other than the passive player), but no war ever ran on to one survivor, even over 1,200 turns. P3 is the lever for a
   conquest ending; without it, the conquest victory is reachable only by a human player.
4. Finding 4 was mostly the harness: with no passive seat to fall, vendetta was met once in 72 campaigns (Ages, Federation,
   turn 122), and that was a real win. P4: no change proposed.
5. Fixed (above).

### New
- N1. Domination comes late and rarely (3 of 72, turn 311 to 399), and only on the large maps. No proposal: it may be right
  for a 60% rule. Simon's call whether it should come sooner.
- N2. The F5 exit gate ("a full campaign on each victory type without a blocker") is met for diplomatic, scientific, vendetta
  and domination by the AI alone. Conquest needs P3, or a human-played campaign, to show.
