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
- UI: the modern Slate redesign (Barclay, `claude/ui-redesign`, passes 1 and 2) is merged: battle records and
  replay, radar, summary cards, setup galaxy preview, 16:10 scaling fix, showcase shots 25-43. BOTF.UI.Shell passes
  on the merge. Open: the records 3D view is letterboxed, and the diplomacy Events mode is sparse.
- Title backdrop (03ec46e): `ABotfCombatScene::ConfigureTitleScene(EmpireId)` films the empire's three hero hulls over
  the gas giant. The Slate title screen does not call it yet. Hook: `ConfigureTitleScene(E); SetPresentationActive(true)`
  and show `GetBattleTexture()` full-bleed behind the menu. Dev render: `-BotfTitleShot=<png> -BotfTitleEmpire=N`.
- The Unreal Insights 24-v-24 profile has not been done yet. The showcase frame report stands in for it.
- `remaster/godot` is the cloud session's Godot 4.5 client, a deliberate comparison build (see the Godot section below).
- Packaged proof (a0ad5c8, Development): `Package-Windows.ps1 -SkipCoreTests -RunUiTests` packaged in 11 min, and the
  packaged `BOTFRemake.exe -BotfShowcase=<dir> -RenderOffscreen` ran the whole showcase with the generated art cooked
  in (`DirectoriesToAlwaysCook` includes /Game/Generated). 3 turns, 50-ship encounter, save/load at round 7,
  avg 6.29 ms (159 fps), p95 9.27 ms, p99 12.38 ms.

## PC session 2026-09-30: hero ship models, true scale, 3D flight

Simon's calls: true scale with a small-craft floor; 3D flight over the unchanged 2D rules; escorts and smaller fly
in wings; upgrade each model's own textures, plating for untextured ones.

