# M0 Foundation: lane briefs

Simon, 8 Nov 2026: "yes start M0, all decisions yes", with at most 6 lanes. Read MASTER-PLAN.md first, §4 and §5. Its §7 records the decisions:
- WEGO combat with shield facings and subsystem targeting;
- a 3D galaxy map;
- a playable Dominion;
- private multiplayer;
- his Trek library as a voice source.

The base is branch `claude/serene-archimedes-nvtcnw` at f2581a7 or later. The repo is `C:\Users\BiggDogg\Documents\ChatGPT\Game Remakes`. The rules core is `remaster/game/Source/BotfSim`; there is no `remaster/core`.

## Browser target (Simon, 8 Nov: "it all needs to be planned around being playable in a browser too")

The game ships two ways from one Godot project: a Windows exe, and a web export served locally, never hosted publicly. That has six consequences:
- **Renderer.** Desktop uses Forward+. Web uses Compatibility (WebGL2). Every shader, VFX and material must work in Compatibility, or carry a declared fallback. Quality tiers are set in project settings and must not be hard-coded per scene.
- **Native core.** The GDExtension core is built to wasm with Emscripten using Godot's dlink web template, alongside the MSVC DLL. Code must not assume threads unless the build is threaded, which needs COOP/COEP headers from the local server.
- **No disk paths at runtime.** Assets, data and audio load through `res://` packs or a web-fetchable asset bundle. Nothing reaches `LocalAssets` or uses `$BOTF_*` env-var paths at runtime: those become a pack-building step.
- **Budgets.** Each asset key has a web size budget: texture size, LOD count, compressed audio format. The art bible sets them.
- **Saves.** Saves go to `user://`, which maps to IndexedDB on web. Settings must not offer options the web build can't honour.
- **Verification.** The verify pipeline exports and tests both targets, and captures screenshots from both.
- Lanes that write code work in their own worktree: `git worktree add "C:\Users\BiggDogg\Documents\ChatGPT\wt-<lane>" -b m0/<lane> claude/serene-archimedes-nvtcnw`. Copy in the DLLs from `remaster\godot\core\bin\*.dll`. The full setup is in GODOT-ROUND-BRIEF.md.
- Docs-only lanes write directly to the paths they own, in the main checkout.
- Stay inside the files you own. If you need a file outside them, stop and report.
- Run every Godot process with `--audio-driver Dummy`. Use the GPU only for the captures your lane needs.
- Nobody audits their own work. The orchestrator reviews; Odo verifies at the end of M0.
- A doc is done when it is complete and specific: no "TBD", and every claim carries a source tag or a file:line.
- Code is done when all suites pass and pc_smoke has 0 failures.
- Never commit LocalAssets or third-party files. The game is private.
- Commit on your branch, ending each message with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not push and do not merge.
- Final report: under 400 words. Cover what is done, with evidence; what is not done; and every file changed.

## L1 Parity matrix (Tuvok)
You own `docs/research/PARITY-MATRIX.md` and `docs/research/VANILLA-DATA.md`.
- Build a row for every feature in BOTF-FEATURE-REFERENCE.md, plus every feature the core has that the original lacks. Each row gives:
  - the original's behaviour;
  - the mod additions;
  - our core today (file:line);
  - our Godot UI today (screen);
  - the target (faithful, corrected or modernised);
  - the workstream number (1-12 from MASTER-PLAN §5);
  - the milestone (M1-M4).
- Close the research gaps from the core's own original-data tables: `ReferenceTechnologies.h`, `ReferenceCatalogue.h` and anything else that parses the original files. Recover the unlocks per tech level and the victory thresholds into VANILLA-DATA.md, citing the source line for each.

