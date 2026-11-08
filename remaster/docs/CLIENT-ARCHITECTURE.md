# Client architecture (Godot strategic client)

Outranked by: Simon's decisions in `standing\simon\` and MASTER-PLAN.md §4 and §7; the M0 lane brief (M0-LANES.md, L4 and "Browser target"). This doc records how the client is built after the M0 L4 refactor (branch `m0/m0-arch`). Paths are relative to `remaster/godot/`.

## 1. Why
MASTER-PLAN §4 "Client architecture": the screens mixed core reads, rules for drafts and orders, and node building in one file per screen, laid out as ports of the web client's `screens/X.tsx`. A redesign meant rewriting a whole screen. The refactor splits every screen into three layers, so a redesign replaces only the presentation and the same data feeds any new look (2D panel, 3D scene, web build).

## 2. The layers

| Layer | Folder | Knows about | Never does |
|---|---|---|---|
| Application state | `scripts/app_session.gd`, `scripts/app_routes.gd`, `scripts/app_saves.gd`, `scripts/app_loader.gd` | the core (`CoreClient`), the view, selection, saves | create nodes |
| View models | `scripts/viewmodels/*_vm.gd` | `AppSession`, `Fmt`, `Roster` | create nodes or use `Lcars` (enforced by `scripts/viewmodels/tests/test_viewmodels.gd:53` `_static_rules`) |
| Presentation | `scenes/ui/app.gd` (shell), `scenes/ui/screens/*.gd`, the panels `scenes/ui/trade_panel.gd`, `empire_tabs.gd`, `end_turn_check.gd` | its view model, the widget layer, navigation (`App.goto`, modals, toasts) | call the core directly (the only remaining reads are `app.core.last_error` and `app.core.backend` for display) |
| Widgets (LCARS design system) | `scenes/ui/widgets/` | design tokens and the Theme | read game state |

### 2.1 Application state
- `AppSession` (`scripts/app_session.gd`) holds `core`, `meta`, `view`, `encyclopedia`, `selection`, `briefing` and `encounter`, and the order API: `issue` (`:141`), `cancel_orders`, `dry_run`, `refresh` (`:131`), `select_*`, `pending_ids`, `unit_order_ids`, `lock_reason` (`:50`). It emits `view_changed`, `selection_changed`, `notified(text, tone)` and `failed(message)` (`:8-11`); the shell turns the last two into toasts.
- `App` (`scenes/ui/app.gd`) is now only the shell. It owns one session (`:21`) and forwards its old fields (`core`, `meta`, `view`, `selection`, `briefing`, `encounter`, `encyclopedia`) and methods (`issue`, `find_by_id`, `select_fleet` ...) to it, so the galaxy screen, the battle scene, the audio system and the tests are unchanged. `_on_view_changed` (`:307`) applies the theme, rebuilds the chrome and refreshes the current screen, in the same order as before.
- `AppRoutes` (`scripts/app_routes.gd:6`, `:25`) is the screen registry (rail label, F-key, hint, script). `App.GAME_SCREENS` aliases it for `tools/pc_smoke.gd`.
- `AppSaves` (`scripts/app_saves.gd`) writes and lists save slots and the settings file.
- `AppLoader` (`scripts/app_loader.gd`) is the only loader: see §5.

### 2.2 View models
One per screen, each a `ViewModel` (`scripts/viewmodels/view_model.gd`): a RefCounted with a `session`, a `changed` signal (`:8`), and helpers `query`/`query_dict` (`:29`), `issue` (`:37`), `dry_run`, `lock_reason`, `place`. A view model returns plain Dictionaries, Arrays, strings and numbers, and owns the screen's state (tabs, drafts, filters, selection).

