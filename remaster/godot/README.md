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
