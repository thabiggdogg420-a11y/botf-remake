# BOTF remake — first Unreal prototype and simulation

Simon's whole-build scope is in [the beta acceptance matrix](../planning/BETA-ACCEPTANCE.md).
The Unreal prototype uses the same C++ simulation as the standalone test runner.
This is the first interactive foundation, not a finished BOTF remake.
Simon's acceptance standard is original-or-better for every section; the plan's
comparison gate applies throughout. No section currently has demonstrated parity.

## Play the Unreal prototype

Open **PowerShell** in `C:\Users\BiggDogg\Documents\ChatGPT\Game Remakes`:

```powershell
.\game\Launch-Prototype.ps1
```

Or open `Build/Packaged/Windows/BOTFRemake.exe` directly. Keep the surrounding
package directory intact. No administrator rights are needed to play.

Select a known star; owned colonies have the working colony inspector. Queue a facility or choose
a workforce preset, then End Turn to commit orders and settle production.
Cancel Last Draft removes the last pending order. Save Snapshot writes a unique
file under the running game's `Saved/BotfSnapshots` folder after read-back verification.
Load Selected Save works across restarts; Next Save cycles compatible snapshots,
newest first. Saves are unique and are never overwritten. New Campaign starts a
Federation/Klingon development scenario (16x12, 28 systems).

### Explore and settle

Use **Next Fleet** to select a ship, click a sector, then **Move to Sector**. End Turn commits the
order and follows a shortest supplied route, one adjacent sector per turn; further turns keep
moving until arrival. To stop, draft a move to the fleet's current sector.
The fleet panel shows current position, committed route/ETA and any pending route.
Cancel Last Draft removes an uncommitted order (including fleet orders).

Only explored systems appear. Ships reveal their own and adjacent sectors; visited
sectors remain known. Cycle to a **Colony ship**, send it to a discovered unclaimed
system, wait for arrival, then **Colonize Current System** and End Turn. That button
settles the ship's location, regardless of the sector currently selected on the map.
The ship is consumed. Select the new colony to manage construction and workforce.
Its income and production begin on the following turn.

The fixture provides one scout and one colony ship. More can be built at a powered
shipyard. Rival empires now have separate treasuries, research and explored sectors.
Their initial AI expands peacefully, develops yards, builds colony ships and researches.
Foreign fleets are shown only within current sensor coverage; internal foreign
production remains hidden. Last-seen intelligence and sophisticated AI remain unfinished.
Schema 5 accepts schema 4 snapshots through validated in-memory migration; those
continue as their original single-empire scenarios. Schema 1�3 files are preserved
but are not migrated. Terraforming, combat and diplomacy remain later work.

### Shipbuilding and supply

Sol starts with a powered yard. Queue Scout or Colony Ship in the colony inspector;
ships share its construction queue and industry with buildings. One empire-wide
shipbuilding slot is available in this fixture, assigned to the lowest system ID
with a powered yard, a ship at the queue head and positive industry. Other ship
queues pause without banking production. This is a provisional dilithium-capacity
stand-in, not the complete original resource model.

New colonies can queue a Shipyard followed by ships. A shipyard needs five power
after factories and labs; Toggle Shipyard Power drafts a change for the next turn.
The sidebar shows actual power state; scrolling reveals lower controls/queue.
Power plants cost 80 industry and add 10 energy with 1 credit upkeep from the next
turn. If an unpowered ship blocks the queue, queue a power plant, commit it, then
use the queue's Up/Down buttons to move it ahead of the ship. Each project retains
its own invested industry when moved; Remove discards only that project's work.
The queue displays pending edits immediately, while they commit at End Turn.
Cancel Last Draft undoes the edit. Completion surplus is separate from project
work; idle production is not banked. These rules are provisional, not proven original parity.
Paused construction retains progress and explains its blocker. New facilities and
ships become eligible for supply/movement/upkeep on the next turn.