- **Models.** 14 fan-made models were downloaded to the gitignored `LocalAssets/incoming/` (Sketchfab glTF,
  Trekmeshes OBJ/3DS, GameFront Vor'Cha Pack). None are committed. `ArtSource/HeroShips/hero-ships.json` lists each source,
  its axes and canon length. Keys: Sovereign, Galaxy, Defiant, Negh'Var, Klingon BoP, Vor'cha, D'deridex, Valdore,
  Romulan BoP, Galor, Hideki, D'Kora, Ferengi shuttle and pod.
- **Pipeline** (all under `Tools/`):
  - `convert_3ds.py`: 3DS → OBJ.
  - `blender_hero_ships.py`: orient, scale to canon metres, weld and decimate to 160k tris, merge material slots,
    bake BC/N/ORM/E from the source textures, plating UVs for untextured hulls, auto weapon/engine sockets, FBX plus
    `hero.json`, EEVEE previews.
  - `ue_import_hero_ships.py`: `M_ShipHero`, per-slot MIs, 4 LODs, `/Game/Local/Ships/...`, merges
    `LocalAssets/ships/hero-manifest.json` into `resolved-manifest.json`. `Import-Assets.ps1` keeps those entries.
- **Scale.** `BotfAssetCatalog.h` `displayLengthForLength`: 350 m = 120 units, a 60 m floor and a 1000-unit cap.
  Meshes are scaled by their X length.
- **3D flight** (`BotfCombatTimeline.h`):
  - Home heights: capitals stay near the plane, small craft spread ±1.5 cells, and each side sits on its own layer.
  - Per-maneuver climbs with pitch.
  - Bank scaled by class.
  - Finger-four wings for escorts within 2.5 cells: the lowest fleet id leads; wingmen peel off when it dies.
  - `spacingOffsets` pushes overlapping true-scale hulls apart, mostly in height. The scene eases the offset, and
    rules positions are untouched.
- **Proof.**
  - client-logic-tests: 516 checks, 0 failures.
  - UE build: succeeded.
  - `ue_import_hero_ships.py`: 14/14 imported.
  - Showcase 6v6: 180 fps avg, p99 9.6 ms.
  - Shots: `pc-screens/battle-hero-overview.png`, `pc-screens/battle-hero-chase.png`.
- **Battle camera redesign** (Simon: "the battle scene should be zoomable to different ships and locations").
  - `BotfCameraRig.h` holds the orbit rig, framing, projection, picking and easing. It is unit-tested
    (client-logic-tests 535 checks, 0 failures).
  - `ABotfCombatScene` films every mode through it. `STacticalViewport` adds cursor zoom, pan, click-to-select,
    double-click follow and fly-to, and faction markers. Keys: F / G follow ship / target, +/- zoom, Home frames the battle.
  - Arena scale doubled (`CellUnits` 170).
  - Chromatic fringe 0.6 → 0.15; hero `EmissiveStrength` 2.5 → 1.5.
  - The showcase adds three shots: `battle-follow-escort`, `battle-follow-enemy` and `battle-free-zoom`.
  - Proof: UE build succeeded; showcase 6v6 at 170 fps avg, p99 9.6 ms.
  - Screenshots: `pc-screens/battle-camera-overview.png`, `battle-camera-follow.png`, `battle-camera-target.png`.
- **Sister classes and hull look.**
  - `hero-ships.json` `variant` derives a class from its base model: `proportions`, plus `modules` copied from a length
    band, scaled, seated on the hull (`onTop` / `below`) and optionally `mirror`ed.
  - Keldon: a Galor, beamier, with the bow section as a raised dorsal module, and its own paint.
  - Nagus: a D'Kora at 520 m with outboard aft pods and greener bronze.
  - `bcTint` repaints a bake; D'Kora and Nagus are brightened because their source paint is near-black.
  - Paint-derived normals are 2.5x instead of 6x and roughness is at least 0.32: this fixes the hull sparkle.
  - `MetallicScale` is 0.35: painted hulls. The capture has no reflection environment, so metal rendered black.
  - A camera-following fill light (`FillLight`) is added.
  - The title sun is a three-quarter front key (it was backlight).
  - Proof: 16/16 heroes built and imported; titles `pc-screens/title-cardassian.png`, `title-ferengi.png`; showcase 6v6 220 fps.
- **Plated hulls (untextured sources: Negh'Var, Vor'cha, Defiant, Hideki).**
  - `blender_hero_ships.py` `bake_plating` bakes the large-scale look into a vertex colour `Col` with Cycles on the GPU.
    It holds the faction hull colour (`PLATING_BAKE`), a rust underside accent, tone mottling, plate families and AO.
  - A unique-texture bake was dropped: these meshes unwrap into thousands of islands and the atlas used 3–13%.
  - `ue_import_hero_ships.py` `M_ShipPlated` multiplies the vertex colour by the tiling faction plating (panel tone,
    seams, roughness, normal).
  - Two bugs made these hulls black:
    - The bake linked the Mix nodes' float sockets instead of their colour sockets, which averaged 0.04.
    - The material read a VertexColor output named "RGB", which doesn't exist, so the multiply stayed at zero.
  - Headless checks can't see vertex colours: `has_vertex_colors` is always False in the commandlet, which has no render data.
    The title shot now logs `BOTF_TITLE_MESH <key> <path> colours=<n>` from the render data instead.
  - Proof: `pc-screens/title-klingon-plated.png`, where the Negh'Var shows deep olive with rust undersides and AO.
- **Beta milestone** (Simon, 30 Sep: "a playable beta … that matches a pre release AAA quality game beta").
  - **Live title menu.** The main menu now films the hero fleet live behind it: the player's empire during a
    campaign, otherwise a random power. It uses `ConfigureTitleScene` and `SBattleImage` in cover mode, with an
    `SBotfScrim` (a smoothstep ramp in whole-unit strips) so the text stays readable. The camera opens straight on
    the shot, and the fleet flies right of centre, between the title and the menu.
  - **Galaxy map.**
    - The painted panorama (`T_SpaceNebula`) shows behind the chart, and charted sectors are translucent.
    - Stars take a spectral class from the system id and seed (`StarLook`, presentation only).
    - Empires show territory: a wash plus borders. Known colonies claim sectors within 1, or 2 at population ≥ 50,
      and a tie between empires stays contested.
  - **Star names.** `generate()` names systems from real star names (`generatedSystemName`), then Gliese
    designations once those run out. No capital or minor-race homeworld is among them. Saves keep their stored
    names.
  - **Emissive coverage.** `blender_hero_ships.py` records each slot's `eCoverage`, and the importer dims slots
    with more than 5% lit by 0.05/coverage (minimum 0.2). The Galor/Keldon panel map is 46% lit.
  - **Packaging.**
    - `DefaultGame.ini` always cooks `/Game/Local`.
    - `Package-Windows.ps1` stages `LocalAssets/resolved-manifest.json` into the package and marks it
      `localContent` (private use only).
    - The first cook failed on a ShaderCompileWorker crash (0xC0000005) in one `M_ShipHero` permutation.
  - Proof: `pc-screens/title-menu-live.png`, `galaxy-nebula-territory.png`. Core suites pass (112 / 25 / 14 / 34).
    Showcase: 46 shots, no errors.
- **Beta 0.2.0 delivered** (commit `cf1e116`).
  - Package: `game\Build\Packages\0.2.0-cf1e116f78-Development-20260930-103414\Windows\BOTFRemake.exe`.
    It is gitignored and private: it holds local third-party content.
  - `README.txt` inside the package covers how to start, what to try, known issues, and where saves and logs are.
  - Packaged smoke run:
    - The showcase gives 47 shots, 0 errors, about 110 fps (p99 11 ms), with 279 local asset keys.
    - A plain launch reaches the live title menu and runs 40 s cleanly.
    - The "unverified" flag comes only from the engine writing `SecurityToken=` into `DefaultEngine.ini` during
      the cook.
  - Also in this pass:
    - A controls overlay (`?`).
    - The title camera cuts straight to its shot.
    - The new-campaign turn number matches the top bar.
- **Parked suggestion: a second combat engine** (Simon, 30 Sep: "park the 2nd combat engine as a suggestion to
  visit at the end"). Chosen at game start and not a replacement: a New Campaign "Combat" selector (Turn-based /
  Real-time / Real-time + Helm), stored with the campaign.
  1. **Real-time**, moderate, about one to two sessions.
     - The rules stay round-based. Rounds run back to back with no Next Round stop, so ship actions play
       continuously in the 3D scene.
     - Orders apply from the next round boundary (6 s at most). Space pauses; speed runs 1x / 2x / 4x.
     - Optional auto-pause when a ship is lost or a new contact appears.
     - Determinism, mid-battle saves, replays and battle records are unchanged.
     - Fully continuous rules are not recommended: they would mean a new combat model and would break replays
       and saves.
  2. **Helm (Bridge Commander style)**, large, about four to eight sessions plus tuning.
     - Built on the real-time mode: you fly one ship and the AI flies the rest.
     - Needs direct flight (throttle, turn, pitch), chase and bridge cameras, and weapon arcs with per-weapon
       charge and reload.
     - Needs per-facing shields, a power-allocation panel, a real-time dogfight AI and a targeting HUD.
     - The result feeds back into the campaign through the same combat report.
     - Existing parts to reuse: true-scale hero models with weapon/engine sockets, the camera rig, beam and
       torpedo effects, shield and explosion effects.
- **Open.**
  - The Vor'Cha Pack NIFs are unused: no diffuse textures.
  - The Romulan D'deridex paint is very dark green by source.
  - Preview top/front renders crop wide hulls (preview only).
- **Voice work started, then development paused** (Simon, 30 Sep: "when whatever is being done now, finishes,
  pause development for now").
  - Committed: `a9b4856` (old saves adopt real star names; 113 sim tests pass; editor build succeeds) and
    `6882fdd` (voice tools and `docs/VOICE-PLAN.md`).
  - Local only (gitignored `LocalAssets/`):
    - Original BOTF audio, from archive.org items `botf_20240331` and `birthof-fed`:
      `botf-original/audio/{english 1364, music 151 segments, sfx 163}`.
    - `english/transcripts.json`: Whisper small.en over all 1364 voice clips.
    - `voice/subs/{ds9,voy,tos}`: English SubRip text per episode.
  - Found, not yet mined: Bridge Commander crew lines on `H:\Games` (Tactical, Helm, Engineering, Science, XO,
    Korbus); TNG and ENT on `D:\TV Shows` have image subtitles only, so they need Whisper.
  - **Resume at** VOICE-PLAN.md §4 step 1: map the BOTF transcripts to `vo.*` keys. Run heavy jobs at Idle priority
    while Simon plays (Whisper, ffmpeg and builds pushed his CPU to 100%).
  - Waiting on Simon: beta 0.2.0 playtest notes (fix these first), the Ferengi markings call, and the sign-ups in
    PC-QUEUE item 0. Don't merge to main yet.
  - Also queued: more ship models for the classes on generated hulls (downloads pre-approved), the 24-v-24 Insights
    profile (item 9), and the fresh-clone check (item 11, near the end). The second combat engine stays parked.

## Cloud session 2026-11-07: resumed after the pause

Development restarted from `700e1c6` (the 30 Sep pause point) in a fresh cloud container. Every toolchain was
rebuilt from scratch: core CMake, Godot 4.5 with godot-cpp and the Linux extension, emsdk, and the web `npm ci`.

- **Verified green:**
  - Core: 11/11 ctest suites, soak included.
  - Unreal stand-in check: 541 client-logic checks, 0 failures, after the fixes below.
  - Asset keys: 203 required, 279 entries, 0 errors.
  - Web: 20 bridge tests.
  - Godot: core tests 10/10 (MSVC/native and wasm fingerprints agree again), UI 85/0, battle timeline 9/0, battle scene 0 failures.
- **Repaired:**
  - `remaster/godot/docs/screens/*.png` were plain blobs although `.gitattributes` routes PNGs through LFS. They are
    now LFS objects, so a fresh clone no longer shows them as modified.
  - The stand-in check had been red since the 30 Sep 3D-flight work:
    - `ShipTrack::small` → `smallCraft` (Windows `small` macro).
    - Shadowed locals and a lambda named `near` (another Windows macro) in the client-logic tests.
    - PC-QUEUE item 12 asks for a UE rebuild.
  - The browser wasm predated the 30 Sep real-star-names change: rebuilt. The Godot fixtures, which are recorded
    from it, were re-dumped.
- **Godot fixes for the PC smoke-test bugs** (`f1050f3`; the bug list is in the next section):
  - Bugs 2, 4 and 7 shared one cause. The combat screen rebuilt the whole 3D battle view on every refresh (each
    order, pick and round): the camera reset, effects recompiled, and frames spiked. The view is now created once
    and re-parented.
  - The effects also prewarm once per battle.
  - Bug 3: opening rounds frame our formation from behind with the enemy beyond. Between rounds the camera holds
    that wide shot.
  - Bug 5: the galaxy fits the known sectors into the area the inspector and toolbar leave visible.
  - Bug 6: an empty Events window is a compact note.
  - Also new:
    - ships shown at their deployment cells before round 1;
    - a CAMERA mode button over the 3D view;
    - a Starbase drill on the title screen (Remastered rules, the full 3D battle).
  - PC-QUEUE item 13 asks for a PC rebuild and smoke run.
- **Hero ships in Godot, and the live title menu** (PC-QUEUE item 14 regenerates and checks them on the PC):
  - `blender_hero_ships.py --gltf` (opt-in; the FBX path is unchanged) also writes `SM_<slug>.glb` per hero (bakes
    embedded, `Col` as COLOR_0, `SOCKET_*` nodes, LOD1-3) and merges `LocalAssets/ships/godot-hero-manifest.json`:
    glb path, canon length, sockets, LODs and the Unreal importer's material hints (`Tools/hero_gltf_manifest.py`,
    offline test `Tools/test_hero_gltf_manifest.py`).
  - Godot: `BattleAssets` loads those glbs by key; `BattleShip` scales them by canon length and fires from their
    `WPN_*` sockets; `HeroLook` rebuilds M_ShipHero / M_ShipPlated as `hero_hull` / `hero_plated` shaders and sets LOD
    visibility ranges. No model → procedural hull. `tests/test_hero_models.gd` proves it on synthetic glbs. Not
    seen with the real models yet (they are not in the cloud).
  - The title screen films `TitleFleet` live (the battle's environment and ships, no timeline): the campaign's or
    latest save's empire, else a random power, right of centre past a gas giant under a front key, behind a scrim.
    `tests/test_title_fleet.gd`; shot `godot/docs/screens/ui-title.png`.
- **Still waiting on Simon** (unchanged from 30 Sep):
  - beta 0.2.0 playtest notes;
  - the Ferengi markings call;
  - the sign-ups (PC-QUEUE item 0).
- **Voice mapping** (VOICE-PLAN.md §4) needs the local `LocalAssets/` audio on the PC.

## PC session 2026-09-29: Godot (Windows 11, RTX 4090)

The cloud's Godot 4.5 client, built natively on Windows and run for the first time here. The mingw DLL was not needed.

**Build steps that worked**
1. Godot 4.5-stable (standard, not .NET) from the GitHub release into `C:\Tools\Godot`. Only the Windows x86_64/console
   templates from `Godot_v4.5-stable_export_templates.tpz` were unpacked, into `%APPDATA%\Godot\export_templates\4.5.stable\`.
2. `git clone --depth 1 --branch godot-4.5-stable https://github.com/godotengine/godot-cpp` into `core/godot-cpp`
   (then create empty `.gdignore` and `.fetched-godot-4.5-stable` files, as `fetch-godot-cpp.sh` does).
3. This PC has VS **18** (2026) Build Tools, not VS 2022, so the "Visual Studio 17 2022" generator does not exist. Use
   CMake and Ninja from the VS 18 install, in a vcvars64 environment. For each of `template_release` and `template_debug`:
   `call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"`
   `cmake -S . -B build-win-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBOTF_GODOT_TARGET=template_release`
   `cmake --build build-win-release --parallel` (debug target: `build-win-debug`), then add `.gdignore` to each build dir.
   Output: `core/bin/libbotf_core.windows.template_{release,debug}.x86_64.dll`, about 1.9 MB each, which import
   only KERNEL32 (static CRT). The first build compiles 1,037 units.
4. `Godot_v4.5-stable_win64_console.exe --headless --path . --import`, then the tests (below).
5. `... --headless --path . --export-release "Windows Desktop" build/BOTFRemastered.exe`. The preset picks the DLL up
   on its own, and it lands next to the exe.

**Fixes**
- `core/CMakeLists.txt`: the mingw `-static -static-libgcc -static-libstdc++` link flags are now mingw-only
  (`WIN32 AND NOT MSVC`). MSVC gets `/EHsc /utf-8 /bigobj /fp:precise`.
- `scenes/ui/app.gd` `toast()`: the fade timer was a SceneTree timer whose lambda captured the toast panel. When more
  than five toasts piled up (end turn, battle rounds), the overflow `free()` removed the panel first, and each timer
  then logged `Lambda capture at index 0 was freed`: 5 errors per smoke run. The wait now runs on the toast's own tween,
  so it dies with the toast. After the fix, stderr is empty and the UI suite still reports 85/0.
- New `tools/pc_smoke.gd`: a GPU smoke driver in a real window. It uses synthetic F1-F8 and C keys and mouse clicks on
  the real buttons, saves 1920x1080 shots and logs frame rates.
  `Godot_v4.5-stable_win64_console.exe --path . -s res://tools/pc_smoke.gd -- <out_dir> [--novsync]`

**Tests (native MSVC core)**
- `tests/run_tests.gd`: 10 passed, 0 failed. This includes `test_fixture_determinism`: MSVC reproduces the wasm
  fingerprints.
- `test_campaign_ui.gd`: 85 checks, 0 failures. `test_battle_timeline.gd`: 9 passed. `test_battle_scene.gd`: 0 failures.
- GPU boot: Vulkan 1.4 Forward+ on the RTX 4090, `core ok=true, 5 empires`, title screen shown. The exported exe
  opens its window, "BOTF Remastered (Godot)".

**Smoke test (`pc_smoke.gd`, 0 failures)**
- Covered: title → New Campaign (Federation, Remastered) → galaxy; a right-click order menu Set course, clicked; a
  System-screen build queued by clicking a construction choice; F1-F8 each opening its screen; 12 turns ended; the
  Events, Victory and Chronicle overlays; save, relaunch (a fresh client), load (turn and core fingerprint identical);
  the tactical drill with Take command clicked, 5 commanded rounds with 3D playback, C pressed, battle finished and
  turn completed.
- Not done by hand: Simon declined screen control of the game window, so a person has not clicked through the
  exported exe. The driver runs the same project through the editor binary.
- Frame rate, 1920x1080 window:
  - Vsync on (the default; display 120 Hz): 76-90 fps.
  - Uncapped: 85-91 fps in galaxy and battle.
  - An empty Godot 4.5 window on this PC tops out at about 103 fps uncapped, so the game is near the machine's windowed
    ceiling (virtual display drivers are installed). It is not GPU-bound.
  - Battle frame spikes are 40-60 ms.

**Bugs (repro steps)**
1. **Fixed:** toast lambda errors. See Fixes above.
2. **First-battle hitch of 1.35 s.** On a fresh shader cache: Tactical drill → Take command → Resolve round. The
   first round freezes for about 1.35 s (first-use pipeline compiles for the battle effects). Once cached it is 73 ms.
   Suggestion: prewarm the beam, torpedo and explosion materials in `load_encounter` (scenes/battle).
3. **Battle camera too far in the opening rounds.** Tactical drill → Take command → Resolve round x2: ships deploy at
   opposite arena edges and are 10-20 px specks against the nebula (`pc-battle-round2.png`). They only read at the
   end (`pc-battle-end.png`).
4. **Cinematic toggle not visible.** In the same battle, press C: the capture 3 s later is framed like the default
   camera. Check by hand; the synthetic key may not reach the SubViewport.
5. **Galaxy opens with a lot of empty chart.** New campaign (Medium galaxy): the known systems sit in a small patch
   and roughly 70% of the map panel is empty until you pan or zoom. The Unreal client fits the chart between its bars.
6. **Events overlay is a large empty modal** when there are no events (turn 12, Events). Minor.
7. Frame spikes of 40-60 ms during battle rounds (worst-frame column above). Not visible as stutter in the shots;
   worth an Insights-style profile.

**Package**
- `C:\Dev\BOTF-Godot-2026-09-29.zip`: `BOTFRemastered.exe` plus `libbotf_core.windows.template_release.x86_64.dll`.
- Unzipped to `C:\Games\BOTF-Godot\`, with the desktop shortcut "BOTF Remastered (Godot)".
- Logs: `%APPDATA%\Godot\app_userdata\BOTF Remastered (Godot)\logs`.
- Screenshots: `remaster/godot/docs/screens/pc-*.png` (960x540).
