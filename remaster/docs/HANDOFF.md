# BOTF Remastered — parking handoff (28 September 2026)

The work is parked on branch `claude/hopeful-franklin-4psgy7`, draft PR
thabiggdogg420-a11y/botf-remake#1. The original playable beta is untouched in `game/`, `tests/` and `master`.

## Merged and verified (CI green)

| Lane | Content | Docs |
|---|---|---|
| 0 | Fork into `remaster/`, CMake, CI covering GCC, Clang ASan/UBSan, the web build and Playwright | this folder |
| A | Stability fixes, unified tactical combat (ram, cloak ambush, disengage, hail), soak harness, schema 12 | COMBAT-RULES.md |
| B | Diplomacy/intel fixes, AI-initiated diplomacy with personalities, calls to arms, minor secession, integer espionage | DIPLOMACY.md |
| C | Browser client: WASM bridge + Preact modern-LCARS UI | ../web/README.md |
| D | Outposts/starbases, anomalies, event deck, Classic/Remastered presets, scientific + diplomatic victory, chronicle, build templates, schema 13 | GAMEPLAY-REMASTER.md |
| E | Unreal client catch-up and review fixes, `FBotfCampaign` | UNREAL-CLIENT.md |
| G2 | 520 original descriptions (no original-game prose), 30-race minor roster | MINOR-RACES.md |

Orchestrator integration rules: a campaign diplomatic victory needs two-thirds of galactic population, at least two
partners (allied empires or member races) and turn 80 or later. Retreats never enter impassable sectors.

## In flight when parked

Each lane works in a git worktree under `.claude/worktrees/`. The branch is named `worktree-agent-<id>`. Check `git branch --list 'worktree-agent-*'` for commits that are not yet merged.

- **F2 — browser beta.** It continues lane F's WIP (commits e82d9a9, f78aa8d) and adds phase-2 and minor-race UI, a tutorial, settings, and artifact-safe save export.
- **H — core integration.** Allies share supply and sensors, treaties forbid attacks, an explicit call-to-arms refusal, `creditForecast`, ship/task-force range and route queries, per-ship orders in combat rounds, a larger save cap, and an AI-war balance check.
- **I — Unreal round 2.** Adds `newCampaign` with a ruleset choice, and surfaces stations, anomalies, events, victory, chronicle, templates and minors.

If a lane never reported, its worktree may hold uncommitted work. Commit it as WIP before deleting anything.

## Resume checklist

1. Merge each finished `worktree-agent-*` branch.
2. Verify the core: `cmake -S remaster -B build && cmake --build build -j8 && ctest --test-dir build`.
3. Compare GCC and Clang `FINAL_HASH` for `BotfSim --reference --seed 26713 --empires 5 --player 4 --turns 250`.
4. Rebuild the web client and run its tests: `cd remaster/web && ./build-wasm.sh --clean && npm ci && npm test && npm run build && npx playwright test`.
   - Use `--clean`: incremental wasm builds can link stale objects.
5. Republish the web build.
   - Convert `dist/index.html` into an artifact fragment: title, style, body and module script.
   - Publish it to https://claude.ai/artifact/TJQpg1jaVSvoWCmnFrALJS with `capabilities: {downloads: true}`.
6. Compile the Unreal client on Windows (UE 5.8): run `game/Build-Unreal.ps1` from `remaster/`.
   - Nothing in the Unreal client has been compiled by Unreal yet.
   - Lanes E and I list the spots least certain to compile; start there.

## Known open items

- Unreal client: needs a first real compile. The per-screen view/presenter split is only partly done.
- Minor races: signature ships are data only, and independent minors cannot be attacked.
- Balance is provisional.
  - Supreme-era games end by scientific victory around turn 150.
  - An idle player is sometimes conquered before turn 200.
- Art, audio and the 3D battle scene are still interim. The ship roster and tech tree have not been expanded.