| Screen | View model | Holds |
|---|---|---|
| Shell (top bar, rail) | `ShellVM` | heading, the 7 stats, the orders stat, rail extras |
| Title | `TitleVM` | latest save, the filmed power, footer |
| New campaign | `NewCampaignVM` | power, size, powers in play, seed, era, rules, preset, the `newCampaign` options |
| Saves | `SavesVM` | the slot list |
| System (F2) | `SystemVM` | colony detail, workforce draft and `workforcePreview`, every colony order |
| Empire (F3) + trade and manager tabs | `EmpireVM` | totals, member bonuses, tabs, trade routes, governors, templates, stations, minor worlds |
| Research (F4) | `ResearchVM` | fields, forecast, allocation draft (`rebalance`), locks, focus |
| Fleets (F5) | `FleetsVM` | units, sectors, marks, send-to plan, stance |
| Diplomacy (F6) | `DiplomacyVM` | selection, panes, treaty draft, refused calls, every diplomatic order |
| Intel (F7) | `IntelVM` | budget draft, targets, missions, reports |
| Database (F8) | `DatabaseVM` | tab, search, facet filters, rows, entries |
| Combat | `CombatVM` | encounter, ship and target, scope, maneuver blocks, tactical orders, round log, archive |
| Overlays | `OverlaysVM` | briefing, categories, events and choices, end-of-game summary |
| End-turn check | `EndTurnVM` | warnings, forecast, dry-run fixes |

The galaxy screen (F1, `scenes/galaxy/galaxy_screen.gd`) and its order menu (`GalaxyOrders`) are outside L4's ownership; they keep calling the App API, which now forwards to the session. Giving them a `GalaxyVM` is the next step for whoever owns `scenes/galaxy` (workstream 2).

### 2.3 Presentation
Each screen script keeps `app`, `refresh()` and its node names (the UI suite and `pc_smoke` find nodes by name), creates its view model lazily (`var vm: XVM: get: ...`), and forwards the fields tests poke (`new_campaign_screen.faction`, `research_screen._set_alloc`, `combat_screen.order/_round/_auto`, `empire_screen.tab`) to it. The "Port of screens/X.tsx" headers are gone; screens are named for what they show.

### 2.4 Widgets
`scenes/ui/widgets/lcars.gd` (class `Lcars`) holds the tokens (palettes, fonts), `build_theme` (`:159`) and the builders (`pill` `:332`, `frame`, `stat`, `meter`, `badge` ...), plus the layout components the screens shared by copy before: `screen_margin` (`:519`), `title_block`, `row_button` (`:537`), `row_inner`, `ignore_mouse`, `icon_button`, `bar_row`, `tab_bar`, `tip`. Drawn widgets each have a file: `LcarsElbow`, `PlanetOrb`, `ChoiceIcon`, `RedAlert`, `ShipSilhouette`, `EventGlyph`, and `DataTable`. Number formatting lives in `Fmt` (`scripts/viewmodels/fmt.gd`) so view models do not depend on widgets; `Lcars.fmt/signed/num` delegate to it.

## 3. Data flow
1. A control calls a view-model command (e.g. `ResearchVM.apply`).
2. The view model calls `session.issue(command, cancel_ids, success)`.
3. The session submits to the core; on success it re-reads the view and emits `view_changed`, then `notified`; on refusal it emits `failed` with the core's reason.
4. The shell refreshes the chrome and the current screen; the screen asks its view model again.

## 4. Fixed bug: the empire option at new campaign
`App.start_campaign` passed its options straight to the core, which (WebBridge `newCampaign`, `remaster/web/bridge/WebBridge.cpp:2396`) and the fixture backend (`scenes/ui/core_client.gd:96`) read the player's power from `faction`. Callers that passed `empire` (`scripts/audio/tests/test_audio.gd:355`, `scripts/audio/tools/audio_tour.gd:26`) always got the Federation. `AppSession.campaign_options` (`scripts/app_session.gd:178`) now maps `empire` to `faction` (`faction` wins when both are given) and `App.start_campaign` (`scenes/ui/app.gd:368`) uses it. Test: `_start_campaign_empire` (`scripts/viewmodels/tests/test_viewmodels.gd:257`) starts empires 3 and 5 through `App.start_campaign({empire: n})` on the fixture and native backends; with the fix removed it fails with "got 1".

## 5. Browser target
The game also ships as a web export served locally (M0-LANES "Browser target"). The client rules that follow:

- **One loader, no disk paths.** All asset, data and save access in the UI layers goes through `AppLoader` (`scripts/app_loader.gd`). It serves only `res://` (the exe's pck or the web pck) and `user://` (AppData on Windows, IndexedDB on web), refuses anything else (`:10`, `:19`), and `mount_pack` (`:26`) is the one place an extra pack (a web-fetchable asset bundle) is added. The fixture backend (`core_client.gd:71`), fonts (`widgets/lcars.gd`), the battle scene (`battle_host.gd:23`), the screen scripts (`app.gd` `goto`), the starfield shader and saves (`app_saves.gd`) all use it. `preload` paths were replaced by class names (`Overlays`). The two remaining environment reads are debug switches, not paths: `BOTF_TITLE_EMPIRE` (`screens/title_screen.gd:93`) and `ROUND_TIMING` (`screens/combat_screen.gd:338`).
- **No threads.** No UI-layer script creates a Thread, uses WorkerThreadPool, Mutex or Semaphore; every core call is synchronous. A non-threaded web export therefore runs the UI unchanged, and needs no COOP/COEP headers for it.
- **Enforcement.** `test_viewmodels.gd` `_static_rules` (`:53`, list at `:39`) scans every script in `scenes/ui`, `scenes/ui/screens`, `scenes/ui/widgets`, `scripts/viewmodels` and `scripts/app*.gd` and fails on threads, `FileAccess`/`DirAccess`/`ResourceLoader`/`load`/`preload` outside `AppLoader`, `OS.execute`, drive paths or `LocalAssets`.
- **Compatibility renderer.** The UI suite and the view-model suite pass on `gl_compatibility` (WebGL2's renderer), in a real window: `scenes/ui/tests/run_ui_compat.ps1` runs both with `--rendering-method gl_compatibility` and `BOTF_EXPECT_RENDERER`, and the UI suite checks the renderer it actually got (`scenes/ui/tests/test_campaign_ui.gd:31`). Run on 8 Nov 2026: "renderer: gl_compatibility (display Windows)", 159 checks 0 failures, 107 checks 0 failures. The UI uses only Control nodes, StyleBoxFlat and CanvasItem drawing; its one shader, the starfield (`scenes/galaxy/starfield.gdshader`, galaxy lane), compiled in that run.
- **Settings the web cannot honour.** `DisplaySettings.offered` hides resolution, window mode, vsync, frame cap and render scale (FSR) on web (`DisplaySettings.WEB_UNSUPPORTED`), because the browser owns the window and Compatibility has no FSR. Tested with `DisplaySettings.web_preview`.
- **Saves** already go to `user://saves` (IndexedDB on web) through `AppSaves`.
- Out of L4: the native core as wasm (L5 and the build lane), web asset budgets (art bible, L3), the web export preset and the nightly two-target run (L6), and the galaxy and battle shaders (their lanes).

## 6. Adding a screen
1. Add `scripts/viewmodels/<name>_vm.gd` extending `ViewModel`; put every core read, draft and order there.
2. Add `scenes/ui/screens/<name>_screen.gd` with `var app: App`, a lazy `vm`, and `refresh()` that builds from the model with `Lcars` widgets.
3. Register it in `AppRoutes.GAME_SCREENS` or `OUTSIDE_SCREENS`.
4. Add model tests to `scripts/viewmodels/tests/test_viewmodels.gd` and a render check to `scenes/ui/tests/test_campaign_ui.gd`.
5. Load files only through `AppLoader`; no threads.

## 7. Tests
- `res://scripts/viewmodels/tests/test_viewmodels.gd` (new): layering and browser rules, pure model logic, every model over a live session on the fixture and native backends, and the empire option. 109 checks.
- `res://scenes/ui/tests/test_campaign_ui.gd`: the UI workflow, unchanged apart from the renderer check.
- `scenes/ui/tests/run_ui_compat.ps1` (new): both suites on the Compatibility renderer.
- `res://scenes/ui/tools/check_scripts.gd` now also loads `widgets`, `scripts` and `scripts/viewmodels`.
