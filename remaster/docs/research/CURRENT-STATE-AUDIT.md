# BOTF Remastered: current-state audit and Simon's vision

Audit date: 8 Nov 2026. Branch `claude/serene-archimedes-nvtcnw`, HEAD `c8dd750` (Merge R3.5). Read-only: no code changed, no GPU or game window run.
Author: Barclay (read-only audit for the tower).

Sources that outrank this file: Simon's recorded words (section 3) and `docs/BATTLE-DESIGN.md`.

## 0. How this was checked, and what was not

Ran (headless, `--audio-driver Dummy`, Godot 4.5-stable console, native MSVC core DLL built 7 Nov 19:59 from core source last changed 7 Nov 19:58):

| Suite | Result |
|---|---|
| `res://tests/run_tests.gd` (core through the GDExtension) | 11 passed, 0 failed |
| `scenes/ui/tests/test_campaign_ui.gd` | 158 checks, 0 failures |
| `scenes/battle/tests/test_battle_timeline.gd` | 17 passed, 0 failures |
| `test_battle_scene.gd`, `test_hero_models.gd`, `test_title_fleet.gd` | 0 failures each (no counts printed) |
| `test_camera_rig.gd` | 67 checks, 0 failures |
| `scenes/galaxy/tests/test_galaxy_map.gd` | 0 failures |
| `scripts/audio/tests/test_audio.gd` | 181 checks, 0 failures |

Not run: the C++ `ctest` suites (no `cmake` on this PC's PATH; the 11 ctest suites and the soak are the cloud's 7 Nov result in HANDOFF.md, not re-proven here), the web `npm test`, `pc_smoke.gd` (needs a GPU window), and any playthrough. Nothing here is a play-through verdict; "quality" calls come from reading code, the committed screenshots in `godot/docs/screens/`, and Simon's own playtest remarks.

Two things I tripped on, both worth knowing:
- The first test run failed to compile ("Identifier DisplaySettings not declared") because the gitignored `.godot` class cache was stale. A headless `--import` fixed it. Anyone running the suites on a stale checkout will see the same false red. `run-tests.sh` imports first; direct `-s` runs do not.
- My runs left two harmless artefacts: untracked `remaster/godot/scenes/galaxy/tests/test_galaxy_map.gd.uid` (Godot auto-generates it; the other tests have theirs committed) and a gitignored `~libbotf_core.windows.template_debug.x86_64.dll` temp copy in `godot/core/bin/`. I have not deleted either (archive-never-delete). Add the `.uid` or discard it as you prefer.

Layout note: the C++ rules core is not at `remaster/core`. It is header-heavy source at `remaster/game/Source/BotfSim/` (Simulation.cpp 6,255 lines; TacticalCombat.h 2,931; Diplomacy.h 3,618; Intelligence.h 1,565; plus Rules/Stations/GalaxyFeatures/GalacticEvents/BuildTemplates/Victory/Chronicle .cpp), built by `remaster/CMakeLists.txt`. `remaster/godot/core/` is only the GDExtension wrapper (`botf_core.cpp`, 66 lines) over `remaster/web/bridge/WebBridge.cpp` (3,020 lines, 37 bridge methods). The "Unreal" folder `remaster/game` therefore also holds the shared core. Tests are in `remaster/tests/scenarios/` (11 C++ suites, about 12k lines).

## 1. Headline

1. The rules core is the strong part and is far ahead of the game around it. Nearly all 47 core command types are reachable from a Godot screen (all but `RefuseCallToArms`, `CancelFirstConstruction` and `PrioritizeLastConstruction`, whose jobs other commands cover). 36 of the 37 bridge methods are called from GDScript.
2. The Godot client is a complete but plain strategic shell: eight LCARS screens, all drawn in code with no image assets at all. The Godot project contains zero textures, models, audio files, icons or portraits; every piece of art and sound is either procedural (code and shaders) or loaded at run time from the gitignored `remaster/game/LocalAssets/` (1.1 GB, 33 hero glbs, 1,699 audio files, third-party and private).
3. Simon's own verdict on 7 Nov: the 3D battle is "a bit shit", the game "doesnt seem to have caught up to the unreal engine version", and (8 Nov) he thinks the effort is "tunnel visioned on the combat engine". This audit agrees: since 7 Nov roughly six rounds went on battle presentation and sound, and the strategic game has had no new content (no new galaxy options, no tutorial, no planet or portrait art, no difficulty setting).
4. Against the original BOTF and its mods (see sibling file `BOTF-FEATURE-REFERENCE.md`), the clearest missing player-facing things are: galaxy shapes (spiral, ring, irregular), minor-race density and difficulty options, crew experience ranks, race-specific structure ladders as the player sees them, invasion aftermath choices (liberate, subjugate, exterminate), monsters and the Borg as actors, any tutorial, planet art, race portraits and leader screens, and a real campaign balance pass (HANDOFF: "Balance is provisional").
5. Packaging is the weakest finished-looking item: `godot/build/BOTFRemastered.exe` is dated 7 Nov 16:35, before R1, R2, R3 and R3.5 were merged, so the only exported exe does not contain any of the current work.

