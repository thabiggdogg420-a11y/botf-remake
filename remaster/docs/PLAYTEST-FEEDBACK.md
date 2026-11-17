# Playtest feedback (outside players)

## 16 Nov 2026 - Braddles (Simon's friend), browser build v9, ~40 min
Relayed by Simon from WhatsApp.
1. "Was trying to get into fights but couldn't" - "They were all friendly on non aggression pacts". AI too peaceful; no war happened in 40 min.
2. "had to dedicate my entire workforce into food so I couldn't build anything" - not a bug in his view; "maybe a tweak so it doesn't happen at the start of the game"; he had the governor on growth, "and then it grew too quick".
3. "OG BOTF felt easier to navigate because of pictures" - ours is too wordy; "with enough exposure, I'd get used to it". Simon: "typical AI ... making it too wordy".

### Status 16 Nov 2026 (Barclay): items 1 and 2 fixed in the core, merged 81bb279 on claude/serene-archimedes-nvtcnw, pushed
1. AI rivalry layer (docs/DIPLOMACY.md section 3.5). First war on a player who accepts every pact: turn 30-49 across 9 presets x 10 seeds (before: erratic, 19 of 90 runs saw no war or one after turn 60, 33 saw one before turn 30). Pacts still happen (Ferengi and others), not with the warlike. Needs a new web build to reach the browser.
2. Growth and Research governors keep an industry floor; the Growth surplus is capped; the queue governor no longer over-builds farms. Growth governor before: industry 0 from turn 8, build frozen for good. After: never below 12 workers, no stalled build, over 100 turns on every preset.
3. Open: wordiness of the UI.