Green map boundaries show the selected ship's supply range; cyan shows the route
to the selected sector; gold shows its committed route. The fleet panel previews
travel time or explains that no supplied route exists. Move is disabled for an
unreachable destination. Next Fleet cycles through player-owned ships; Focus Fleet selects
its sector. Co-located fleets show the selected ship ID plus a count of companions.
Shipyard power loss can pause a committed route; restoring supply resumes it.
Holding position is legal even after losing supply; movement requires a connected
path through supplied sectors (the origin may be just outside the boundary).

### Research and workforce

The Research tab shows all six fields, level/progress, committed percentages and
draft allocation. Focus assigns 50% to one field and 10% to each other field;
Balanced uses 17/17/17/17/16/16. Every point is accounted for. Costs are a synthetic
level ladder, with next-turn effects: Biotech food, Computers research,
Construction industry and Energy power gain 5% per level; Propulsion adds range
every two levels. Weapons points are tracked, but combat integration is pending.
Energy 1 unlocks the fusion plant; Computers 1 plus Construction 1 unlocks the
research institute. This is not the complete BOTF technology/content roster.

Colony workforce can be adjusted in groups of up to five, within population and
facility limits. The draft shows unassigned workers, food deficit and projected
industry/research before committing. Balanced and Industry presets remain.

Close the game/editor before rebuilding. In **PowerShell**:

```powershell
.\game\Build-Unreal.ps1
.\game\Build-Unreal.ps1 -Package
```

Unreal 5.8.3 and the installed Visual Studio C++/Windows/.NET Framework SDKs are
required to build. The `.uproject` opens the editor. The interface currently uses
Slate with simple star markers and synthetic economy values. It is not final art.

## Run on Windows

Open **PowerShell** in `C:\Users\BiggDogg\Documents\ChatGPT\Game Remakes`.
No administrator rights are required. Installed Visual Studio C++ Build Tools
and a Windows SDK are required for compilation.

```powershell
.\game\Build-Simulation.ps1 -Test
.\game\Build\BotfSim.exe --seed 1999 --turns 30
```

The executable prints the generated systems and an economic report each turn.
New games queue a factory, farm and laboratory at Sol. Different seeds change
system positions. This demo does not vary starting colony resources by seed.

Save a pending turn, then resume it. Choose a **new filename** for each save;
the parent directory must already exist. Example:

```powershell
.\game\Build\BotfSim.exe --seed 1999 --turns 0 --save .\game\Build\my-pending.botfsave
.\game\Build\BotfSim.exe --load .\game\Build\my-pending.botfsave --turns 30 --save .\game\Build\my-turn30.botfsave
```

`game/Build/` contains ignored build/test artifacts. It is convenient for test
saves, but keep any campaign saves you want to retain in a separate chosen
folder. Nothing here automatically deletes files. An existing save or unfinished
`.pending-write` file causes an explicit refusal rather than an overwrite.

## Implemented

- Seeded sector galaxy, unique locations and stable system IDs; bounded partial
  shuffle works even when every sector contains a system.
- Multiple owned colonies, workforce/capacity, food deficit reporting, power allocation,
  industry, research totals, treasury income and building maintenance.
- Pending workforce/queue commands, monotonic command IDs, cancellation and
  validation of the full draft. Drafts do not change live colony outputs.
- Atomic turn resolution: commit commands, compute current outputs, complete at
  most one queue product per colony, retain completion overflow, publish validated state.
- Versioned canonical text saves, pending orders and RNG state, checksum,
  bounded parsing, incompatible/corrupt-save rejection and read-back verification.
- Repeatable state hashes and per-colony turn reports for future UI integration.

## Explicit prototype rules: `prototype-shipyards-3`

All numbers below are synthetic fixtures, **not extracted BOTF balance**.
Definitions are currently compiled into `Simulation.cpp`; the external content
loader and full faction/technology tables are subsequent packages. Change
`ContentVersion` whenever rules/data change incompatibly.

