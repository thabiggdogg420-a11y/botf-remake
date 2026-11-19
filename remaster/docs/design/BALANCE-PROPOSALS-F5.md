# F5 balance proposals (for Simon; nothing applied)

Outranked by: Simon's words in `AI_stuff\standing\simon\` and `botf-rulings.md`; docs/design/BALANCE-AI.md for the current values. Balance changes are rule changes, so each line below waits for Simon's yes or no. Nothing in the game has been changed.

Barclay, 20 Nov 2026.

## How the evidence was made
- Tool: `tests/scenarios/AiSoakReport.cpp` (target `BotfAiSoak`, a report, not a suite). For each registered preset it starts a campaign with the preset's own new-game options (lineup, era, size, shape, minor density, difficulty, start systems, threats), leaves the player's seat passive (no orders), and lets the rival AIs play up to 400 turns. Every turn it reads `victoryProgress` for every empire still standing and records the best percent and the first turn each condition was met.
- Run: 9 presets x 8 seeds (7000 + 131k), 72 campaigns. Raw output: `Documents\ChatGPT\f5-balance\r2\<preset>-<seed>.txt`. An earlier run on default galaxy options is in `f5-balance\soak-*.txt`.
- Limit: the player's seat is passive, and a campaign ends when the player is eliminated (the core judges the end from the player's side). 56 of 72 campaigns ended that way, so the AIs rarely got to finish one another. See finding 5.

## Findings
1. **Diplomatic victory comes on the first legal turn, and it is nearly always the Ferengi.** In Remastered the Ferengi won diplomatically at turn 150 and 154 (the rule's earliest turn is 150). On the default-galaxy run it was 5 of 8 Remastered games, every one at turn 150 to 249, median 150. Ultimate: Ferengi, turn 150. Mirror Universe: the Cardassian Alliance, 3 of 8 (median turn 230). No other victory type was reached by any AI in any preset.
2. **Ferengi expansion runs away.** Average peak colonies per AI (round 2): Remastered Ferengi 16.0 against Klingon 7.1, Romulan 9.2, Cardassian 7.6. Balance of Power: Ferengi 29.5 against Klingon 9.4, Romulan 19.5, Cardassian 13.4. In Ultimate the gap is small (9.5 against 7.9 to 8.6).
3. **AIs never eliminate each other.** Across all 72 campaigns no AI empire lost its last colony. Every elimination was the passive player. Conquest and domination were never reached by an AI (best domination: Ferengi, Classic, once, turn 344).
4. **Vendetta reads as met when the passive player falls.** In Ages, Galaxies and Ultimate several AIs "reached" vendetta on the same turn (73, 90, 104 on the default-galaxy run). Their sworn rivals include the passive player, so this is mostly the harness. It may also be real: a vendetta can be settled by someone else's war.
5. **The Dominion War preset runs out of memory (a blocker, not balance).** Seed 7655 (default galaxy) grew past 19 GB during turn 61's advance. Seed 7131 (the preset's own options) passed 4 GB after turn 50 and was stopped. The turn-61 save is `Documents\ChatGPT\f5-balance\soak-dominion-war-7655-turn61.sav`. A player would see the game hang and eat memory at end turn.

## Proposals (each one is Simon's call)
- P1 (finding 1). Make a diplomatic win hold. Option a: the bloc must keep the two-thirds share for 10 consecutive turns. Option b: raise the earliest turn from 150 to 200. Option c: require 3 allied empires or member races, not 2. a is the closest to the original's spirit and stops a turn-150 snap win.
- P2 (finding 2). Find out why the Ferengi out-expand before changing any number: their AI personality, credits or trade income, or minor-race joins. Nothing is proposed until a breakdown run says which. If it is minors, the Ferengi join bonus is the lever.
- P3 (finding 3). Have the AI finish wars. When a rival is down to its last 1 to 3 colonies and the AI is winning, the AI commits to invasion instead of making peace. This is an AI behaviour change, so it is a rule change for Simon.
- P4 (finding 4). None yet. The harness change below shows whether this is real.

## Not rule changes (next steps I can take on Simon's word)
- T1. Fix the Dominion War memory runaway (finding 5). This is a bug, starting from the turn-61 save.
- T2. Add an observer mode to the core for soaks only (the AI also plays the player's seat), so soaks can run to every victory type. It is behind a flag that no game sets, and the player-facing rules are unchanged.
