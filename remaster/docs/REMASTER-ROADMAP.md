# BOTF Remastered — roadmap

`remaster/` is the working fork. `game/` and `tests/` at the repository root stay frozen as the
preserved playable-beta baseline.

## Design pillars

1. **Classic loop, modern clarity.** BOTF's identity stays intact: five asymmetric majors, minor races, colony management, a research spread, treaties and espionage, task forces and command-and-watch tactical battles. The modern layer adds explanation, previews, automation and fewer clicks.
2. **Every number explains itself.** Clients never re-implement rules. The core exposes forecasts and reasons, and the UI shows cause → effect.
3. **A deterministic, replayable core.** Integer and fixed-point outcome math only. Seeds and state are fully saved. Results are identical across MSVC, GCC, Clang and WebAssembly, and CI enforces this.
4. **One engine: Godot.** Simon chose Godot on 7 Nov 2026, after the Godot vs Unreal bake-off (HANDOFF.md "PC session 2026-11-07").
   - The Godot client (`godot/`, a GDExtension over the same core) is the game. All new presentation, content and polish work goes there.
   - The Unreal client (`game/`) is frozen as a reference. It is kept buildable but gets no new features.
   - The web client (WASM + TypeScript) stays as the CI and headless check of the core.

## Phases

| Phase | Content | Status |
|---|---|---|
| 0 Foundation | Fork, CMake, CI (GCC + Clang ASan/UBSan), fixture-independent tests | Done |
| 1 Stability | Fix every game-breaking defect in the audit. Soak harness across seeds, eras and players. Save schema 12 with migrations from 4–11. | Done (lane A) |
| 1 Combat | One tactical engine for headless and client play. Mutual ramming, cloak-ambush, disengage-under-fire retreat, negotiated hail truces, 3+ empire battles. | Done (lane A) |
| 1 Diplomacy/Intel | AI-initiated diplomacy with faction personalities. Treaties with real effects (trade, shared sensors, shared supply, mutual defense). Dynamic minor membership and secession. Integer espionage math with meaningful security. | Done (lanes B and H). Treaty effects are wired into the turn loop in schema 14 ([GAMEPLAY-REMASTER.md](GAMEPLAY-REMASTER.md) section 13) |
| 1 Web client | WASM bridge and a modern-LCARS TypeScript client covering the galaxy, systems, empire, research, fleets, tactical combat, diplomacy, intel, encyclopedia and saves | Round 1 done (lane C); phase-2 UI in lane F2 |
| 2 Modern gameplay | Troop-transport outposts and starbases (extending supply and sensors, and defending in combat). Empire-wide governors and queue templates. A turn chronicle. Minor-race progression and more minors. Curated galactic events and anomalies. Ruleset presets ("Classic", "Remastered"). Alternative victories: diplomatic, scientific and domination. | Done: lane D core layer (schema 13), lane G2 minor-race roster ([MINOR-RACES.md](MINOR-RACES.md)), lane H schema-14 treaty integration and diplomatic victory pacing |
| 3 Unreal client | Apply the Unreal review fixes, split the god-widget into a campaign subsystem with per-screen views, and surface all phase 1–2 features | Lane E merged (not yet compiled in Unreal); phase-2 UI in lane I |
| 3G Godot to finished game | Close the gap list in HANDOFF.md "Godot finish plan", round by round; Simon plays each round's build. | Started 7 Nov 2026 |
| 4 Content & polish | Replace embedded original text with original remaster descriptions. Expand the ship roster and tech tree, add art and audio, balance with long AI-vs-AI soaks, add a tutorial. | Later |

## Working rules

- At most three concurrent builders. Each works in its own worktree with exclusive file ownership. The orchestrator merges and re-verifies.
- A change lands only with regression tests and a green soak, and the core must build warning-free.
- Save compatibility: older saves keep loading, and any incompatible rule change bumps the schema.