| Item | Current fixture rule |
|---|---|
| Map | 12 × 8 sectors, 12 systems by default; one colony named Sol |
| Population | Fixed 40; 20 food workers, 10 industry, 10 research |
| Starting facilities | 2 farms, 2 factories, 1 laboratory, 1 shipyard; 10 workers per ordinary facility |
| Food | 2 per food worker; deficits reported without population consequences yet |
| Industry | 2 per worker supported by powered factories |
| Research | 1 per worker supported by powered laboratories; cumulative points only |
| Energy | Base supply 20 at Sol plus 10 per power plant; factories, then laboratories, then enabled shipyard |
| Maintenance | 1 per ordinary building, 2 per shipyard; scout 1, colony ship 2 per turn |
| Income | Integer population / 2 per turn |
| Construction cost | Farm 60, factory 100, laboratory 80, power plant 80, shipyard 120, scout 60, colony ship 100 industry |
| Construction timing | One completion per colony/turn; output and maintenance of new facilities begin next turn |
| Overflow | Retained after completion; idle production is not newly banked |
| Power requirement | Farm 0; factory/laboratory 5 each, including staffed or unstaffed facilities |

This instantiates only a narrow part of the provisional economy/command plan.
Player-configurable power priority, growth/starvation, morale, diplomacy, combat,
tech unlocks, AI, extracted data and production-quality visuals are
**not implemented**. Negative treasury is currently permitted with a hard numeric
bound; full insolvency consequences are not implemented. Colony construction has
no monetary purchase command yet.

Prototype limits: 64 × 64 sectors; at most 256 systems, 64 queued buildings per
colony and 1,024 pending commands; 1 MiB save input. They bound the initial runner,
not the final game's scope. Colonies and fleets now carry explicit owner IDs, but
the fixture validates only the Federation player. Multiple empire economies and
separate knowledge views must be added before rival AI integration.

New colonies start with population 20, 10 food workers, 10 industry workers, one
farm, one factory, no laboratory, and energy 10. All generated systems are currently
settleable. These are test values, not BOTF data. Movement is one sector per axis
(diagonals allowed); sensors reveal a square radius of one. Scout supply radius is
six sectors and colony-ship radius four, measured by maximum axis distance from a
powered owned yard. Numeric ranges and path tie-breaking are provisional. Base
Sol income is +10 after its buildings and the two starting ships' upkeep.

## Source organization

- `Source/BotfSim/Simulation.h`: plain state, commands, reports and API boundary.
- `Source/BotfSim/Simulation.cpp`: generation, validation, economy, turns and codec.
- `Source/BotfSim/Main.cpp`: terminal demo and filesystem save adapter.
- `../tests/scenarios/SimulationTests.cpp`: authored acceptance scenarios.
- `Build-Simulation.ps1`: discovers MSVC, compiles with `/W4 /WX`, optionally tests.

The simulation has no Unreal types, filesystem access, wall-clock dependency or
rendering dependency. UI work can consume its state/reports and submit commands.
This API is a starting boundary, not a frozen production save/schema guarantee.

## Historical verification — initial economy slice, 26 September 2026

Compiled and executed using installed MSVC 14.51.36231, x64 C++20, optimization
enabled and warnings treated as errors. All **12 scenario groups passed**:

1. Seed reproducibility, distinct layouts, completely filled map and invalid size.
2. Hand-calculated home output and treasury ledger.
3. Power shortage and zero-power output.
4. Deferred commands, cancellation and ID reuse rejection.
5. Workforce changes at commit, excess workers and unowned targets.
6. One completion, carried surplus and next-turn upkeep.
7. Exact cost boundary and no free idle banking.
8. Cumulative queue capacity across pending commands.
9. Failed turn rolls back all settlement and pending actions.
10. Save with pending orders, resume and compare 30 turns.
11. Corruption, truncation, version/content mismatch, trailing/oversized input.
12. Invalid directly constructed state.

Actual CLI disk test: seed 1999, 30 turns. Factory completes turn 5, farm turn 8,
laboratory turn 12. Final credits **485**, research **300**. Direct run and a run
saved at turn 0 and turn 7 produced identical turn-30 save bytes.

- Final state hash: `13666451703072960083` (non-cryptographic deterministic fingerprint).
- Matching file SHA-256: `4221A62C16EDB4AA6D6797162A72B0D82BD12C402BDDA82DFD0D0F0796CB4281`.
- Existing-file save attempt rejected; original SHA-256 unchanged.
- Raw outputs/test saves: `Build/verification-20260926-203451/` (ignored generated artifacts).