Quality scale used below: **placeholder** (stands in for real work), **functional** (works, plain), **polished** (would pass a pre-release review).

## 2. State inventory by system

For each system: (a) core, with files and test status; (b) what the Godot client shows the player, by screen; (c) quality.
Test status for core means: the 11 native Godot core tests passed today; C++ suite names are listed but were not re-run (section 0).

### 2.1 Galaxy generation
- (a) `Simulation.cpp` `generate()` (about line 3900): N systems placed on a W×H grid by seeded shuffle, real star names first then Gliese designations, homes and starting fleets, up to 256 systems (`MaxSystems`), map up to 64×64. `Rules.cpp` `newCampaign()` and `placeMinorRaceRoster()`. Remastered adds `GalaxyFeatures.cpp`: nebula clouds, black holes, neutron stars, wormhole pairs, ion storms (they drift), subspace rifts; Classic has none. Tests: `SimulationTests`, `RemasterGameplayTests` (159 checks), `SoakTests`. No shapes: the layout is uniform random scatter. No minor-density, no difficulty, no 3D positions.
- (b) New Campaign screen: five faction cards, size Small 12×9/16 systems, Medium 16×12/28, Large 22×16/48, Huge 30×20/80, number of powers, seed, five starting eras, reference or prototype rules, Classic or Remastered. No galaxy preview, no shape, no density, no difficulty. Galaxy screen (F1): 2D star chart with pan/zoom, fog of war, territory tint, supply range, routes with ETAs, anomaly quads (volumetric shaders for nebula, ion storm, rift, wormhole, black hole).
- (c) Core: functional. UI: functional, the chart is the nicest 2D part. Less variety than the original (which had irregular, spiral and ring galaxies).

### 2.2 Systems and planets
- (a) `Simulation.h/.cpp` `Planet`, `System`, `Colony`; `ReferencePlanets.h` (265 lines). Nine planet classes (M, O, P, L, H, K, Y, gas giant, asteroids), terraforming states and work, capacity, workforce split (food, industry, research), morale, energy, power and fusion plants, shipyards, defence batteries, queue governors (Manual, Balanced, Growth, Research, Manufacturing), queue pinning, build templates (`BuildTemplates.cpp`, 16 per empire). Tests: `SimulationTests` (591 checks), `RemasterGameplayTests`.
- (b) System screen (F2): planets list with an orb per planet, population and capacity, workforce sliders with the core's forecast, governor, energy and shipyard toggle, construction choices, build queue (move, remove, buy, scrap, pin), templates, terraform from the chart's right-click menu with a `canTerraform` preview, megaproject panel. Empire screen (F3): sortable colony and fleet tables, governors for every colony, templates, stations with starbase upgrade, member minor worlds with secession risk, trade routes.
- (c) Core: functional, deep. UI: functional. Planet art is placeholder: `PlanetOrb` in `system_screen.gd` draws a few circles in one flat colour per class. No planet textures, no system view of a star with orbiting planets.

### 2.3 Economy
- (a) `Simulation.cpp` `economy()`, `forecast`/`creditForecast` (the core explains every number), upkeep, trade routes and treaty trade (`TradeRoute`, `ProposeTradeTreaty`), gifts, `BuyConstruction`, `ScrapStructure`, minor-race join bonuses. The audit of 28 Sep rated economy about 35% of the original; many bonuses were then inactive. 318 building records exist in `ReferenceCatalogue.h`, but one builds only what the core's `ConstructionChoice` list offers. Dilithium, per-race structure ladders and emergency programs are not modelled as in the original (grep finds dilithium only as catalogue text).
- (b) Top bar credits and income, System-screen forecast, End-turn check (what the turn will do, what is idle, with dry-run fixes), Trade panel (routes you run, candidates with value and blocker), Empire screen totals.
- (c) Core: functional, thinner than the original. UI: functional.

### 2.4 Research
- (a) Six fields × levels 0–10 (66 named technologies plus level-0 entries; 77 catalogue records) with cost tables in `ReferenceTechnologies.h`, allocation percentages, progress, reserve, research theft (intel), survey and event research grants. Many unlocks exist as ship and building requirements; the 28 Sep audit said "few unlocks". Scientific victory megaproject uses the tech threshold. Tests: `SimulationTests`, `RemasterGameplayTests`, `ContentTests` (77 original descriptions).
- (b) Research screen (F4): sliders that keep 100%, even split, focus, apply and revert, per-field ladder with unlocks and the core's forecast.
- (c) Core: functional. UI: functional. The tech tree has not been expanded beyond the original six by eleven (HANDOFF: "tech tree has not been expanded").

