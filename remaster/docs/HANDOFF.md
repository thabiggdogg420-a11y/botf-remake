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
https://claude.ai/artifact/3QvMcKGWN9nzVqkZphDnan.

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
   - Publish it to https://claude.ai/artifact/3QvMcKGWN9nzVqkZphDnan with `capabilities: {downloads: true}`.
6. Compile and package the Unreal client on Windows (UE 5.8): run `game/Package-Windows.ps1` from `remaster/`
   (see PACKAGING.md). On failure, paste `game/Build/Logs/<stamp>/FAILURE-SUMMARY.txt` back into the session.
   - Nothing in the Unreal client has been compiled by Unreal yet.
   - Lanes E and I list the spots least certain to compile; start there.

## Known open items

- Unreal client: needs a first real compile. The per-screen view/presenter split is only partly done.
- Minor races: signature ships are data only, and independent minors cannot be attacked.
- Balance is provisional. AI alliances usually win diplomatically at turn 150; rival AIs are too weak to capture stations.
  - Supreme-era games end by scientific victory around turn 150.
  - An idle player is sometimes conquered before turn 200.
- Art, audio and the 3D battle scene are still interim. The tech tree has not been expanded.
- Ship roster: 98 expanded classes (43 buildable in Remastered; Classic unchanged) with asset keys for a local HD
  manifest. See SHIP-ROSTER.md. The Unreal Object Database lists them (not UE-compiled yet). The web client
  (wasm rebuilt) shows them in the Database, build options and fleets, and its tactical arena draws typed weapon VFX
  with synthesised audio cues behind a `CombatPresentationSource` adapter (web/README.md).
- Local-only asset pipeline (lane J): `../assets/manifest.json` lists candidate HD ships, weapon, sfx and voice sources, with their terms. It is documented in ASSET-SOURCES.md and AUDIO-PLAN.md.
  - `../game/Import-Assets.ps1` resolves and converts manually downloaded archives into the gitignored `game/LocalAssets/`. Blender and ffmpeg handle conversion, and the output is imported into `/Game/Local`.
  - Not yet run on Windows or against a real BOTF `.snd` bank.
  - `UBotfAssetRegistry` (ship meshes by key in the 3D battle) and `UBotfAudioSubsystem` (cues, priority, ducking, volume sliders) consume `resolved-manifest.json`; written and unit-tested outside Unreal, not UE-compiled yet (UNREAL-CLIENT.md, "Local assets and audio").
- Asset keys are unified on the roster scheme (`ship.<fac>.<class>`, `weapon.<Type>[.<fac>]`); `assets/check_asset_keys.py` checks that every roster and legacy key has a manifest entry.

## PC session 2026-09-29 (Windows, UE 5.8.3)

Branch `claude/serene-archimedes-nvtcnw`. First real Unreal build, the packaged game, and the start of the art pass.
Screenshots: `pc-screens/` (800x450).

**What built**
- The first UE 5.8 compile needed two fixes (80eab60): braces around an `if/else` that wraps `UE_LOG` (UE 5.8
  expands it to a block), and a lambda parameter `Tag` renamed `ShipTag` (C4458, a name hiding a member).
- `Package-Windows.ps1` then built and packaged every target. `-RunUiTests` passes (BOTF.UI.Shell).
- The engine keeps adding an Android `SecurityToken=` line to `Config/DefaultEngine.ini`. It is never committed.

**Smoke test, packaged game.** It launches (`BOTF_INTERFACE_READY`) and runs a new Remastered campaign.

**Smoke test, repeatable.** `game/Run-Showcase.ps1 -OutDir <new folder>` drives the editor build offscreen
(`FBotfShowcase`, development builds only). It:
- screenshots title, setup, galaxy, system, research, diplomacy, intel, empire and settings;
- ends 3 turns;
- fights a 24-v-24 rules 3 battle (50 units with stations) in all six camera modes;
- saves and loads mid-battle (round 7 restored);
- writes a frame-time report.

Run9 (editor `-game`, 1080p offscreen): avg 6.05 ms (165 fps), p95 7.78 ms, p99 13.55 ms. The max frame was
309 ms, the first-use shader compile. The script refuses a non-empty folder and never deletes.

**Art (all original and procedural)**
- Rebuilt by `game/Build-GeneratedArt.ps1` into gitignored `ArtSource/Generated` and `Content/Generated`. This
  keeps the repo inside GitHub's 1 GB LFS quota. Only the generators are committed.
- Sky and planets: `generate_space_art.py` (8192x4096 nebula sky, gas giant, moon) and `ue_build_environment.py`.
  The sky dome, gas giant and moon follow the capture camera, lit by a warm sun and a cool rim light, with graded
  post-processing. Shields stay hidden and flare only on impact.
- Ships: `blender_generate_ships.py` makes 159 hulls, one per `ship.*` key, with canon lengths, a vertex mask
  (windows, warm glow, cool glow, accent) and `WPN_*`/`ENG_*` sockets.
  - `ue_import_generated_ships.py` imports them with `M_ShipHull` and one colour scheme per faction. It uses
    Interchange: the legacy FBX importer lost the colour layer on reimport, which made whole hulls glow.
  - `ApplyShipMesh` uses the generated hull whenever there is no local model. Stations use their faction's
    generated starbase or outpost.
- Effects: `ue_build_vfx.py` builds additive `M_FxGlow` for beams, torpedoes and flashes, `M_FxFireball`
  (noise-eroded by `Age`) with a shockwave ring for explosions, and hull-plated debris. Niagara `vfx.*` keys still
  override them.
- Audio: `Import-Assets.ps1 -UnrealImport` imported 121 sounds into `/Game/Local` (gitignored). The 21 mapped cues
  measure -17.8 to -24.2 dB RMS, so no Sound Class or Sound Mix rebalancing is needed (PC-QUEUE item 8).

**Warnings, stubs and honest limits**
- The procedural hulls read as their classes but are blocky next to hand-made models. A local HD model from the
  login-gated sources (PC-QUEUE item 0) overrides them automatically.
- UI: the Slate UI still predates the browser beta's look. A full modern redesign against `remaster/web` is in
  progress on branch `claude/ui-redesign` (worktree `C:\Dev\botf-remake-ui`), to be merged after review.
- The Unreal Insights 24-v-24 profile has not been done yet. The showcase frame report stands in for it.
- A separate session pushed `remaster/godot` (a 4-file Godot skeleton, 87d898e) to this branch during this session.
- Packaged proof (a0ad5c8, Development): `Package-Windows.ps1 -SkipCoreTests -RunUiTests` packaged in 11 min, and the
  packaged `BOTFRemake.exe -BotfShowcase=<dir> -RenderOffscreen` ran the whole showcase with the generated art cooked
  in (`DirectoriesToAlwaysCook` includes /Game/Generated). 3 turns, 50-ship encounter, save/load at round 7,
  avg 6.29 ms (159 fps), p95 9.27 ms, p99 12.38 ms.
