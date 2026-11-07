# Godot finish rounds: shared brief for builders

Simon, 7 Nov 2026: "go with godot, and begin its development towards the full finished game", then "go for all of it".
The plan is in HANDOFF.md, "Godot finish plan" (rounds R1-R6). This file holds the rules every round builder follows.

## Setup
- Main checkout: `C:\Users\BiggDogg\Documents\ChatGPT\Game Remakes`, branch `claude/serene-archimedes-nvtcnw`.
- Make your own worktree and branch, and never work in the main checkout:
  `git -C "<main>" worktree add "C:\Users\BiggDogg\Documents\ChatGPT\wt-<round>" -b godot/<round> claude/serene-archimedes-nvtcnw`
- The worktree is missing gitignored files. Copy them in:
  - `remaster\godot\core\bin\*.dll` from the main checkout, so the GDExtension loads.
  - For local models, set `BOTF_HERO_MANIFEST=<main>\remaster\game\LocalAssets\ships\godot-hero-manifest.json`.
- Godot: `C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe`.
  - Import first: `--headless --path remaster\godot --import`.
  - Then run all suites with `--headless --path remaster\godot -s <suite>`:
    - `res://tests/run_tests.gd`
    - `res://scenes/ui/tests/test_campaign_ui.gd`
    - the five `res://scenes/battle/tests/test_*.gd`
- Screenshots: `SHOT_FULL=1 godot --path remaster\godot --resolution 1920x1080 -s res://scenes/ui/tools/screenshot.gd -- <outdir> <shot>`. The shot names are in that file. `SHOT_DRILL_SHIPS=24` gives a 24v24 starbase drill.
- Smoke run: `-s res://tools/pc_smoke.gd`.
- Heavy jobs (Blender, long renders) run at BelowNormal priority.

## Rules
- Stay inside your file ownership list. If you need a file outside it, stop and report rather than editing it.
- Do not change the C++ core rules. Bridge additions in `web/bridge/WebBridge.cpp` are allowed only if your round says so. If you add one, rebuild the DLLs as described in HANDOFF.md, "PC session 2026-11-07".
- The game is private and personal: never distribute it. Never commit `LocalAssets`, third-party audio or models, or renders of them.
- Do not port from Unreal. Unreal (`remaster/game`) is a reference only and is frozen: read it, never edit it.
- Work is done when all suites pass, `pc_smoke.gd` shows 0 failures, and screenshots show each new feature on screen. A change with no test is not done.
- Commit on your branch with clear messages, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not push and do not merge: the orchestrator merges.
- Final report, under 400 words:
  - the branch and its commits;
  - each item, marked done or not done, with evidence: test output lines and screenshot paths;
  - anything left open.