These are developer implementation checks, **not an independent review** or
historical BOTF verification. The current save checksum detects accidental changes;
it is not a security/authentication mechanism.

## Unreal and exploration verification

### Unreal verification — 26 September 2026

Editor and Windows Development package builds passed. The rebuilt packaged game
was launched and exercised through its visible interface:

- Tab focused Queue Farm; Enter submitted the draft. The previous non-focusable
  widget error is absent from the new runtime log.
- Farm plus Industry preset remained pending until End Turn: turn 1 produced
  40 industry, 0 research and 115 credits, with 40 stored construction industry.
- Turn 2 completed the farm: 3 farms, 20 surplus industry, 130 credits. Next-turn
  net income displayed +14, reflecting the new building's maintenance.
- Saving turn 0 and loading after turn 2 restored both pending orders, initial
  workforce and resources. Saved/loaded state fingerprints both
  `9601868373846647543`. Cancel Last Draft then removed the workforce order only.
- Earlier editor-runtime check verified unclaimed-system selection disables
  colony orders. The packaged test confirmed the readable turn summary.

Evidence: `Build/package-focus-fix-verification.log` and
`Build/packaged-focus-fixed-play.log` (no `Error:` entries at the checked point).
These are builder checks, not independent review or full gameplay acceptance.

### Exploration slice — 26 September 2026

The C++20 core builds with `/W4 /WX`. All 16 scenario groups pass: the original 12
plus deferred/bounded/cancelled fleet orders and resumed travel; bounded exploration
and persistent knowledge; colony-ship consumption, deferred settlement production,
new-colony development and pending-colonization reload; invalid fleet/knowledge
state and old-format rejection. The initial-slice hashes above are historical:
schema 2 includes fleets, ownership and exploration, so its fingerprints differ.

The Unreal editor/game targets compiled and the Windows package passed. Visible
packaged-game check: both ships routed from (11,1) to (7,3); System-2 appeared on
turn 3, ships arrived on turn 4, and colonization consumed the colony ship on turn 5.
Saving the pending settlement and replaying it after load produced identical
turn-5 fingerprints (`4868390359216746518`). Turn 6 produced from both colonies:
credits 198 (+23), research 60, new-colony laboratory queue with 20 stored industry.
Unknown sectors hid system names and colony controls until discovery/settlement.

Evidence: `Build/exploration-package.log` (`BUILD SUCCESSFUL`) and
`Build/exploration-play.log` (matching save/load/turn hashes; no `Error:` entries
at the checked point). The updated package was left running at turn 6.

### Next systems

Add minors/diplomacy, validated external content, richer economy/population and campaign integration.
Queue editing, initial research and multiple empire economies are now implemented. Basic movement, exploration, settlement and
the Unreal galaxy/colony adapter are implemented. The
compile/package/import pipeline is verified. Keep this test runner as the fast
way to reproduce simulation failures while the graphical game is developed.

### Shipyard and supply slice — 27 September 2026

All 24 C++ scenario groups pass with warnings treated as errors. New checks cover
ship thresholds/launches, next-turn upkeep, non-recycled IDs, shipbuilding saves,
yard prerequisites, duplicate yards, power-loss pauses, one-slot competition,
range edges, extension and loss of supply, disconnected regions, power expansion,
and queue recovery without transferring progress between products.

Source evidence: `Build/shipyards-final-simulation-tests.log`. Save schema is 3;
previous verification hashes remain historical, not current expected values.
Current parity status is **below original**: provisional numbers, incomplete
resource/ship/station content, limited queue controls and no reference playtest.

Packaged verification: `Build/shipyards-final-package.log` reports BUILD SUCCESSFUL.
The visible game built scout #3, paused its construction while the yard was off,
restored range/production after reactivation, and allowed selection and movement
of the new ship. The queue showed remaining turns and its power blocker. An
out-of-range destination disabled Move and displayed an explanation; a valid route
showed a four-turn preview. The new scout launched on turn 4 after the one-turn
power interruption, then paid upkeep and moved on turn 5 (149 credits, 50 research).

