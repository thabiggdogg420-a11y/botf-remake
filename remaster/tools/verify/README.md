# verify: one-command export, test and capture

Run after every merge (nothing is scheduled; it only runs when someone runs it):

    powershell -NoProfile -File remaster\tools\verify\verify.ps1

Options: `-Target both|windows|web`, `-SkipExport`, `-SkipTests`, `-SkipCapture`. Exit code 0 only if every step passed.

Output: `remaster\captures\<yyyy-MM-dd>\<HHmmss>\` (gitignored), `captures\latest.txt` holds the last run's path.
- `sheet-<group>.png` contact sheets: `screens` (title, new game, F1-F8 screens), `battles` (five battle situations),
  `galaxy` (galaxy open plus each anomaly type), `compat` (the same anomalies under the Compatibility renderer), `smoke`
  (pc_smoke's screens), `web` (headless-browser capture of the web build). A shot that is one flat colour is marked BLANK.
- `summary.md` / `summary.json`: one row per step. `logs\` holds every process's stdout and stderr.

What runs, and how it overlaps:
1. Import once. Stamp the Windows preset with version `0.1.<commit count>` (restored after export; the exe also gets the icon).
2. Windows export, Web export and 10 headless suites run at once (no GPU). The capture windows start in the same moment:
   four Godot windows (screens, battles, galaxy, compat) and `pc_smoke`, sharing the GPU.
3. `battle-scene` runs alone afterwards: its frame-step check is wall-clock sensitive and fails under load.
4. Web capture (headless Chrome against `serve_web.py`), then contact sheets.
`pc_smoke` shares the GPU with the captures here, so its fps numbers are lower than a solo run; its pass/fail is what counts.

Serving the web build for play: `python remaster\tools\verify\serve_web.py` (binds 127.0.0.1 only, COOP/COEP on, refuses any other
bind address). Never publish it.

One-time setup for the Web export: `powershell -File remaster\tools\verify\setup-web-export.ps1` (installs the `web_*.zip`
templates). Until then the Web step reports BLOCKED. Even after that, the Web export and capture are blocked until the wasm
GDExtension core exists (lane L5): `core\botf_core.gdextension` has no `web` library entry, and `core\bin` holds no wasm build.

Debug env switches in the game (all off unless set, all read-only diagnostics, none in a shipped path):
`ROUND_TIMING` (prints round-start timings), `BOTF_CUE_TRACE` (prints audio cue scheduling), `SHOT_*` (screenshot tool only),
`BOTF_AUDIO_DIR`, `BOTF_HERO_MANIFEST`, `BOTF_RESOLVED_MANIFEST` (asset locations; to become pack-building inputs for the web build).
