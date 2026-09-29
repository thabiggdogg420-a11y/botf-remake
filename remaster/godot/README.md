# BOTF Remastered: Godot client

This is the Godot 4.5 client, built as an alternative to the Unreal client in `../game`, so the two can be compared.
It runs the same deterministic core through the same JSON API that the browser client uses:
`botf_call(method, argsJson)` in `../web/bridge/WebBridge.cpp`, exposed to GDScript by a GDExtension in `core/`.

Everything here runs headless on Linux (tests, screenshots), and the Windows `.exe` is exported from Linux.

## Layout and ownership (parallel work)
| Path | Contents |
|---|---|
| `core/` | GDExtension (C++, godot-cpp), `BotfCore` singleton, build scripts, export presets |
| `scenes/galaxy`, `scenes/ui` | Galaxy map, LCARS theme and all strategic screens |
| `scenes/battle` | 3D tactical battle scene that plays the rules 3 combat event timeline |
| `tests/` | Headless GDScript tests (run with `godot --headless`) |
| `fixtures/` | Recorded `botf_call` JSON responses, for developing screens without the extension |

Only the owner of `core/` edits `project.godot` settings, apart from autoloads, which are listed in the README.

## Core API: `BotfCore`

`core/` builds the `botf_core` GDExtension: every core translation unit plus `../web/bridge/WebBridge.cpp`,
compiled natively. It registers an engine singleton, usable from any script by name:

```gdscript
var reply: Dictionary = BotfCore.request("newCampaign", {"faction": 1, "seed": 424242, "ruleset": "reference"})
if reply.ok:
    var view: Dictionary = reply.result      # same shape as the browser client's view
else:
    push_error(reply.error)                  # the core's own message, e.g. an illegal order
var raw: String = BotfCore.request_json("view", "{}")   # the exact bridge JSON string
```

- `request(method: String, args: Dictionary = {}) -> Dictionary` returns `{ok: true, result: ...}` or
  `{ok: false, error: String}`. Integral JSON numbers are converted to `int` (Godot's JSON parser returns floats).
- `request_json(method: String, args_json: String = "{}") -> String` is the raw `botf_call` passthrough.
- The name is `request`, not `call`: `Object.call()` already exists on every Godot object and cannot be overridden.
- Methods and arguments are those of `dispatch()` in `WebBridge.cpp` (`meta`, `newCampaign`, `view`, `submit`,
  `dryRun`, `colony`, `research`, `diplomacy`, `intel`, `endTurn`, `tactical`, `tacticalOrder`, `tacticalRound`,
  `tacticalAuto`, `save`, `load`, `fingerprint`, ...). `web/test/bridge.test.mjs` shows how to drive them.
- Check `Engine.has_singleton("BotfCore")` when a scene must also work without the extension (use `fixtures/`).

## Build the extension

```bash
core/build.sh                    # linux x86_64, template_debug (editor, tests)  -> core/bin/*.so
core/build.sh windows release    # mingw-w64 cross-compile                        -> core/bin/*.dll
core/build.sh all both           # everything the exports need
```

Needs CMake, a C++20 compiler and, for Windows, `apt install g++-mingw-w64-x86-64-posix`.
`core/fetch-godot-cpp.sh` clones godot-cpp (`godot-4.5-stable`) into `core/godot-cpp/` (gitignored).
A release export needs the `template_release` library; the release engine template crashes on a debug one.
The first godot-cpp build takes several minutes; later builds are incremental.
Determinism: the native extension reproduces the wasm core's fingerprints (`fixtures/determinism.json`
is checked by the tests).

## Test

```bash
./run-tests.sh                   # builds (incremental), imports, runs tests/run_tests.gd headless
SKIP_BUILD=1 ./run-tests.sh      # reuse core/bin
```

`tests/run_tests.gd` is a GDScript port of part of `web/test/bridge.test.mjs`: new campaign, orders,
forecast, ten turns, save/load determinism, battle drill with a mid-battle save, rules 3 combat events,
diplomacy drill, and the wasm-vs-native fingerprint and view comparison. The Godot binary comes from
`$GODOT`, `godot4`/`godot` on PATH, `/tmp/claude-0/Godot_v4.5-stable_linux.x86_64`, or is downloaded
to `~/.cache/botf-godot` (see `tools/godot-env.sh`). CI runs this as the `godot` job of `remaster-ci.yml`.

## Fixtures

`tools/dump-fixtures.sh` records `botf_call` responses with the committed wasm core (Node 22, no npm install)
into `fixtures/<name>.json`, each `{method, args, result}`:

| File | Call |
|---|---|
| `meta.json` | `meta` |
| `campaign-view.json` | `newCampaign` (faction 1, seed 424242, reference) |
| `colony-detail.json` | `colony` for the home system |
| `research.json`, `diplomacy.json`, `intel.json` | turn 0 screens |
| `campaign-briefing-turn5.json`, `campaign-view-turn5.json` | `endTurn` status/briefing and `view` at turn 5 |
| `diplomacy-drill-view.json`, `diplomacy-drill.json` | `newDiplomacyDrill` view and its `diplomacy` screen |
| `battle-rules3-endturn.json` | `endTurn` returning `{status: "battle"}` (station drill, rules 3) |
| `battle-rules3-encounter.json` | `tactical` before the first round |
| `battle-rules3-round1.json`, `battle-rules3-round2.json` | `tacticalRound` (rounds carry the event stream) |
| `battle-rules3-final.json` | `tacticalAuto` to the end |
| `determinism.json` | fingerprint after 10 turns (checked by the tests) |

Load one with `JSON.parse_string(FileAccess.get_file_as_string("res://fixtures/campaign-view.json")).result`
(numbers are floats there). CI fails when fixtures are stale; rerun the dumper after core changes.

## Export (Windows from Linux)

```bash
./export-windows.sh              # build/BOTFRemastered.exe + build/libbotf_core.windows.template_release.x86_64.dll
./export-windows.sh --linux      # also build/linux/BOTFRemastered.x86_64 and boot it headless as a check
EXPORT_MODE=debug ./export-windows.sh
```

`tools/install-export-templates.sh` downloads the 4.5-stable templates once (~1.3 GB) and keeps the four
x86_64 ones in `~/.local/share/godot/export_templates/4.5.stable/`. Presets are in `export_presets.cfg`
(pck embedded in the exe; the extension DLL ships next to it and must stay there). `build/` is gitignored.

## Screenshots (cloud, no GPU)

```bash
tools/screenshot.sh res://scenes/galaxy/galaxy.tscn /tmp/shots/galaxy.png
tools/screenshot.sh res://scenes/battle/battle.tscn /tmp/shots/battle.png --frames=120 --count=4 --interval=60
RENDERER=opengl3 SIZE=1280x720 tools/screenshot.sh res://tests/screenshot_smoke.tscn /tmp/shots/smoke.png
```

Runs Godot under `xvfb-run` with Mesa software rendering: Vulkan Forward+ on lavapipe by default
(`RENDERER=opengl3` for Compatibility on llvmpipe), instantiates the scene through `tools/screenshot.gd`
and writes PNGs. Options after the output path: `--frames=N` to wait before the first capture (default 30),
`--count=N --interval=M` for a series (`name-1.png`, `name-2.png`, ...), `--size=WxH`, and `--call=method`
to call a method on the scene root first (e.g. one that loads a fixture). Relative output paths are relative
to the current directory. `SHOT_TIMEOUT` (default 300 s) stops a hung run. Needs `xvfb` and `mesa-vulkan-drivers`.