Saving the new fleet and pending route, loading and repeating the turn produced
the same turn-5 fingerprint `6559954365747332333`. Evidence:
`Build/shipyards-play.log`; no `Error:` entries at the checked point. Power plant
and queue-recovery semantics were scenario-tested; their whole UI workflow has not
yet been exercised. These are builder checks, not independent review. The packaged
game is left open at turn 5.

### Development and rival empires � 27 September 2026

Core: 32 scenario groups pass with warnings as errors (`Build/empires-simulation-tests.log`).
New coverage: arbitrary project moves/removal/undo preserving invested work; prerequisite
rejection; research rounding/ledger conservation, thresholds, delayed benefits and unlocks;
terminal-level reserves; independent empire economies and exploration; reproducible
100-turn five-empire expansion; schema-4 migration and corruption rejection.

Packaged schema-4 development check: Energy focus committed at End Turn, queue rows
moved a power plant before a factory while retaining the factory's 20 industry, and
saving/restarting restored both pending queue orders. Save/load hash
`6138995273093160545`; following turn matched the CLI hash `7054682628717122317`.
Evidence: `Build/development-package.log`, `Build/development-play.log`,
`Build/development-restart.log`. These hashes are historical after schema 5.

Simon's turn-11 schema-4 session was saved intact before replacing that build.
The schema-5 CLI reads it and resolves turn 12 to hash `11898284288001959325`.
The original file remains unchanged. The multi-empire package compiled successfully
(`Build/empires-package.log`); final compatibility build and live checks are ongoing.

Gate B is still incomplete (notably minors/membership). Gates C�E remain open:
combat, diplomacy/trade/intelligence/events/endings, complete economy/content,
modern 3D presentation/audio/settings, balance/performance and independent review.
These are builder checks, not an independent audit or a beta acceptance claim.

### Diplomacy, reference data and population — 27 September 2026

Current source is schema 7 (`prototype-population-7`). Thirty-nine C++ scenario
groups pass with warnings as errors (`Build/population-simulation-tests.log`).
New campaigns include contact, delayed gifts, friendship, competing membership
bids and next-turn member research/production benefits. Gifts drafted at T now
arrive at T+2; the earlier T+3 bug was reproduced and fixed (before/after logs:
`Build/gift-timing-before.log`, `Build/gift-timing-after.log`). Saved due dates stay
unchanged. Acceptance thresholds and Bynar bonus remain prototype policy P-04.

The diplomacy package passed compilation/cooking (`Build/diplomacy-package.log`).
Its visible run verified exploration-driven Bynar contact, friendship acceptance,
gift charging, the pending delivery display and delivery (relations 60 to 80).
Membership save/replay is scenario-tested; its complete visible workflow remains
pending because the PC is in active use. `Build/diplomacy-play.log` is the evidence.
Navigation label padding and inspector wrapping were corrected and editor-compiled;
the updated population package is being built separately in `Build/PopulationPreview`
to preserve the running executable. Visual re-verification is still required.

Population now follows the S15 growth/shortage equations with P-02 deterministic
rounding. Growth uses fixed-point arithmetic, shortages use exact integer square-root
boundaries, food is capped for the growth calculation, and employment/capacity limit
growth. Prototype capacity 100 and base rate 2% are explicit fixtures. New citizens
remain unassigned; losses consume unassigned population first, then trim industry,
research and food assignments to prevent overstaffing. Existing production uses its
pre-completion snapshot; newly founded/joined colonies can grow immediately but begin
production next turn. Draft workforce forecasts do not consume random numbers.
Schema 4–6 migrations retain frozen population rules for existing sessions; new
campaigns enable growth. Planet-based capacity, terraforming and growth modifiers
remain open. The 100-turn five-empire regression also passes with growth enabled.

The local reference exporter has read 77 technology records and 125 ship records
from Simon's installer, hashing 20 tables and preserving source IDs/unknown data.
Two binary-parser test groups pass. Those tables are staged reference data, not yet
a replacement for the synthetic gameplay content. See `research/BOTF-RESEARCH.md`.

Gate B still needs integrated visible gameplay verification. Gates C–E remain open,
including combat, major diplomacy/trade/intelligence/events/endings, complete content,
modern 3D presentation/audio/settings, balancing and independent review. This remains
an early playable prototype, not a working beta or an original-or-better acceptance.