### 2.5 Ships and design
- (a) There is no ship designer in the core, and none in the original (the original had fixed classes upgraded by tech). Data: 125 legacy reference ship records plus 98 expanded roster classes (`ShipRosterData.h`, generated by `tools/ship_roster/gen_ship_roster.py`; 43 buildable in Remastered), each with six tech requirements, weapons mounts and arcs, cost, upkeep, era and source tag; Borg, Dominion, Breen, Hirogen, Kazon, Krenim and others exist as data only. Tests: `ShipRosterTests` (66 checks), `ContentTests`.
- (b) Build choices on the System screen; Database screen (F8): ships with expanded-roster filters (faction, role, era, source), technologies, structures, races, minors. No ship detail with a 3D model viewer. No refit or design screen.
- (c) Data: functional, large. UI: functional (a table). Hulls: see 2.18.

### 2.6 Fleets and supply
- (a) `Fleet`, `TaskForce`, movement, contact, supply ranges (colonies, shipyards, outposts radius 3, starbases radius 5), allied supply under treaty, retreat, stances, outposts and starbases (`Stations.cpp`), terraforming and invasion orders, pathfinding with wormholes and hazards (`GalaxyFeatures.cpp`). Tests: `RemasterGameplayTests`, `SimulationTests`, `IntegrationTests` (101 checks), soak. Known: 28 Sep audit found a turn-171 group-supply blocker in the old tree; the soak in the cloud now passes.
- (b) Galaxy screen: select a sector, right-click order menu (every action dry-run through the core and disabled with the core's own reason: set course, attack, colonize, invade, build outpost, terraform, retreat, form or split a task force, stance), drafted-order counter. Fleets screen (F5): registry grouped by sector, task-force forming, unit detail, send-to with ETA, stance. Supply is a map layer.
- (c) Core: functional. UI: functional. The original's intercept, raid, defend and redeploy orders are not all present; the core has attack, retreat, invade, colonize, terraform, outposts.

### 2.7 Combat
- (a) `TacticalCombat.h` (2,931 lines), one resolver for headless and client play: rules 1 (legacy), 2 and 3 (Remastered flight model, weapon mounts, arcs, ram, cloak ambush, disengage under fire, hail truces, 3+ empires, stations as immobile combatants, per-ship standing orders, battle records and replays, mid-battle save/load). `docs/COMBAT-RULES.md`. Tests: `TacticalCombatTests` (the largest suite; headless fixtures), `CampaignSessionTests` (71), `IntegrationTests`. Rules are round-based; shield facings, subsystem targeting, simultaneous plan/resolve and range bands are proposals 9–12 in `BATTLE-DESIGN.md` awaiting Simon's call.
- (b) Combat screen: forces, ship cards, maneuvers (Assault, Charge, Circle, Evade, Hold, Ram and others) at all/group/ship scope, encounter choice (Take command, Auto-resolve, Hail), Resolve round, combat log, battle archive and replays. The 3D `BattleScene` (battle_scene, battle_timeline 1,005 lines, battle_camera 830, battle_fx 734, battle_ship, shaders): true-scale hulls, 3D flight, camera modes (overview, chase, ship, target, top-down, cinematic), brackets and radar, beams, bolts, torpedoes, shields, hull hits, explosions, cloak, warp-out, a tactical default camera with a director, settings for FPS and render scale. Starbase drill on the title screen. 24v24 smoke at 87–88 fps (HANDOFF, not re-run).
- (c) Rules: functional to polished. Presentation: functional, and unaccepted by Simon. His 7 Nov words (section 3) say scale, torpedoes, spread and the whole "replica of original BOTF combat" approach are wrong. R3.5 pass 3 addressed steady flight, spread, torpedo look, markers and camera but not the plan-phase 3D overlays (firing arcs, range rings, path preview), damage numbers, persistent venting and fire, or formations (BATTLE-DESIGN proposals 5, 7, 8: "Not built"). Explosion fireball mesh, debris chunks, moon, warp-in: "weaker" or "missing" (`GODOT-UNREAL-PARITY.md`). First-battle hitch about 0.3 s remains; a 150–250 ms round-start spike at 24v24 is open.

### 2.8 Diplomacy
- (a) `Diplomacy.h` (3,618 lines) plus `DiplomacyTests` (1,724 lines), `CampaignDiplomacyTests`: five major personalities with acceptance thresholds and scoring, proposals and treaties (non-aggression, friendship, trade, alliance, peace, membership, affiliation), war and betrayal, AI-initiated diplomacy, calls to arms, treaty effects wired into the turn loop (allied supply, shared sensors, no attacks under treaty, refusal), eliminated empires, diplomatic victory predicate. `docs/DIPLOMACY.md`.
- (b) Diplomacy screen (F6): known powers and minors with dossiers and relation memory, incoming proposals with accept/reject/reconsider, treaty builder, statecraft actions, treaty ledger, galactic news, link to trade routes. Diplomacy drill on the title screen.
- (c) Core: functional to polished (the strongest module by test volume). UI: functional. No race portraits or hail screens (a diplomacy conversation is a text panel); the voice lines exist in local audio but no scene shows an alien.

### 2.9 Intel
- (a) `Intelligence.h` (1,565 lines), `IntelligenceTests` (1,041 lines): integer espionage maths, budget split between security and per-rival espionage and sabotage, department focus, theft, AI doctrine, reports. `DIPLOMACY.md` section 9. Two intel systems (legacy and modern) still coexist (28 Sep audit).
- (b) Intel screen (F7): budget sliders with apply/revert, focus, Gather Intelligence, Sabotage, missions and reports.
- (c) Core: functional. UI: functional.

### 2.10 Minors
- (a) `MinorRaces.h` (430 lines): 30-race roster with temperament, tech level, join bonus, affinity to each major, optional signature ship; placement scales with galaxy size (`clamp(systems/5,1,30)`, so 28 systems gives 5); membership, attitude, secession, progression. `docs/MINOR-RACES.md`. Tests: `ContentTests`, `CampaignDiplomacyTests`. Open: signature ships are data only; independent (non-member) minors cannot be attacked (HANDOFF).
- (b) Minors appear in the Diplomacy screen, the Empire screen member list (with secession risk) and the Database. They are not drawn on the chart in any special way beyond a colour/marker.
- (c) Core: functional. UI: functional. No minor-race art or structures reached by the player the way the original's "each minor gives one buildable structure" worked.

### 2.11 Events
- (a) `GalacticEvents.cpp` (613 lines): 14-card curated deck (plague, supernova warning, subspace rift, trade boom, derelict vessel, labor unrest, scientific breakthrough), choices with deadlines, AI decisions; Classic uses the original random morale/credit events. Anomaly surveys. Tests: `RemasterGameplayTests`. Chronicle (`Chronicle.cpp`) classifies every turn line.
- (b) Galaxy HUD alerts, event cards (choice buttons), Turn Briefing/Chronicle overlay, Events window (an empty one is now a compact note), Victory overlay. The ion-storm shader shows blocky artefacts (HANDOFF).
- (c) Core: functional. UI: functional.

### 2.12 Victory
- (a) `Victory.cpp` (297 lines): conquest, domination (60% of systems after turn 20), scientific (six-field threshold plus era-named megaproject), diplomatic (bloc with two-thirds of population from turn 150). `victoryProgress` gives percent and explanation for each. Tests: `RemasterGameplayTests`, soak: 10 of 55 Remastered soak campaigns ended by megaproject. Balance is provisional (AI alliances tend to win diplomatically at turn 150; Supreme-era games end scientifically around turn 150).
- (b) Victory overlay with the four conditions and progress, megaproject panel on the System screen, End screen.
- (c) Core: functional. UI: functional. No end-game speeches (the original disc ones exist in local audio), no cinematic.

### 2.13 AI
- (a) `Simulation.cpp` `planRivals` (line 1321) for strategy; `Diplomacy.h` for personality and war; `Intelligence.h` for espionage doctrine; `Stations.cpp` and `Victory.cpp` for outposts, raids and megaprojects. No player difficulty setting (`difficultyModifier = 0`). Soaks run AI-vs-AI across seeds and eras. Known: rival AIs are too weak to capture stations; an idle player is sometimes conquered before turn 200.
- (b) The player sees AI only through diplomacy, news and contact. No difficulty selector, no AI-personality display beyond dossiers.
- (c) Functional, balance provisional.

### 2.14 Saves
- (a) Schema 14, checksummed, migrations from 4–13 (older saves load as Classic), 8 MiB limit, tactical encounter and `COMBAT_ORDERS` sections, deterministic fingerprints that match between wasm and native MSVC (checked by `test_fixture_determinism`).
- (b) Saves screen (list, name, save, load, delete) over JSON blobs in `user://saves`; one rolling autosave slot per campaign; Continue on the title screen; "Quit to title" with confirm. No thumbnails, no multi-slot autosave history.
- (c) Core: polished. UI: functional.

### 2.15 UI screens (Godot)
Title (live 3D hero fleet behind the menu, Continue, New, Load, drills, Settings, Quit with confirm); New Campaign; Galaxy (F1); System (F2); Empire (F3); Research (F4); Fleets (F5); Diplomacy (F6); Intel (F7); Database (F8); Combat; Saves; overlays (Turn Briefing, event cards, Victory, End screen, Game menu, Settings, Keyboard help, End-turn check). UI suite: 158 checks, 0 failures. Faction LCARS themes for all five empires (`scenes/ui/theme/*.tres`, `lcars.gd` 520 lines) plus vector emblems. Settings: UI scale, colourblind, reduced motion, autosave, confirm end turn, grid, window mode, resolution, vsync, FPS cap, render scale, AA, shadows, five volume sliders. Missing: confirm-or-revert on a display change, a single quality preset, a Galaxy-screen tutorial overlay.
- Quality: functional, consistent, readable; text-and-panel heavy with almost no pictures. Simon on 29 Sep: the UI "should look alot closer to the browser beta I saw yesterday" and the whole UI should be redesigned "more modern and better working for today's tech"; the Godot UI is a port of the web client's screens (each file says "Port of screens/X.tsx"), so it carries that direction but has had no fresh design pass of its own.

### 2.16 Audio
- (a) None in the core. Cue tables and files are client side.
- (b) `scripts/audio/` (R1): cue scheduler with per-key cooldown and a 4-voice salvo cap, five buses with ducking, music on title, galaxy and battle, UI/turn/research/construction cues, advisor and diplomacy and victory voice hooks, volume sliders that persist, generated fallback tones (`botf_tones.gd`). With the private local rebuild (`game/LocalAssets/audio`): 261 cue keys (215 `vo.*`, 25 `mus.*`, 17 `sfx.*`, 4 `weapon.*`) over 1,055 clip assignments, from 1,364 original voice clips, 151 music segments and 163 sfx. 181 audio tests pass.
- (c) Functional, and partly guessed. The SFX and weapon picks were chosen by length and spectrum, not by ear; red alert, torpedo and beam were remapped on 8 Nov and are "still a guess" (`GODOT-UNREAL-PARITY.md`); music tracks 11–15 are unidentified; order-acknowledgement voice is not hooked to the order buttons; the salvo cap drops about 70% of cues in a volley. Simon on 7 Nov: sounds "all working fine", then "there seems to be no sounds for weapons" and the klaxon sounds "like a generic beep boop". A clean checkout without the private files plays only generated tones.

### 2.17 Tutorial and onboarding
- None in Godot (confirmed by grep: no tutorial or onboarding code in `scenes/ui` or `scenes/galaxy`). `GODOT-UNREAL-PARITY.md`: "missing, R5". The web client has a tutorial; Unreal had a partial one. The "First-run tutorial" and "advisor hints" are round R5 and are not started. Missing for the player.

### 2.18 Art and content (counts)

| Item | Count | Where it lives | Quality |
|---|---|---|---|
| Ship/station asset keys with a procedural hull | 159 (`ShipFactory.CANON_LENGTHS`), faction palette and shader | In the repo | Placeholder: primitives and extruded outlines with PBR panels and nav lights |
| Hero 3D hulls (real models) | 33 glb (Federation 13, Klingon 7, Romulan 5, Cardassian 6, Ferengi 2, plus starbases/outposts) from the Starhelm Bridge Commander library | `game/LocalAssets/ships`, gitignored, third-party, private | Unknown: chosen from review renders and loaded by the Godot battle and title (HANDOFF, 7 Nov), but the Godot tests use synthetic glbs and I did not view them |
| Classes with no hero model | about 126 of the 159 keys, including the 27 BOTF-only classes (Liwi, Rtan, Groumall and others); the Ferengi shuttle and pod were dropped | n/a | Procedural (placeholder) |
| Ship classes in the data | 125 legacy + 98 expanded = 223 records | `ReferenceCatalogue.h`, `ShipRosterData.h` | Data complete; text is original remaster prose |
| Technologies | 77 catalogue records (6×11 grid) | core | Original descriptions |
| Buildings | 318 records | core | Original descriptions |
| Minor races | 30 | `MinorRaces.h` | Data only, no art |
| Planet art | 0 images; one coloured circle per class on the System screen; a procedural gas giant backdrop in battle | code | Placeholder |
| Race portraits and leader art | 0. Only 5 vector emblem outlines (`lcars.gd` EMBLEMS). `RaceCatalogue` portrait ids exist in core data but no image behind them | code | Missing |
| Star and galaxy art | procedural shaders (starfield, nebula, volumetric anomaly quads, battle sky) | code | Functional to polished |
| Images in the Godot project | 0 art images (the 30 PNGs are committed documentation screenshots) | n/a | n/a |
| Fonts | 3 (Antonio, Inter, Share Tech Mono), licences included | repo | Polished |
| Audio | none in repo; 1,699 files local (1,536 ogg + 163 wav) | gitignored LocalAssets | see 2.16 |
| Stations | procedural or hero (6 hero starbases/outposts/platform) | as above | Functional |

### 2.19 Packaging
- Godot: `export_presets.cfg` exports `build/BOTFRemastered.exe` (99 MB) plus the release GDExtension DLL beside it. That exe is dated 7 Nov 16:35, so it predates R1 sound, R2 gameplay surface, R3 and R3.5. The preset has no application icon (`application/icon=""`), no installer, no version stamp, and the local audio and models are not packaged (the exe finds them only through `$BOTF_*` variables or sibling `LocalAssets` folders). Multi-slot autosaves, confirm-or-revert and a full-campaign AI-vs-AI soak through to each victory type are Round R6 and not started. `PACKAGING.md` describes the Unreal package only.
- Unreal: packaged beta 0.2.0 exists (30 Sep) but the client is frozen.
- Web: wasm plus TypeScript client stays as the CI/headless check; a browser beta artifact was published in September.
- Quality: placeholder (works as a developer export, not a release).

## 3. Simon's vision, in his own words

Method: Simon's typed messages and queued messages were pulled from the Claude transcripts under `C:\Users\BiggDogg\.claude\projects\C--Users-BiggDogg-AI-stuff\` (session ids in brackets; timestamps are UTC as stored, Sydney is +11 hours in November). `standing\simon\` holds no BOTF-specific rulings (it covers StarhelmBC and WSTC; I searched every file). The Handbook vault page `Projects\BOTF Remake.md` holds only dot's/Codex's records and no Simon quotes. Quotes are verbatim, spelling included. Items marked "(doc)" are Simon quotes reproduced in the repo docs that I could not find in a transcript.

### 3.1 Why the project exists

- 21 Aug 2026 (session 158abf86): "i dont know where youre finding that, i had previously talked about it but didnt do any of the work yet. im wondering if you think it would be easy to remake the game true to the original but in a modern game engine that would possibly fix the issues it had with memory bleed and heavy lag in the late game?"
- 21 Aug 2026 (158abf86): "i want to do both, i want to first recreate botf in a modern engine staying as true to it as possible, and then once thats achieved id likely want to move on to enhancing it with modern features."
- 21 Aug 2026 (158abf86): "first of all, the game has been extensively studied by many, seems silly to waste time redoing that, there would be tonnes of info available about the game engine and mechnics. aim for the original 5 factions, all ships etc. solo hobby project, not meaning to be shared to anyone, no engine preference as i have no idea about that stuff"
- 21 Aug 2026 (158abf86): "bro you have no clue how long things take these days, and cost, i could make this project in less than a weekend by vibe coding it with claude"
- 21 Aug 2026 (158abf86): "begin by doing the research, everything to possibly know about the game and all its modding stuff too"
- 25 Sep 2026 (bd274f06): "Can you look at the project I started a while ago called BOTF remake, and tell me how far along it is, if it's viable to keep developing it etc?"

### 3.2 The bar: original-or-better, then AAA

- 27 Sep 2026, Simon's execution brief (paraphrase in `planning/BETA-ACCEPTANCE.md`, not verbatim): "Preserve working systems; vanilla BOTF is the reference, documented corrections and deliberate modernizations must be identified." And `game/README.md`: "Simon's acceptance standard is original-or-better for every section."
- 27 Sep 2026 (paraphrased in `planning/UI-FIRST-DELIVERY.md`): Simon rejected the old packaged interface; "Modern right-click context menus, pan/zoom and direct double-click navigation are deliberate usability requirements from Simon's feedback." Each beta must be "a self-contained Windows package and ZIP that a reviewer can extract and launch without Unreal Editor or the source tree" ("Simon's explicit requirement").
- 29 Sep 2026 (6fb10e4f): "the last input ill have to you is. make your own creative decisions. make the end result be AAA quality ATLEAST. ship models, sounds, UI, all of the features of the game should be WAAAAYYY better than the current state of the game, and 500x better than the original game."
- 29 Sep 2026 (6fb10e4f): "makw your current milestone, to be a playable beta to give me, that matches a pre release AAA quality game beta"
- 7 Nov 2026 (399fcc84): "Ideally we wil choose if Godot can support the vision for the game without compromising quality that unreal engine might offer, and then push the entire game vision and expansion on that engine?"
- 7 Nov 2026 (399fcc84): "go with godot, and begin its development towards the full finished game", then "go for all of it".

### 3.3 The interface

- 29 Sep 2026 (6fb10e4f): "You need to send someone to do a run over the UI too, it should look alot closer to the browser beta I saw yesterday"
- 29 Sep 2026 (6fb10e4f): "I'd go so far as to suggest you need to redesign the entire games UI relative to the original to make it more modern and better working for today's tech"
- 29 Sep 2026 (6fb10e4f): "how come the build you have open looks so different to the build the cloud session showed me? the UI and everything already got updated but isnt in this build?" (He expects every build he is handed to show the latest work.)

### 3.4 Ships, models and scale

- 29 Sep 2026 (6fb10e4f): "Just go find 3D models you need and tweak them in blender", then "There's tonnes of models made of all of these ships from all sorts of mods and games over the years", then "use the sites to find the models you need, and use the models as a base to tweak and make AAA quality for our game"
- 29 Sep 2026 (6fb10e4f): "make sure the ships all get properly scaled, and the 3d combat engine runs combat in a away that makes sense in a 3D outer space situation, ships move up down left right forward back etc small ships group in wings etc"
- 29 Sep 2026 (6fb10e4f): "regarding ships scale, the battle scene should be zoomable to different ships and locations making the overall scene scale less of an issue, redesign the way the combat occurs if you need to to achieve a AAA quality outcome"
- 7 Nov 2026 (doc, `HANDOFF.md`): "use those, choose the best quality models for each ship needed" (the Bridge Commander library from the Starhelm tools).

### 3.5 Combat

- 30 Sep 2026 (6fb10e4f): "how difficult would it be, to add a secondary combat option that isnt as rigid to being turn based? combat could run real time after the first round of orders? and if that were possible, how difficult would it be to add a mode inside that combat mode, that allows you to take control of a single ship similarly to Bridge Commander and fight the combat in it like that? not as a replacement, to the current turn based combat, but as a selectable option at game start?" Then: "finish the beta, park the 2nd combat engine as a suggestion to visit at the end".
- 7 Nov 2026 (399fcc84, 21:43 UTC): "i played the release you wanted me to play, the sounds are all working fine. but the 3d battling is a bit shit, the ships bob up and down like boats, and theres no weapon textures to fire? the overall game doesnt seem to have caught up to the unreal engine version we had running before this? the settings dont offer any resolution options, FPS caps etc. theres no quit button on main menu?"
- 7 Nov 2026 (399fcc84, 22:05 UTC; `BATTLE-DESIGN.md` dates it 8 Nov local): "scale is off, torpedos are way wrong, the ships look huge compared to the nearby planet, the battle should be a little more spread out. it looks like your trying to replica oringinal BOTF combat, but that was arguably the original games weakest points. it needs to be a modernized, updated, smartly designed 3d turn based space battle."

### 3.6 Sound and voice

- 30 Sep 2026 (6fb10e4f): "while i test play it, start working on a plan to get or synthesize voice lines for the game, there are SOOOOOO many available out there from other games, other mods, tv shows and movies etc"
- 30 Sep 2026 (6fb10e4f), his numbered answers: "1. no, find the files online, you have authority to download whatever you need"; "3. yes, i have the collective works of start trek up to the end of ENT, on 1 of my hard drives"; "5. yes if needed, preferably just find the voice line audio online though"; "14. yes, download whatever you need, dont ask me to approve it".
- 7 Nov 2026 (399fcc84, 21:59 UTC): "i dont know what hes doing, but whover is testing stuff, the game opens and runs sound dcts that shouldnt even be in the game, it sounds like the red alert klaxon has been replacd with a generic beep boop beep boop, there seems to be no sounds for weapons, or anything really"

### 3.7 How he wants the work run

- 29 Sep 2026 (6fb10e4f): "Get back to work. The cloud version of Claude is working on a separate fork now running on Godot that I'll compare to your unreal engine build"
- 29 Sep 2026 (6fb10e4f): "instead of constraining yourself to the way im trying to tell you to do it, think of the best way to use the tools available to you including gemini, to create the 3d models easily and accurately"
- 29 Sep 2026 (6fb10e4f): "i dont want to use my gpu for stuff if i can use existing subscriptions and ai tools?"
- 7 Nov 2026 (399fcc84, 22:03 UTC): "is barclay actually looking at any of the footage from the game when he tests it? because i feel like anyone working on it, that looked at it, would immediately be able t see whats wrong with alot of it"
- 8 Nov 2026 (399fcc84, 00:23 UTC): "Why do you keep pushing it to me to test when it's barely progressed ?"
- 8 Nov 2026 (399fcc84, 00:58 UTC), the brief for this audit: "i feel like you are tunnel visioned on the combat engine. i feel like you need to pull back, refactor, and compare what we currently have against the original BOTF and all its modded features, and plan yourself accordingly to bring our remaster to its proper vision. strip out any restrictions you may have put on yourself relative to timescales, budgets resources etc. assume you have unlimited resources, time and a full team of expert games software engineers, how would that change your plan to bring this to a reality relative to my vision?"
- Standing rules from Simon's memory notes that apply: Godot is the one engine (7 Nov); each round is a big three-lane round that Simon plays (`starhelm-substantial-rounds`); look at the running package before handover (`starhelm-look-before-handover`); no human timescales; game remakes are private, never for distribution.

### 3.8 A scope directive of uncertain authorship

On 10 Oct 2026 a "SYSTEM DIRECTIVE: PROJECT GENESIS (AAA ST:BOTF REMAKE)" was queued into a separate Unity attempt (session 5d82f04e, folder `AI_stuff\BOTF-Remake`, since abandoned for this branch). It was pasted by Simon but its text reads as AI-drafted, so treat it as his chosen scope rather than his words. Its scope lines:
- "incorporate systems and content pioneered in Star Trek: Supremacy, Birth of the Federation II (BOTF2), Ultimate Mod 5 (UM5), and All Ages Mod."
- "Expand beyond the original 5 empires to support selectable era-appropriate majors (Federation, Klingon, Romulan, Cardassian, Ferengi, plus Dominion and Borg mechanics)."
- "Fully implement all canonical minor civilizations (~30+ from original plus extended community rosters like Bajorans, Vulcans, Andorians, Breen, Son'a, Tamarians, etc.) with unique planetary bonuses, special units, diplomatic treaties, and integration mechanics."
- "Support multi-era selection and progression (e.g., Enterprise/Early Warp, TOS/Movie Era, TNG/DS9/Voyager Era, and Post-Nemesis/Late Era)."
- "Multi-tiered intelligence branches (Internal Security, Espionage, Sabotage across specific targets), terraforming stages, trade route networks, and system-level morale mechanics."
- Combat: "Tactical Combat Layer (Modernized 6DoF WEGO)": an order phase with the turn paused, then 10 to 15 seconds of simultaneous execution. This is the shape Simon's 7 Nov "modernized, updated, smartly designed 3d turn based space battle" points at, and it is `BATTLE-DESIGN.md` proposal 11.

### 3.9 Distilled vision (my synthesis, for the planner; each line traces to a quote above)

1. A faithful, modernised Birth of the Federation first (all five factions, all ships, minors, mods' features), then enhancement. Original is the reference, not the ceiling: "original-or-better for every section".
2. AAA presentation across ships, sound, UI and features, with a pre-release-AAA-quality playable beta as the standing milestone.
3. A fully redesigned, modern UI that looks like the browser beta's approach, with direct map interaction.
4. Real 3D hulls (found and tweaked, not hand-built from scratch), true scale, readable at any zoom.
5. Combat is a modern, smartly designed 3D turn-based space battle, not a copy of original BOTF combat; a real-time and a Bridge-Commander-style helm mode are parked as a late option.
6. Original BOTF voice and sound, plus voice lines gathered from other games and shows, found rather than synthesised where possible.
7. Private, solo, never distributed.
8. Never hand him a build that shows no visible progress; look at the running footage before handover.

## 4. Where the current build falls short of that vision

| Vision line | Current state | Gap |
|---|---|---|
| Faithful to BOTF, all mods' features | Core covers the loop and adds Remastered layers; no galaxy shapes, difficulty, density, crew ranks, invasion aftermath choices, monsters or Borg as actors, race structure ladders, emergency programs | Large content and rules gap; `BOTF-FEATURE-REFERENCE.md` lists the original's features to diff against |
| Original-or-better for every section | Economy about 35% and minors about 15% of original per the 28 Sep audit (before Lane G, which raised minors); no section has a recorded parity test | Needs a feature-by-feature parity matrix, not yet written |
| AAA beta | Plain UI, procedural planets, no portraits, procedural hulls for most classes, guessed sound, stale exported exe | Large |
| Modern, redesigned UI | A port of the web client with LCARS theming | No fresh design pass; no images |
| Real hulls | 33 of 159 keys; none verified in a Godot run by a person | R4 not started |
| Modern 3D turn-based battle | Rules unchanged from BOTF-style rounds; presentation retuned over R3 and R3.5 | Proposals 5–12 open; Simon's call on 9–12 |
| Voice and sound | Original voice mapped to 215 keys; SFX picks guessed | Needs Simon's ear; order acknowledgements unhooked |
| Never a build without visible progress | Last exported exe predates the work | Rebuild before any handover |

## 5. Open items inherited (from HANDOFF.md and the parity doc)

First-battle hitch about 0.3 s; 150–250 ms round-start spike at 24v24 (`combat_screen.refresh()` rebuilds 48 cards); `app.start_campaign` ignores the `empire` option; trade routes untested natively (no drill has a visible foreign colony); ion-storm shader artefacts; Unreal environment art not rebuilt; Ferengi markings call and playtest notes on beta 0.2.0 still waiting on Simon; Unreal client frozen; `tools/build_audio_cues.py` lines 90 and 100 still hold the old SFX numbers (the local `cues.json` was edited by hand and is uncommitted).
