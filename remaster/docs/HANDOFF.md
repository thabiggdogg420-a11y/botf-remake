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
| H | Treaty effects in the turn loop (allied supply, shared sensors, no attacks under treaty, refuse call to arms), creditForecast and range/route queries, per-ship battle orders, 8 MiB saves, schema 14 | GAMEPLAY-REMASTER.md §13 |
| I | Unreal round 2: newCampaign + ruleset choice; stations, anomalies, events, victory, chronicle, templates, minor races in the Slate UI (shim-checked, not UE-compiled) | UNREAL-CLIENT.md |
| F2 | Browser beta: stations, anomalies, events, victory, chronicle, templates, ruleset choice, minor races, tutorial, settings, artifact-safe export; 18 bridge + 23 e2e tests | ../web/README.md |

Integration rules: a campaign diplomatic victory needs two-thirds of galactic population, at least two partners
(allied empires or member races), turn 150 or later, and the claimant must lead its alliance by population. Retreats never enter impassable sectors.

## Status when parked

Nothing is in flight. Every lane is merged. The playable browser beta is published at
https://claude.ai/artifact/TJQpg1jaVSvoWCmnFrALJS.

The Unreal client has passed stand-in compile checks: g++/clang against a Slate shim, plus 194 view-model checks against the real core.
Neither lane E nor lane I has been compiled by UE 5.8. See UNREAL-CLIENT.md for the spots least likely to compile.

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
- Balance is provisional. AI alliances usually win diplomatically at turn 150; rival AIs are too weak to capture stations.
  - Supreme-era games end by scientific victory around turn 150.
  - An idle player is sometimes conquered before turn 200.
- Art, audio and the 3D battle scene are still interim. The ship roster and tech tree have not been expanded.
- Local-only asset pipeline (lane J): `../assets/manifest.json` lists candidate HD ships, weapon, sfx and voice sources, with their terms. It is documented in ASSET-SOURCES.md and AUDIO-PLAN.md.
  - `../game/Import-Assets.ps1` resolves and converts manually downloaded archives into the gitignored `game/LocalAssets/`. Blender and ffmpeg handle conversion, and the output is imported into `/Game/Local`.
  - Not yet run on Windows or against a real BOTF `.snd` bank.
  - No Unreal code consumes `resolved-manifest.json` yet; `UBotfAudioSubsystem` and ship mesh lookup by key are the next step.