Population verification update: all 40 C++ scenarios pass
(`Build/population-final-simulation-tests.log`), including explicit schema-5 and
schema-6 migrations preserving saved gift dates. Five empires completed 1,000 turns
and a verified save (`Build/population-five-empires-soak.log`, final hash
3713491597290260581). This is stability evidence, not balance or AI quality acceptance.
The packaged population run grew Sol from 40 to 41, previewed a two-person shortage
loss, saved that pending order, and resolved turn 2 to hash 10103338355475793025.
Loading and repeating produced the same hash; the standalone CLI reading that actual
snapshot also matched. Save/load hash: 6935322607592679245. Evidence:
`Build/population-play.log`. Mouse activation was unreliable during shared-PC use;
focused buttons were activated with Enter. Mouse-only verification remains open.
The smaller-window check still clipped Diplomacy; navigation slots were changed to
size to their labels and a follow-up package/visual check is in progress.

## Parallel campaign integration - 27 September 2026

Schema 8 adds war/peace, ship damage/retreat, invasions, trade, intelligence,
events and victory/defeat. Full screens expose these orders and reports; campaign
setup supports all five player factions. The 3D scene illustrates structured
battle reports; it is not tactical playback. One volley per strategic turn and
synthetic content remain beta blockers.

Sol built separate campaign/UI lanes. Luna independently found a multi-rival
defeat validation bug; the fix/regression brings standalone coverage to 49 groups.
The CampaignPreview package predates that final fix and must be replaced. Editor
compile and package passed after one crashed Unreal shader worker was retried.
Evidence: Build/campaign8-editor-retry.log and campaign8-package-retry.log.

Live mouse checks started a five-empire Ferengi game, rejected a destroyer with
its technology requirement, saved/loaded snapshots and displayed a deterministic
completed battle. Advancing it changed hull 9 to 6: turn-2 hash
13936582967417696438, Build/campaign8-play.log. Mouse-only navigation now works.
Source fixes for overexposed scene lighting, repeated capture requests, colony
button layout and clipped map text await the next package.

Icewolf132's community Ranger/Ulysses/Lyran meshes converted with textures and
imported into Unreal. Ranger geometry/UVs were visually checked in Blender.
It is an interim Federation illustration; other factions remain silhouettes.
These old meshes prove conversion, not the final modern-art quality gate.

## Reference campaign integration - 27 September 2026

ReferencePreview packaged successfully (Build/reference9-package-retry.log).
Schema 9 supports sourced six-field research and faction ship names, hulls,
requirements, costs and maintenance. Ruleset is persisted; migrations preserve
old prototype campaigns. The reference encyclopedia includes 77 technologies,
125 ships and 318 buildings, including records not implemented as playable content.
Combat now resolves bounded rounds and records participants and per-round losses.

53 standalone scenario groups passed. Independent review then identified unsafe
retreat destinations and stale retreat orders; their fixes are in the next core
work block. Live package checks verified reference startup with Oberth hull 19,
source research names/costs, and the compact colony construction/workforce page.
Battle round controls, revised exposure and full encyclopedia navigation still
need live verification. Additional inspector/research layout changes postdate
this package. ReferencePreview is a development checkpoint, not the beta.

All six completed Icewolf132 models are imported. Original readme and conversion
manifest are retained in ArtSource/Community/Icewolf132; future packaged builds
copy them into Windows/Credits/Icewolf132. Asset reuse is the preferred art route,
but the current imported art is interim and the other four factions still need
suitable modern models. Default Launch-Prototype.ps1 still targets the older
Build/Packaged package; use the specifically named preview when verifying it.

Deadline integration check, 27 September: ReferencePreview live verification
confirmed first/final combat rounds show the appropriate surviving/withdrawn
ships, and the encyclopedia search displays all 11 source-only Sociology entries
without treating them as a seventh active research field. Battle-control clipping
and image aspect ratio were found and corrected in source for the next package.
Evidence: Build/reference9-lighting-check.log. Original Packaged saves remain in
place; the next PlaytestPreview package will be staged separately.

