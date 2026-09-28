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
| `BotfGameMode.cpp` | `ABotfGameMode` plus `SBotfInterface`, the single ~6,200-line widget that builds every screen, and the `BOTF.UI.Shell` automation test |
| `BotfCampaign.h/.cpp` | `FBotfCampaign`: the campaign itself (see below) |
| `BotfViewModel.h` | Engine-independent view-model helpers (range, forecast, tactical and diplomacy text), unit-tested with a plain compiler |
| `BotfGameplayViewModel.h` | **New (lane I).** Engine-independent view models for the schema-13 gameplay layer and the minor-race roster (see [Gameplay layer and minor races](#gameplay-layer-and-minor-races)) |
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

### Gameplay layer and minor races

The client surfaces the schema-13 layer ([GAMEPLAY-REMASTER.md](GAMEPLAY-REMASTER.md)) and the minor-race roster
([MINOR-RACES.md](MINOR-RACES.md)). The logic lives in `BotfGameplayViewModel.h` (`botf::view`); the Slate code only
converts its results and submits orders through `FBotfCampaign`.

| Feature | Where | Core API |
|---|---|---|
| **New campaign** | Setup: GAMEPLAY row, REMASTERED (default) or CLASSIC, with a summary of each preset. Every new campaign goes through `view::startCampaign`, which calls `botf::newCampaign(CampaignSetup)`; `generateCampaign` / `generateReferenceCampaign` stay the fixtures' baseline. Loading keeps a schema-13 Classic choice; older saves leave the default. | `newCampaign`, `rulesetPresetName` |
| **Stations on the chart** | Station block in the sector's top-right corner (a starbase gets a second frame, an upgrade in progress an accent frame); an empty frame marks a transport assembling an outpost. Supply (bright) and sensor (faint) Chebyshev rings for own stations and the station in the selected sector; STATIONS toggle. | `visibleStations`, `stationStats` |
| **Build Outpost** | Unit card (BUILD OUTPOST, with the assembly time or core's blocker) and the sector menu. The order replaces the transport's other drafted fleet orders (core accepts one per fleet). | `makeBuildOutpostCommand`, `outpostBlocker`, `outpostBuildTurns` |
| **Station inspector** | Left dock when the selected sector holds a visible station: hull, shields, armament, reach, upkeep, repairs, upgrade progress. For an own outpost: funding colony (cycle), cost and core's blocker, UPGRADE TO STARBASE. | `stationUpgradeCost`, `stationUpgradeBlocker`, `stationUpgradeFor` |
| **Stations in battle** | Ship cards tag them `STATION (immobile ...)`; the radar frames them in their owner's colour; replays and records name them by combatant id (they are recorded as Destroyer-class participants). The 3D scene gives station slots a flat placeholder disc and never loads a ship mesh for them (`ConfigureBattle`'s `StationSlots`). | `isStationCombatant`, `stationIdForCombatant` |
| **Anomalies** | Nebulae and ion storms tint their sectors; black holes, neutron stars, wormholes (with a link to a charted exit) and rifts get glyphs; only explored sectors. The hover hint lists each anomaly's effect; the left dock gives the full text. ANOMALIES toggle. | `knownFeatures`, `featuresAt`, `ionStormCells` |
| **Galactic events** | EVENTS card (top bar; opens by itself when a decision is waiting): description, place, deadline, each option with its effect and the default. A choice is a drafted `ResolveEventChoice`; choosing again replaces it. Inbox rows and end-turn warnings for undecided events. | `empireEvents`, `eventChoices`, `eventDefaultChoice`, `describeEvent` |
| **Victory** | Empire, VICTORY tab: one bar per condition, the megaproject and rivals' projects. Solar System sheet: MEGAPROJECT section (START HERE, CANCEL PROJECT; a start drafted this turn is withdrawn). End-of-game overlay (Esc or REVIEW THE GALAXY closes it; NEW CAMPAIGN, MAIN MENU). | `victoryProgress`, `megaprojectBlocker`, `megaprojectIndustryCost`, `megaprojectWorkPerTurn`, `megaprojectCreditCost` |
| **Turn briefing** | The inbox lists the last turn's `TurnReport::chronicle` by category; an entry with a place centres the chart there, an event entry reopens its card. | `chronicleCategoryName` |
| **Build templates** | Solar System sheet, under the queue: name field, SAVE QUEUE AS TEMPLATE, and per template APPLY, DEFAULT / UNSET DEFAULT, UPDATE (from the queue), DELETE. | `empireBuildTemplates`, `make...BuildTemplateCommand` |
| **Minor races** | Chart: a ring around contacted independent minors, profile line in the hover hint, full profile in the left dock. Diplomacy: known-races rows (temperament, join bonus, tech, population), dossier status line and RACE INFO profile with the description. Empire, CAMPAIGN tab: MEMBER RACES with the active bonuses. | `minors::raceFor`, `minors::memberBonuses`, `describeJoinBonus` |

`BOTF.UI.Shell` gained a block that starts a Remastered Expanded campaign through the setup screen (layers on,
anomalies present, roster placed, profiles and victory rows readable), orders Build Outpost, injects a galactic event
decision (test-only `InjectBotfTestEvent`), checks the end-turn warning, chooses and re-chooses an option, ends one
turn (decision applied, transport assembling, briefing entry under Events), round-trips a save and starts a Classic
campaign.

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
- `BotfGameplayViewModel.h` compiles against the core with GCC and Clang (`-Wall -Wextra -Wshadow -Werror`). A scratch
  test (about 190 checks) runs it against real campaigns: both presets and every era through `newCampaign`, fog-filtered
  anomaly marks and tooltips, Build Outpost through assembly, the inspector and upgrade blockers against core, a real
  station battle (immobile combatant, tags, record labels), a curated event decided and resolved, briefing entries for
  90 turns, victory rows, megaproject start and cancel, outcome text, templates and minor profiles. It also mirrors the
  new `BOTF.UI.Shell` block step by step against the core.

How the Slate shim is built (rebuilt by lane I; the harness is not kept in the repository):

- Every engine include resolves to one `Shim.h`. SNew/SAssignNew construct the widget and call its `Construct` with
  the stored extra arguments, so our own widgets' `Construct` signatures, `SLATE_ARGUMENT` / `SLATE_ATTRIBUTE` names
  and virtual overrides are checked. Standard widgets take any layout method, but text, colour, visibility, brush,
  index and callback arguments are typed, and `_Lambda` results are checked.
- It runs twice: with `TCHAR = char` so `FString::Printf` and `UE_LOG` get printf format checking, and with
  `TCHAR = wchar_t` / `TEXT() = L"..."` to catch char/TCHAR mixing.
- It is calibrated on the unmodified files (they must pass unchanged) and on negative probes that must fail: an
  `FString` passed as text, a missing `*` for `%s`, a format mismatch, a wrong `_Lambda` result, an unknown own-widget
  argument, and a narrow literal as `TCHAR*`.
- `BotfGameMode.cpp`, `BotfCombatScene.cpp` and `BotfCampaign.cpp` (with every client header they include) pass with
  GCC 13 and Clang 18 in both modes, with `-Wall -Wextra -Wshadow -Werror`.

These harnesses live outside the repository (they are not part of CMake or the Unreal build). Keep new
client logic in `BotfViewModel.h`-style headers so it can be checked the same way.

One stand-in check is in the repository and runs in CI (job `unreal-tu`): `game/Tools/check-unreal-tu.sh`. It
compiles `SimulationBridge.cpp` as the single translation unit UBT builds, with `-Wshadow -Werror`. It then compiles
the core and both view-model headers again, with engine and `<windows.h>` macro names predefined (`near`, `far`,
`IN`, `OUT`, `check`, `verify`, `TEXT`, `PI`, `interface`, `small`, ...). The check was calibrated on the core's old
`bool near` local, which it rejects.

## First real compile: risk list

Build with `game/Package-Windows.ps1` (see [PACKAGING.md](PACKAGING.md)).

Fixed before the first compile:

- `FSlateDrawElement::MakeLines` gets its points as `FVector2f` through `BotfUi::DrawLines`. The `TArray<FVector2D>`
  overload is a UE 5 deprecation shim.
- `SPickBar::StepAt` stores `GetLocalSize()` in an `FVector2D` before `FMath::Max(1.0, ...)`. Since UE 5.1 the
  getter returns float-based data, and a mixed float/double template call can fail to deduce.
- The module declares `ApplicationCore`, which provides `GenericPlatform/GenericWindow.h` and `EWindowMode`.
- The core no longer names a local `near`, which `<windows.h>` defines as an empty macro.

Still unproven, in rough order of likelihood. Each is a single-line fix if it fails:

| Where | Risk |
|---|---|
| `BotfGameMode.cpp`, `BOTF.UI.Shell` test | `FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0)` uses the `uint32 UserIndex` constructor. Newer engines prefer `FInputDeviceId`; if the overload is gone, use `FInputDeviceId::CreateFromInternalId(0)`. |
| every `OnPaint` | `Geometry.ToPaintGeometry(FVector2D, FSlateLayoutTransform(FVector2D))` depends on the `FDeprecateVector2DParameter` conversions. If they fail, pass `FVector2f`. |
| custom leaf widgets | `ComputeDesiredSize(float) const` returning `FVector2D`. If the engine moved this to `FVector2f`, the `override` fails loudly. |
| `BotfUiTheme.h` | `FSlateRoundedBoxBrush(FLinearColor, FVector4)`. If the radii type changed, use `FVector4f`. |
| `BotfGameMode.cpp` `BeginPlay` | `PC->bShowMouseCursor = true` writes the field directly. Use `PC->SetShowMouseCursor(true)` if it is made private. |
| `BotfCombatScene.h` | A plain `enum ECameraMode : int` inside a `UCLASS` body. UHT normally ignores it; if not, move it outside the class. |
| `IMPLEMENT_SIMPLE_AUTOMATION_TEST` flags | `EAutomationTestFlags::EditorContext \| ProductFilter` is valid in UE 5.5+. |
| `BOTFRemake.uproject` | `EditorScriptingUtilities` plugin: if 5.8 removed or renamed it, the project refuses to open. Drop the entry. |
| `Content/` | Assets saved by a newer engine than the local one fail to load at cook time (`LogLinker`). |
| Runtime only | `FCoreStyle::GetDefaultFontStyle("BoldCondensed", ...)` falls back to the default face if the typeface is missing. |
| `BotfAssetRegistry.h`, `BotfAudioSubsystem.h` | `UCLASS() ... final` subsystems declared in `Private/` with non-UPROPERTY members of plain C++ types (`botf::assets::Catalog`, `CueScheduler`). UHT accepts this; if it objects to `final`, drop it. |
| `BotfAudioSubsystem.h` | Subsystem that is also an `FTickableGameObject` (`GetTickableTickType` / `IsTickableWhenPaused` overrides). If an override signature changed, adjust or tick from the game mode instead. |
| `BotfAudioSubsystem.cpp` | `UGameplayStatics::CreateSound2D(World, Sound, Volume, Pitch, StartTime, nullptr, bPersist, bAutoDestroy)` argument order, `UAudioComponent::AdjustVolume`, `FadeOut`, `Collection.InitializeDependency(UClass*)`. |
| `BotfSettingsPanel.cpp` | `SSlider` `Value_Lambda` / `OnMouseCaptureEnd_Lambda` / `OnControllerCaptureEnd_Lambda` argument names. |
| `BotfAssetRegistry.cpp` | `FSoftObjectPath(FString).TryLoad()` on `/Game/Local/...` paths that exist only on a PC that ran Import-Assets.ps1; a missing asset logs a warning and falls back. |

## Local assets and audio

- **Asset keys.** Ships use `ship.<fac>.<class>` (roster `assetKey`, or the slug of a legacy class name), weapons
  `weapon.<WeaponType>[.<fac>]`; see SHIP-ROSTER.md and `../assets/check_asset_keys.py`.
- **`UBotfAssetRegistry`** (`UGameInstanceSubsystem`) reads `LocalAssets/resolved-manifest.json` at startup
  (`BOTF_RESOLVED_MANIFEST` overrides the path) and resolves a key to a `/Game` object path by walking the manifest
  fallback chain; nothing resolved means the built-in `/Game/Presentation/<Faction>` mesh, then an engine shape.
  Parsing and the walk are in the engine-independent `BotfJson.h` / `BotfAssetCatalog.h`.
- **Combat scene.** `ABotfCombatScene::ApplyShipMesh` gives every participant with a class id its own mesh, scaled by
  canon length (`displayRadiusForLength`, square-root compressed between radius 40 and 170). Ships whose chain ends at a
  placeholder keep the scene's interim art (franger, Ulysses, faction studies).
- **`UBotfAudioSubsystem`** plays cues from `BotfAudioCues.h` (keys, priority, cooldowns, one voice line at a time with
  a 0.6 s gap, 24 sfx with the lowest priority culled, voice ducking of sfx and music) and `BotfAudioEvents.h` (turn
  briefing, tactical round and diplomacy mapping). Hooks: `FinishTurn`, `OpenEncounter`, `TacOrder`, `TacNextRound`,
  `TacAuto`, `Submit` (war, peace and trade proposals). Volumes (master, effects, voice, interface, music) are sliders
  in Settings, stored in `GameUserSettings.ini` `[BotfAudio]`.
- **Object Database.** The ship list shows the legacy catalogue and the 98 expanded roster classes
  (`BotfShipDatabase.h`, via the core's `findShip` / `findRosterClass`).
- `game/Tools/client-logic-tests.cpp` checks all of the above without Unreal; `check-unreal-tu.sh` builds and runs it.

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
| `megaprojectPace(state, empire)` | the megaproject share in `divertIndustry` (min of work per turn and colony industry) | turns left on the megaproject |
| `outcomeCondition(state)` (or a field) | inferred from `victoryProgress` flags and the colony count | the end-of-game overlay's "how" |
| `minors::joinBonusFor(minor)` | the legacy (+10% research) branch of `memberBonuses` | minor profiles of older saves |
| `templateNameProblem(name)` | the 1-48 printable-byte name check of `DefineBuildTemplate` | naming a template from free text |

Other gaps the gameplay layer showed (no formula is mirrored for these, the client works around them):

| Wanted | Why |
|---|---|
| a station flag or type on `CombatParticipant` | records list stations as Destroyer-class; once a station is destroyed a record cannot say whether it was an outpost or a starbase |
| `knownFeatureCells(state, viewer)` | `knownFeatures` admits an ion storm by its centre only; the client draws only the explored cells of its front |
| `wormholeExit(state, feature)` | the client looks the exit up by `pairId` |
| resolved option costs in `EventChoiceInfo` | effects carry both rulesets' prices as text ("150 credits (60 in prototype rules)") |

Save size: `botf::MaxSaveBytes` (8 MiB) is the campaign cap in `botf::deserialize`, which also loads a turn-session
envelope's baseline. The largest soak save is about 51 KiB. The client no longer adds a limit of its own.