## L2 Design docs (Tuvok)
You own `docs/design/*.md`: one doc per workstream, 12 in all, each one to three pages.
- Every doc uses the same template:
  - reference behaviour (BOTF and mods);
  - modern target;
  - player-facing surfaces (screens and scenes);
  - core changes, naming the files;
  - client changes;
  - content and art needs, by asset key;
  - acceptance tests, as a list a QA tester can play through;
  - the M1 vertical-slice subset.
- Fold Simon's decisions in: WEGO combat, the 3D galaxy, the Dominion, multiplayer, voice.
- BATTLE-DESIGN.md is the input for the combat doc.

## L3 Art bible and asset manifest (barclay, sonnet)
You own:
- `docs/ART-BIBLE.md`
- `remaster/assets/manifest.json` (new)
- `remaster/tools/asset_status.py`
- `remaster/tools/turntable/` (new)

Your work:
- **Art bible.** Cover:
  - scale rules (true lengths, and planet against ship);
  - palette per empire;
  - lighting rig;
  - LCARS UI style;
  - VFX language;
  - portrait style;
  - planet texture spec;
  - rules for the original BOTF art;
  - the review method, judged frame by frame.
- **Manifest.** Make one row for every asset key the game uses: 159 ship keys, planet classes, portraits for majors and minors, structure icons, VFX, music, sfx and voice categories. Each row carries:
  - source;
  - status (procedural, placeholder or final);
  - owner workstream;
  - milestone.
- **asset_status.py.** It prints counts by status and category.
- **Turntable tool.** It renders any ship or planet key to a contact sheet: Godot, Dummy audio, 8 angles.

## L4 Client architecture refactor (barclay, opus)
You own `remaster/godot/scenes/ui/**`, `remaster/godot/scripts/app*` and a new `remaster/godot/scripts/viewmodels/`. Use worktree `wt-m0-arch`.
- Split the screens into three layers:
  1. view models, one per screen, which read the core and expose plain data and signals;
  2. presentation;
  3. a widget library implementing the LCARS design system.
- Retire the "Port of screens/X.tsx" structure. Behaviour must not change: the UI suite must stay green, and you add view-model unit tests.
- Fix the bug where `app.start_campaign` ignores the empire option. Add a test for it.
- Write `docs/CLIENT-ARCHITECTURE.md`.

## L5 Data-driven content (barclay, opus)
You own:
- the content loading inside `remaster/game/Source/BotfSim`, meaning the Reference*.h and ShipRosterData.h consumers;
- a new `remaster/data/` folder;
- the generator tools under `remaster/tools/`.

Use worktree `wt-m0-data`.
- Move ships, structures, techs, minors, events and text out of the compiled headers and into JSON data files that the core loads at start. Keep a content checksum inside the save and the determinism fingerprint.
- Saves must still load, with schema migrations if needed. The WASM and native fingerprints must still match, checked by `test_fixture_determinism`. All suites must stay green.
- Rebuild both DLLs following HANDOFF.md "PC session 2026-11-07".
- Write `docs/DATA-FORMAT.md`. It is the foundation of the mod tools.

## L6 Build pipeline and carried-over bugs (barclay, sonnet)
You own:
- `remaster/tools/nightly/` (new);
- `remaster/godot/export_presets.cfg`;
- the app icon;
- the ion storm shader and the galaxy anomaly shaders;
- `remaster/tools/build_audio_cues.py`;
- the trade-route test fixture.

Use worktree `wt-m0-build`.

**Nightly script.** It does three things:
- export the exe with an icon and a version stamp;
- run every suite;
- run a fixed capture script covering the title, the galaxy, a system, every F-key screen and five battle situations, then make contact sheets in `remaster/captures/<date>/`, which is gitignored.

The orchestrator must be able to run it with one PowerShell command.

**Carried-over bugs.** Fix these:
- the blocky artefacts in the ion storm shader;
- the trade-route native test, using a drill that has a visible foreign colony;
- `build_audio_cues.py` matching the hand-edited cues.json.

**Housekeeping.** Remove the R3.5 debug env prints, or keep them gated and document them.
