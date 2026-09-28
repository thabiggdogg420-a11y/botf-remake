# Unreal client architecture

The Unreal client (`game/Source/BOTFRemake/`) is a thin Slate front end over the deterministic core
(`game/Source/BotfSim/`). The core is compiled into the module unchanged by `SimulationBridge.cpp`. This page
describes where the client is today, the architecture it is moving to, and the order of the remaining steps.

## Rules the client follows

- **Clients never re-implement rules.** When the UI needs a number, it asks the core. A formula that the core
  applies but does not yet expose is copied into one place, `BotfViewModel.h`, marked `MIRROR` and listed under
  [Core queries the client needs](#core-queries-the-client-needs), until the core offers the query.
- **Every order goes through core validation.** Orders are submitted, cancelled and dry-run against the real
  `botf::submit` / `botf::cancel` contract. The UI never predicts acceptance on its own.
- **No exception crosses Slate.** Core calls can throw. Actions run through the campaign's exception boundary
  (`FBotfCampaign::Run`). Text attributes go through `SafeText`, predicates through `SafeBool`, and every custom
  `OnPaint` catches and draws nothing rather than unwinding into the engine.
- **The UI never outlives the world.** The combat scene is held by `TWeakObjectPtr`. Its render target is drawn
  only while the scene is alive and is never rooted by the UI, because rooting it would keep the actor and the
  world alive after Play-In-Editor ends.

## Where it is today

| File | Role |
|---|---|
| `BotfGameMode.cpp` | `ABotfGameMode` plus `SBotfInterface`, the single ~4,700-line widget that builds every screen, and the `BOTF.UI.Shell` automation test |
| `BotfCampaign.h/.cpp` | **New.** `FBotfCampaign`: the campaign itself (see below) |
| `BotfViewModel.h` | **New.** Engine-independent view-model helpers (range, forecast, tactical and diplomacy text), unit-tested with a plain compiler |
| `BotfGalaxyMap.h` | `SStarChart`, the galaxy chart, with per-draft paint caches |
| `BotfCombatUi.h`, `BotfTacticalRadar.h`, `BotfBattleReplay.h` | Tactical viewport, radar, battle image and record replay |
| `BotfUiTheme.h`, `BotfUiControls.h` | LCARS palette, buttons, bars and sliders |
| `BotfSettingsPanel.*` | Video settings with a confirm-or-revert test |
| `BotfCombatScene.*` | The 3D battle actor rendered into a texture |

### Step 1, done: `FBotfCampaign`

`FBotfCampaign` is plain C++, not a `UObject`, so it can be owned by the widget now and moved into a
`UGameInstanceSubsystem` later without changes. It owns:

- the committed `botf::GameState` and the open turn transaction (`botf::session::TurnSession`);
- the draft preview cache. `Preview()` never throws: if the core rejects the draft, the committed state is shown
  and `PreviewError()` explains why. `PreviewGeneration()` changes whenever the preview is rebuilt, and views key
  their own caches on it;
- order submission, cancellation, atomic replacement (`ReplaceOrders`) and dry runs (`DryRun`);
- the turn flow: `BeginTurn`, and `ResumeTurn`, which commits or pauses on a player battle. Finished encounters of the
  last committed turn stay available (`LastTurnEncounters`) so the replay can show orders, AI decisions, ambushes
  and rams;
- save, load and snapshot listing:
  - Saves are written to a `.pending-write` file, verified by reading them back and published by rename. The temp
    file is deleted on any failure.
  - An unapplied intelligence allocation is added to a copy of the draft and committed only once the file is
    published.
  - Only the envelope limit (32 MiB) is checked by the client. Plain campaigns are bounded by `botf::deserialize`
    itself.
- dirty tracking against the last verified save;
- the single exception boundary, `Run`;
- change notifications (`Subscribe`): `Draft`, `Replaced`, `TurnCommitted`, `Session`, `Saved`.

`SBotfInterface` owns one `FBotfCampaign` and consumes its notifications:

- On `Replaced`, it drops every selection made in the old campaign.
- On any change, it repaints the chart.

For now the widget keeps reference aliases (`State`, `Session`, `StateVersion`, `Saves`, ...) to the campaign's
fields. This kept the 4,700-line widget and its automation test compiling unchanged while ownership moved. New code
should call the campaign's methods rather than the aliases.

### Other fixes in this pass

- **Keyboard.**
  - In the command deck, global shortcuts run on the tunnelling pass (`OnPreviewKeyDown`). A button that kept
    focus after a mouse click can no longer swallow Enter, Space or Ctrl+Enter.
  - Keys with no global meaning still reach the focused control.
  - Menus and dialogs keep normal focus navigation.
  - No hotkey fires while a text field (the database search) has focus.
- **Mouse capture.** The tactical viewport ends its orbit drag when capture is lost or the button is no longer
  down (for example after alt-tab).
- **Task forces.** A task force's range, routes and hints use its shortest-ranged member. The core validates a
  group order member by member, so this is what the core will accept.
- **Quit.** Quit goes through `UKismetSystemLibrary::QuitGame`: a packaged game exits, and Play-In-Editor ends
  without killing the editor.
- **Floating point.** The module uses `FPSemantics = FPSemanticsMode.Precise`.
- **Per-frame cost.** Supply coverage and course routes are computed once per draft version, not per sector per
  frame. The treasury forecast and database search results are cached.
- **Settings teardown.** Destroying the settings panel no longer applies display settings.
- **Automation test.** The test deletes its snapshot folder when it finishes.
- **Core catch-up** (schema 12 and the lane B diplomacy):
  - fusion plants are shown, counted and scrapped separately;
  - the tactical UI shows disengaging ships, cloak/ambush, rams (impact and recoil), AI-controlled orders, standing
    orders, the hail outlook and truces, in the round log, the ship cards, the radar and the battle replay;
  - diplomacy shows AI proposals with their response deadline, calls to arms (join or refuse), ended treaties
    (expired, broken, superseded, dissolved), relation memory and standing, betrayal warnings, allies, shared
    sensors, eliminated empires and the diplomatic-majority status.
- **Compile fix.** `SystemStrategicStatus` called a `botf::systemAt` that has never existed in the core.

## Target architecture

```
ABotfGameMode ── spawns ──> ABotfCombatScene (3D, render target)
      │
      └─ UBotfCampaignSubsystem (UGameInstanceSubsystem) ── owns ──> FBotfCampaign
                                                                        │ notifications
                        ┌───────────────────────────────────────────────┤
                        v                                               v
                 Presenters (plain C++)                         SBotfShell (screen switcher,
                 one per screen, built on BotfViewModel.h        top bar, inbox, modal layer)
                        │ view models (values + commands)               │
                        v                                               v
                 Views (Slate, one file per screen) <─────────── hosts the active view
```

- **`FBotfCampaign` (done)** is the only owner of campaign state. A future subsystem only holds it, so the
  campaign survives map travel and is reachable from UMG or Python.
- **Presenters** turn campaign state into view models: plain structs of strings, numbers and enabled flags,
  plus command functions. A presenter is engine-independent, like `BotfViewModel.h`, and is unit-tested with a
  plain compiler against the core. Each presenter caches on `PreviewGeneration()`.
- **Views** are thin Slate widgets. They bind to view-model fields and call presenter commands through
  `RunGuarded`. They never call `botf::` directly.
- **One file per screen:**
  - `SBotfGalaxyView` (chart, docks, inbox)
  - `SBotfSystemView`
  - `SBotfResearchView`
  - `SBotfDiplomacyView`
  - `SBotfIntelView`
  - `SBotfEmpireView`
  - `SBotfCombatView` (live encounter and replay)
  - `SBotfShellView` (title, setup, load, settings, confirm)

## Next steps

These are in order. Each step is behaviour-preserving and keeps `BOTF.UI.Shell` green.

1. Replace the reference aliases in `SBotfInterface` with calls on `Campaign`, one screen at a time. Delete each
   alias when nothing uses it.
2. Move `FBotfCampaign` into `UBotfCampaignSubsystem` (a `UGameInstanceSubsystem`). The widget receives the
   subsystem instead of constructing a campaign.
3. Extract the diplomacy presenter and view first. It is the most self-contained screen, and its text helpers
   already live in `BotfViewModel.h`. Then do combat, then system, then galaxy.
4. Replace `RefreshAll()`, which rebuilds every list after every action, with per-view refresh on campaign
   notifications.
5. Run turn resolution off the game thread. `session::resume` is pure on copies, so a task can compute the result
   and the game thread commits it.
6. Remove each `MIRROR` formula in `BotfViewModel.h` as the core adds the query.

## Verifying without Unreal

The core-facing parts compile and run without Unreal:

- `BotfViewModel.h` compiles against the core with GCC and Clang (`-Wall -Wextra -Wshadow -Werror`). A scratch
  test checks it against real campaigns and battles:
  - task force range and routes agree with core validation;
  - the forecast matches what `advance()` charges;
  - round logs contain disengage, ambush and AI flags;
  - replay matching works.
- `BotfCampaign.cpp` compiles against the core through a small Unreal type shim (`FString`, `TArray`,
  `IFileManager` backed by `std::filesystem`). A scratch test exercises:
  - save, verify and publish;
  - temp-file cleanup and an unchanged draft when publishing fails;
  - a mid-battle save and load;
  - turn commit and notifications.
- `BotfGameMode.cpp` and every client header parse with GCC and Clang against a loose Slate shim. The shim is typed
  where our code's correctness depends on it: own members, core calls, lambdas and printf formats.
  - This catches typos, wrong member names, shadowing, const and capture errors, and format mismatches.
  - It does not prove that a Slate API exists. The owner's UE build remains the authority for those.

These harnesses live outside the repository (they are not part of CMake or the Unreal build). Keep new
client logic in `BotfViewModel.h`-style headers so it can be checked the same way.

## Core queries the client needs

`BotfViewModel.h` mirrors these formulas. Schema 14 (GAMEPLAY-REMASTER.md section 13) adds the core queries, so the
mirrors can be deleted when the client is next updated:

| Mirror in the client | Core query (schema 14) | Used for |
|---|---|---|
| `shipMaintenance(fleet)` | `botf::fleetUpkeep(fleet)` | treasury forecast |
| `view::tradeRouteIncome(state, route)` | `botf::tradeRouteRevenue(state, route)` (a different name, because the unqualified mirror call would otherwise be ambiguous) | forecast, trade panel |
| `view::treatyDividend(state, empire)` | `creditForecast(...).treatyDividends` | forecast |
| `forecastIncome(committed, draft, empire)` | `botf::creditForecast(state, empire)`, and `TurnReport::ledger` for what the turn actually did | the whole forecast; it now also itemises station upkeep, events and pending orders |
| `view::supplyRange(state, fleet)` | `botf::supplyRange(state, fleet)` (allied yards count, schema 14) | range readout |
| per-member `route()` checks for group courses | `botf::taskForceRoute(state, id, x, y)` and `botf::taskForceRange(state, id)` | group courses and range |
| web bridge `shipSpeed` / `unitSpeed` | `botf::shipSpeed(state, fleet)` | ETA |
| orders read from `tactical::Encounter` only | `CombatRound::orders` (maneuver, target, AI, decloak/ambush, disengaging, hail, ram) | replays of battles from saves and older turns |
| the call to arms could only lapse | `botf::makeRefuseCallToArmsCommand(id, defenderId, aggressorId)` | an explicit REFUSE that applies at once |

Save size: `botf::MaxSaveBytes` (8 MiB) is the campaign cap in `botf::deserialize`, which also loads a turn-session
envelope's baseline. The largest soak save is about 51 KiB. The client no longer adds a limit of its own.
