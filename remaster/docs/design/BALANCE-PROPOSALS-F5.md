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

## Round 3 (20 Nov 2026): P1-P3 applied on Simon's "Yes"
Same harness as round 2 (observer mode, 9 presets x 8 seeds, 400 turns). Before: `f5-balance\r4-before\` (the code before P1-P3,
identical to round 2). After: `f5-balance\r4-after\`. Breakdown runs for P2: `f5-balance\r4-gains\` (`BOTF_SOAK_GAINS=1`).

### What was changed
- **P1, diplomatic win holds 10 turns.** `Victory.cpp`: the bloc must meet every diplomatic condition for 10 consecutive turns
  (`DiplomaticVictoryHoldTurns`). The count is per empire in `GameState::diplomaticHold`, saved as the optional
  `DIPLOMATIC_HOLD` section (schema 25, written only while a hold runs, so every other save is byte-identical). The victory
  explanation shows the turns held. Test: BalanceTests "P1" (seed 7000: first hold turn 152, the save round-trips mid-hold).
- **P2, the Ferengi lead: cause found, then fixed.** The breakdown (colony gains by source, war turns, what every colony's
  build queue held each turn) showed the Ferengi built 2-3x the colony ships of anyone else, with the same war time. The
  reason: the vanilla race academy (Starfleet Academy, Tactical College, Naval Academy, Central Command; 5,000 industry). The
  AI's one-per-empire limit counted only finished academies and the "already queued" check looked at the one colony, so
  every yard in the empire started the same academy at once: the academy filled 46-63% of all AI colony build slots in
  Remastered. The Ferengi have no academy record, so their yards built colony ships instead. Fix (`Structures.h aiAcademy`):
  one academy project per empire, at its strongest yard. Not a number change; a bug in the AI's build planner.
  BalanceTests' academy check now runs in observer mode, because the passive-seat campaigns it used end at turn 85-110,
  before one 5,000-industry project can finish (with every yard building it, one used to finish by chance).
- **P3, the AI finishes its wars.** `Diplomacy.h finishingWar`: at war, the rival holds 1-3 colonies, the AI holds at least
  twice as many and at least 6, and its fleets are at least 1.5x as strong. Then the AI offers no peace, refuses the
  rival's plea (-200 on the evaluation), and its task forces count the rival's worlds 8 squares nearer when picking a
  target. Test: DiplomacyTests (no offer, refusal, and the old behaviour back at 4 rival colonies).

### Endings, before and after (72 campaigns, 400 turns)
| | Before | After |
|---|---|---|
| Diplomatic | 27 (20 on turn 150, 1 on 151) | 25 (9 on turn 159, the first legal turn now; median 179) |
| Domination | 3 (turn 311, 395, 399) | 3 (turn 291, 299, 341) |
| Scientific | 1 (turn 391) | 1 (turn 331) |
| Vendetta | 1 (turn 122) | 2 (turn 114, 132) |
| Conquest | 0 | **0** |
| Turn cap | 40 | 41 |
| Diplomatic winners | Ferengi 13, Federation 11, Cardassian, Klingon, Romulan 1 each | Federation 15, Ferengi 9, Romulan 1 |
| Empires eliminated | 18 | 25 |

Average peak colonies: Remastered Ferengi 14.1 -> 15.2, Federation 10.1 -> 14.2, Romulan 6.9 -> 7.2, Cardassian 5.2 -> 7.0,
Klingon 5.1 -> 7.5. Balance of Power Ferengi 36.4 -> 31.2, Federation 18.6 -> 19.2, Romulan 18.4 -> 19.2, Cardassian 9.0 ->
13.0, Klingon 8.2 -> 10.9. At turn 150 in Balance of Power the Ferengi no longer lead (Federation 14.2, Romulan 14.2, Ferengi
12.8); they pull ahead later, when the warlike empires are fighting each other and the Ferengi are left alone (lowest losses
in every run).

### What it says
- P1 works: no win on the first legal turn by a bloc that cannot hold it; the spread of diplomatic wins runs from turn 159 to 364.
- P2 works for the cause found; what remains of the Ferengi lead is their peaceful personality (fewest losses), not a bug.
- **Conquest still does not happen between AIs.** P3 raised eliminations by 39% (18 to 25), but no campaign ran down to one
  empire in 400 turns. A conquest ending needs every rival gone; the AIs finish the nearly beaten, but the strong ones still
  balance each other until the cap or a diplomatic win. The conquest victory remains reachable by a human player. Whether
  the AI should be pushed further (for example, peace terms that hand over worlds, or alliances breaking late) is Simon's call.

## Round 4 (21 Nov 2026): why no AI ever wins by conquest
Barclay. Observer soaks, the same harness with a new report switch (`BOTF_SOAK_WARS=<n>`: every n turns, who is left, their
colonies and ships, and for each surviving pair: at war and since when, turns at war, worlds taken from each other, active
treaties; plus transport-turns spent idle at an enemy world). Raw output: `Documents\ChatGPT\r6-conquest\` (`classic-1200`,
`kcw-1200`, `remastered-400`, `bop-800` before; `after\`, `after2\` after the AI fix; `exp*.txt` the what-if runs below, built
from a scratch copy of the core that is not in the game).

### What the soaks show (code as of 8b02dc3, P1-P3 in)
Classic and Klingon Civil War for 1,200 turns on 4 seeds, Remastered 400 x 8, Balance of Power 800 x 4. No conquest, and no
campaign ever came down to one empire.
1. **Alliances never end.** Every Full Alliance between AIs is indefinite, and none was broken in 1,200 turns. In Classic, 2 to
   4 alliance pairs formed between turn 30 and 120 and were all still standing at turn 1,200 (Federation-Romulan from turn 66,
   Federation-Ferengi from 35, and so on). Allies never fight, so a bloc of two or three allies is a floor under the number of
   empires left. Klingon Civil War forms no alliances, and there every pair is at war for most of 1,200 turns; it still ends
   with four empires.
2. **AI fleets do not grow with the empire.** The AI build planner keeps at most 5 warships and 1 troop transport at war (2
   and 2 for a minor-race campaign, a peacetime standing fleet by personality), whatever its size. A 25-colony Ferengi
   Alliance fields 7 ships at turn 1,200, the same as at turn 300. Total ships in a five-empire galaxy stay at 20-30 for the
   whole game.
3. **The AI never lands on a big world.** A transport lands only when its troops are at least 7/10 of the world's ground
   defence as the AI reckons it (8 + hull 14 + 2 x weapons tech, against 8 + population/3 + morale/10 + batteries, times the
   shield grid). A homeworld of 120+ population is out of reach for any single transport at any tech level, and the AI never
   sends two together. The last world of a beaten empire is nearly always its homeworld, so empires sit on one colony for
   hundreds of turns at war with a far larger neighbour (Klingon Empire, 1 colony, at war with all four others from turn
   ~1,000 to 1,200, seed 7000).
4. **Domination comes first.** Colonies fill the galaxy (50 of 50 systems colonised by turn 300-600 in every Classic run).
   Domination needs 60% of the systems; conquest needs every colony. Whoever holds every colony crossed 60% on the way, so
   in a full galaxy conquest can only be the ending if the last rival falls on the same turn the winner first reaches 60%.
   This holds for a human player as much as for the AI.

### Looking for an AI bug: none found that blocks conquest
The one candidate was the transport. The AI sends it to the nearest enemy world; when its own 7/10 test refuses the landing it
waits there, and with one transport allowed at a time it builds no other. I changed the planner so that a transport (and a task
force carrying troops) went only for worlds it would assault, and soaked it (`r6-conquest\after\`, `after2\`). The result was
no better for conquest: the same number of empires fell (2 in 4 Classic campaigns before and after, none in Klingon Civil
War), one each way (seed 7000 now finished a one-colony Klingon Empire; seed 7262 now let one survive at one or two colonies
from turn 700 to 1,200, where the old code finished it by turn 900), and 4 of 16 endings in Remastered and Ultimate moved
(Ages none). The waiting transport is in effect a siege: it lands the turn a starved or bombarded world's defence drops under the
line. So the waiting is
behaviour, not a bug, and the change was taken back; the game's AI is as it was at 8b02dc3. I found no other fault in the war code
(read: the war build order, task-force targeting with P3's pull, the transport test, invasion resolution; not traced turn by
turn: war declarations). Every reason above is a rule or a tuning number, so each one is put to Simon below.

### What-if runs (scratch copy, not in the game; to size the rule levers)
Classic and Klingon Civil War, 1,200 turns, 4 seeds each, domination switched off so that only conquest can end a campaign
between AIs that are not diplomats:
| Run | Change | Conquests | Empires left at 1,200 (Classic) |
|---|---|---|---|
| A | none | 0 / 8 | 4-5 |
| B | AI fleet caps scale with colonies (+1 warship per 2 colonies, +1 transport per 6) | 0 / 8 | 2-5 (seed 7000: Cardassian and Ferengi, allied since turn 57) |
| C | AI alliances last 100 turns (renewable) | 0 / 8 | 4-5 |
| D | B + C | 0 / 8 | 3-5; last worlds of 1 colony left standing |
| E | D + the transport change above | 0 / 4 Classic (Klingon Civil War stopped early, 2 of 4 ran: 0) | 3-4 |
| F | E + AI transports land at any odds (as a player may) | 0 / 4 Classic (Klingon Civil War stopped early: 0) | 2-4 (seed 7262: Federation and Ferengi, allied since turn 1,183) |
Every lever moves the galaxy towards fewer empires, but none alone or together produced a conquest in 1,200 turns: the last
two or three powers end up allied (C3 below), and with alliances lapsing they re-ally within a few turns.

### Proposals (each is Simon's call; none applied)
- C1. The AI may land on a world it cannot take with one transport: either several transports land together (their troops
  added), or the AI accepts worse odds when finishing a war (P3's "finishing" test). Without this no AI ever takes a capital.
- C2. AI war fleets scale with the empire (for example, at war up to 3 warships plus 1 per 2 colonies, and 1 transport per 6
  colonies, inside the existing 256-ship galaxy cap). Today it is 5 and 1 for every empire.
- C3. Alliances between AIs lapse: a Full Alliance runs 100 turns and is renewed only while both still want it, and an alliance
  of the last two powers left ends (the bloc has nobody left to stand against).
- C4. Conquest and domination. Either leave it (conquest is a win only in a galaxy with room to spare, which is how the rule
  reads today), or let a campaign go on past domination when the winner chooses to (the original's "continue playing"), or
  count domination only after a set turn. This one is about the rule itself, not the AI.

## Round 5 (21 Nov 2026, Barclay): conquest after C1-C4
Simon's C1-C4 are in (4e016af). The question: why do observer soaks still give 0 AI conquest wins in 1,200 turns? Evidence in
`Documents\ChatGPT\rc-captures\soak` (the RC run), `soak2` (diagnosis), `soak3` (fixes 1-2), `soak4` (fixes 1-3). The soak report
(`tests/scenarios/AiSoakReport.cpp`, BOTF_SOAK_WARS) now also prints STUCK lines (transports sitting at a hostile world: troops,
the world's defence, what guards it), LAST lines (an empire's last two worlds and what each enemy has against them) and REVIVE
lines (an empire back from 0 colonies).

### Causes found
1. Without play-on, domination always comes first. Conquest needs every colony; domination needs 60% of the systems; colonies
   fill the galaxy by turn ~300, so the winner passes 60% long before it can hold 100%. An AI conquest is possible only in a
   campaign where domination does not end the game (play-on; BOTF_SOAK_PLAYON simulates it). This is the rule as written (C4 left
   it), not the AI.
2. Bug (fixed): a beaten AI came back. An empire with no worlds still planned: its leftover colony ship, transport or a pending
   membership offer took a world, often a seceded minor world, so the "eliminated" empire held a colony again. Classic play-on,
   4 seeds: 56 revivals (Klingons at Antede 30 times between turns 761 and 1101). Now an AI with no worlds settles nothing, lands
   nothing and makes no minor offers; its ships keep their last orders. After: 0 revivals in every soak.
3. Bug (fixed): transports parked for good at a world guarded by a minor's signature ship. A guard blocks any landing while it
   stands and mends a tenth of its hull and shields every turn; the AI kept sending groups whose attack was below that. Classic
   seed 7393 (turn 1100): nine Ferengi warships, 45 attack a turn, against a Vulcan guard mending 49, with three transports
   (171 troops against a defence of 52), for hundreds of turns. Now such a world is not a target for a group (or transport) that
   cannot break its guard, and transports there go elsewhere. Parked transport-turns at turn 1,200 (play-on): Classic 591 / 1027
   / 85 / 1219 -> 297 / 187 / 79 / 180; Klingon Civil War 228 / 967 / 927 / 994 -> 101 / 325 / 623 / 321 (seeds 7000-7393).
4. C3 consistency (fixed): two AIs that are the last powers left kept proposing a Full Alliance that C3 ended the next turn;
   the AI no longer proposes it.
5. Not a bug, rule territory: AI alliances last for ever. C3 renews an AI alliance every 100 turns while both still want it, and
   they do: Federation-Ferengi from turn 36 to 1,200 in Classic 7131, Cardassian-Ferengi, Romulan-Ferengi and
   Federation-Ferengi all from turns 35-57 in Classic 7000. Nobody can conquer a galaxy in which a third of it is allied to
   everyone. And once only two remain, any treaty still binds (non-aggression, friendship, affiliation), the Federation and the
   Ferengi never start a war, and the Romulans break one only at 3:1 with trust under 20.
6. Not a bug, tuning: wars between equals take about one world per 50 turns (Classic 7393 with the endgame experiment below:
   Romulans net +8 worlds in 485 turns of war), so even an open two-power war needs well over 1,000 turns to finish.

### Soak endings (1,200 turns, 4 seeds each; observer mode)
| Run | Classic | Klingon Civil War |
|---|---|---|
| RC (before), normal | domination 2, cap 2 | diplomatic 1, domination 2, cap 1 |
| RC (before), play-on | cap 4 | diplomatic 1, cap 3 |
| fixes 1-2, play-on | cap 4 | diplomatic 2, cap 2 |
| fixes 1-3, play-on | cap 4 | diplomatic 4 (Federation 3, Cardassian 1; turns 376-1143) |
| fixes 1-3 + endgame experiment, play-on | cap 4 | diplomatic 4 |
Conquest: 0 in every run. "Endgame experiment" (BOTF_SOAK_ENDGAME_WAR, soak only, not a rule): when two AI powers are the last
left, their treaties lapse and the stronger declares war. Only one of 8 Classic runs reached two powers inside 1,200 turns.

### Proposals (each is Simon's call; none applied)
- C5. AI alliances lapse for good at 100 turns (no renewal between AIs), or renewal only while a common enemy holds more worlds
  than either partner. Fixes the "allied to everyone" Ferengi bloc (cause 5).
- C6. The last two powers: treaties between two AIs that are the last left end (as C3 does for alliances), and the stronger
  goes to war whatever its personality.
- C7. Victory setting at New campaign: domination on or off (the original's choice of victory conditions). With it off, an AI
  can win by conquest in a normal campaign; today only a human's play-on allows it (cause 1).
- C8. Faster wars between equals (for example a transport's troops +50%, or a world's defence from population /4 instead of /3),
  sized by soak once C5-C7 are decided (cause 6).

## Round 6 (21 Nov 2026, Barclay): C5-C8 applied on Simon's delegation
Simon: "You choose whatever makes the most sense" (rulings 21 Nov, standing\simon\botf-rulings.md). Applied in 1ad9cdb:
- C5. AI-AI alliances are proposed and renewed (100-turn review) only while a third surviving major at war with one of them
  holds more worlds than either partner (`aiCommonEnemy`). Human alliances unchanged.
- C6. Last two powers, both AI: no new proposals; the stronger ends its treaties and declares war (was the soak-only experiment).
- C7. New Campaign setting "Domination victory" (default on), saved as NO_DOMINATION, shown on the end-screen goals,
  passed by LAN and hotseat. Off: domination never ends the game; conquest is the military victory.
- C8. Lever: a world's invasion defence uses population / 5 (was / 3), `InvasionPopDefenseDivisor`. Chosen over transport
  troops because it also helps the AI's own joint-landing estimate and leaves troop costs untouched.

Soaks, domination off, play-on, 4 seeds each, 1,200 turns:
| Run | Classic | KCW |
|---|---|---|
| C5 (renewal only) - C7, divisor 3 | cap 4 | diplomatic 4 |
| + divisor 5 | cap 4 | cap 2, diplomatic 2 |
| + divisor 10 | cap 4 | cap 1, diplomatic 3 |
| C5 proposal gate + divisor 3 | cap 4 | diplomatic 4 |
| C5 proposal gate + divisor 5 (chosen) | cap 3, conquest 1 (Ferengi, seed 7131, turn 1086) | cap 2, diplomatic 1, vendetta 1 |
Domination on, chosen settings, Classic: cap 3, domination 1 (Ferengi, turn 537). No win before turn 200 in any run.
Divisor 10 bought nothing over 5, so 5 stands. Target "some AI conquest wins" is met only thinly (1 of 8).

Why so few: AI-AI alliances had been lapsing and re-signing at once (hence the proposal gate). Treaties below alliance
(non-aggression, friendship, affiliation) still hold peace among three or more powers for hundreds of turns.
### Proposal C9 (Simon's call; not applied)
- Non-aggression, friendship and affiliation between two AIs also end at a fixed term (e.g. 100 turns) unless C5's common-enemy
  test holds, so long-lived AI blocs break up and the strong can turn on the weak.

## Round 7 (21 Nov 2026, Barclay): C9 applied, conquest still rare
C9 (Simon's delegation, rulings 21 Nov): AI-AI non-aggression, friendship and affiliation lapse at their 100-turn review unless
the C5 common-enemy test holds. Player treaties untouched. Test: DiplomacyTests `testC9LowerTreatiesLapse`.

Soaks, domination off, play-on, 4 seeds, 1,200 turns (C5-C8 + C9):
| Run | Classic | KCW |
|---|---|---|
| Round 6 (C5-C8) | cap 3, conquest 1 (t1086) | cap 2, diplomatic 1, vendetta 1 |
| + C9 (applied) | cap 4 | cap 2, diplomatic 1 (t636), vendetta 1 (t477) |
| + C9 + war-focus experiment (soak only, removed) | cap 3, conquest 1 (Ferengi, t807) | cap 3, diplomatic 1 (t760) |
| + C9 + siege experiment (soak only, removed): undefended world under enemy warships loses 5% pop a turn | cap 3, conquest 1 (Romulan, t509) | cap 4 |
War focus: an AI at war with two or more majors offered peace to all but its weakest. Siege: an undefended world under enemy warships loses 5% of its population a turn. Each was
soaked as a switch the game never sets, then removed (the siege patch is kept at rc-captures\soak4\siege-experiment.patch).
Neither one moves conquest off 1 in 8.

What the soaks show. Wars are no longer rare: with C9, Classic seeds end with 1-6 pairs at war and 33-75 worlds taken; KCW 94-207.
The galaxy also narrows: by turn 1,200, three of four Classic seeds were down to 2-4 powers. The block is the last few worlds.
Seed 7262 had two powers left from about turn 1,000, Federation 46 worlds to Ferengi 4. Under C6 they were at war, and at turn
1,200 it stood at 47 to 3: about one world per 100 turns, with the Ferengi keeping 7 fleets at home. Seed 7131: 39 to 11.
Defence is not the limit either: divisor 10 bought nothing over 5 (Round 6). Idle transports are rare (191-224
transport-turns at war across 1,200 turns).

### Target corrected (coordinator, 22 Nov 2026)
In the original BOTF and in other 4X games, AI conquest wins are rare, and campaigns end in 200-500 turns. The AI conquest-win
rate is no longer a target, and nothing beyond C9 is proposed. C10, drafted earlier this round, is withdrawn. Round 7 measures
instead (a) how fast a strong, aggressive player wins by conquest and (b) whether the AI galaxy keeps moving.

#### (b) Does the AI galaxy keep moving? (AI only, C5-C9, domination off, 1,200-turn cap)
| Seed | Classic: worlds changing hands per 100 turns | Classic: longest run with no AI-AI war | KCW: per 100 turns | KCW: longest no-war run |
|---|---|---|---|---|
| 7000 | 4.9 | 31 turns (turns 1-31) | 10.9 (vendetta win t477) | 29 (turns 1-29) |
| 7131 | 10.1 | 31 (1-31) | 17.3 | 31 (1-31) |
| 7262 | 7.5 | 29 (1-29) | 14.5 | 29 (1-29) |
| 7393 | 6.1 | 33 (1-33) | 14.8 (diplomatic win t636) | 33 (1-33) |
The galaxy never settles into lasting peace. The only war-free stretch is the opening 29-33 turns, before first contact;
after that some pair of AIs is at war on every turn. Worlds change hands at 5-10 per 100 turns in Classic and 11-17 in KCW.
(BotfAiSoak CHURN line, files rc-captures\soak4\r7-churn-*.txt.)

#### (a) How fast does a strong, aggressive player win by conquest? (domination off)
Stand-in: the observer AI seated on the Klingon Empire (the aggressive personality) as the player seat (BOTF_SOAK_PLAYER=2),
Classic, 4 seeds, 1,200-turn cap. Rival handicaps were added to stand in for a player who outplays the AI.
| Galaxy, rival difficulty | Conquest wins | Player seat at the end |
|---|---|---|
| Medium, Normal | 0 of 4 | eliminated in 3 (turns 239, 388, 1014); 10 worlds in the 4th |
| Large, Normal | 0 of 4 | eliminated in 4 (turns 193, 513, 672, 836) |
| Medium, Easy (rivals 90%) | 0 of 4 | eliminated in 3 (107, 697, 987); 10 worlds in the 4th |
| Large, Easy | 0 of 1 (run stopped) | eliminated turn 703 |
| Medium, Simple (rivals 75%) | 0 of 4 | eliminated in 1 (227); 17, 6 and 7 worlds in the rest |
Reading. This stand-in does not answer (a). The AI playing the player seat is not a strong player: it loses even to
handicapped rivals, and it never wins by conquest inside 1,200 turns on any size. That puts it far outside 200-500, but the
measurement is of the AI, not of a strong human-style player. No rule is sized from it.
To answer (a) properly the next step is a measurement tool, not a rule: a scripted aggressive player for BotfAiSoak. It
would expand fast, build warships and troop transports from turn ~40, gather one strike force, beat the nearest rival's
fleet, then invade its worlds in turn, and keep peace with everyone else. Its conquest turn on medium and large is the
number to set against 200-500. If that comes out far outside the range, the sized proposal follows from it.