### Frontend and Gemini integration checkpoint - 27 September 2026
- Added title/Continue/New/Load/Settings/Quit shell, in-game menu, unsaved-campaign confirmation, and settings panel with reversible video changes. Gemini built the settings panel; separate reviewers checked it. Runtime testing found stale saved-display values on entry; Gemini corrected live viewport refresh and FPS-only persistence. Corrected code compiled and packaged successfully in Build/FrontendPreview (frontend-package.log).
- The current FrontendPreview package is an intermediate verification build, not a release candidate: independent review reproduced damaged AI leaving a repair yard and an unsafe colony fallback bypassing fleet-strength checks. Gemini is correcting these before a final package. Do not treat its 63 passing scenarios as sufficient acceptance; independent adversarial cases exposed gaps.
- Initial runtime video timeout rollback was verified. Corrected settings/menu runtime acceptance remains open because Computer Use did not expose the newly launched packaged window. Audio, finished fleet art, tactical interaction and full original-game parity remain incomplete. No beta-complete claim.
- Existing PlaytestPreview and its saves remain available. Launcher default has not been promoted to FrontendPreview pending correction and verification.
- Follow-up independent validation: Gemini corrections pass all65 standalone scenarios under MSVC /W4 /WX. The separate ReviewAiPacing.cpp harness confirms the damaged rival stays at its safe powered yard and repairs, and a weak scout avoids the adjacent defended colony. Actual faction5 (--player5 with5empires)200turn soak matches100+100reload hash15356853636376133800; battle count fell159to5. No invasions/captures in that seed by200turns, so conquest pacing remains incomplete.
- Simon authorized Anthropic source sharing and requested Claude Opus5.5 low-effort UI redesign. Claude CLI updated from2.1.269 to2.1.283 via official npm after built-in update failed. Exactmodel respondedREADY. Claude now owns UI-only redesign; separate review/build/runtime verification still required.
- FrontendPreview updated successfully with the independently reviewed65-scenario AI corrections. Packaged live checks: title/settings, FPS-only60cap without displaychange, NewCampaign, EndTurn, Menu, SaveAndQuit passed. Turn1save created and normal exit confirmed. Persisted GameUserSettings hasFrameRateLimit60. ComputerUse recovery worked by resetting its connection and launching the inner gameexecutable directly. Launcher now defaults to this verified FrontendPreview checkpoint; PlaytestPreview remains available separately. Claude redesign is not in this package yet.
- Resumed live testing: Continue restored saved turn1 correctly afterrestart; 60FPScap displayed correctly afterrestart; Escape opened menu; loadbrowser selected/loaded shieldbattlefixture; replay FirstRound restored all3participants and FinalRound showed surviving2withcorrectshieldstate; oldschema4turn11save loaded through packagedUI with204credits. Battlepresentation hasmissingstarbackground and overlappingFedships; independent sceneowner is fixingthose, pendingrebuildandvisualretest.

## Command preview verification - 27 September 2026

Default Launch-Prototype.ps1 now opens Build/CommandPreview/Windows. Integrated Claude UI and combat presentation package completed successfully (Build/command-package.log). Older FrontendPreview remains available via -PackageDirectory.

Independent headless Unreal BOTF.UI.Shell test compiled and passed (Build/ui-automation-run.log): real title/setup/campaign transitions, dirty confirmation/cancellation, isolated snapshot save/load fingerprint roundtrip, save-before-replace, settings entry/exit and quit-request protection. The initial test used an invalid third-faction/two-empire combination; corrected to five empires. Normal setup already guards this combination. Test snapshots are isolated under Build/UiAutomationSnapshots; test does not prove video-mode rollback or rendering.

Packaged visual smoke check at 3440x1440: title, campaign setup, galaxy, colony management, campaign menu, load browser and save-before-load confirmation render and operate. Battle fixture final round shows visible stars, separated surviving ships, destroyed enemy hidden and differentiated shield strength. Artwork is still provisional; stars are oversized and ships small. Smaller-window layouts and all faction themes still need visual verification. This is a verified checkpoint, not completion of the full beta plan.
