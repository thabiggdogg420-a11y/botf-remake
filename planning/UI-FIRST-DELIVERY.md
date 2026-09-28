# Next delivery: a usable BOTF interface

**Prompting update:** Simon subsequently requested much shorter, outcome-focused builder prompts with design freedom. Use [BUILDER-PROMPTS.md](BUILDER-PROMPTS.md) when dispatching Claude or Gemini. The detailed material below is Astra's review and integration checklist, not a prescribed implementation or a brief to paste wholesale to builders. Builder design choices may satisfy the user workflows through different layouts and interactions.

Authority: Simon's 27 September feedback supersedes the sequencing in the earlier restart handoff. He rejected the shipped placeholder layout and map interactions, and requested preparation while Gemini is quota-paused. The full-game brief remains the eventual scope. This document is the next delivery contract, not a replacement for implementing it.

## What actually exists

- Launch-Prototype.ps1 still opens Build/CommandPreview/Windows. Simon played the old package; the new source was never delivered in an executable.
- New UI source exists, but the first Unreal compile failed. Claude repaired the Slate constructor argument limit in source; the repair has not been compiled or visually checked.
- BotfGalaxyMap.h:197-202 implements right-click through a zero-argument Context callback, ignoring the clicked sector. BotfGameMode uses that callback to open the generic screen selector. That is not object-context interaction.
- No OnMouseButtonDoubleClick exists in the galaxy widget. No mouse-wheel zoom or map pan handler exists there either. These are source findings, not a visual review.
- BuildCommandColumn still places global readouts, six navigation buttons and a scrolling screen-specific panel in one persistent column. Changing colors or labels on this composition cannot by itself establish original-or-better usability.
- Six independent combat-session scenarios passed. Fourteen of fifteen long campaign cases passed determinism/validation. The known turn171 group-supply blocker remains. Intelligence and diplomacy modules are not yet integrated into campaign state.

## Next delivery order

1. **Astra establishes a working integration baseline.** Check owners/processes; build the released UI repair and run existing headless shell/session checks. Send precise errors to the responsible builder. A compiled current screen is a baseline to inspect, not an accepted redesign. Record the source revision/hashes and package build identity so an old executable cannot be presented as new progress.
2. **Claude rebuilds the player workflows and layout.** Use the concise Claude prompt in BUILDER-PROMPTS.md and include Simon's criticism as context; leave layout and interaction design to Claude. Read/view original tutorial screens and compare against actual current screenshots. Produce the galaxy, contextual fleet orders and system management as one coherent playable section. Root captures reference/current comparisons in the same review artifact; source-only assertions are insufficient. Reuse styling and working bindings, replace inherited placeholder composition where it obstructs the workflows.
3. **Gemini fixes campaign blockers in parallel.** The first core assignment is the saved turn171 group-supply reproduction and real terraforming work/cost correctness, using the existing soak-fixes brief. Freeze public interfaces during the first UI section except agreed necessities. An independent Gemini lane may finish diplomacy's bounded corrections; do not delay the first UI package for unrelated social integration.
4. **Sol/Luna independently verify.** Sol checks command correctness, cancel/replace atomicity, session/save regression and source/build identity. Luna checks original-reference fidelity, information hierarchy and unsupported gameplay claims. Actual visual/input verification uses the packaged executable, not a reviewer reading code and declaring the UX good. Nobody approves their own implementation.
5. **Astra packages and launches the new section.** Use a distinct package directory and a visible build identifier. Exercise the acceptance matrix below at 1280x800, 1920x1080 and the user's 3440x1440 display. Preserve the old package and all saves. Only after this gate passes, switch the default launcher to the verified new package, then launch it and check its visible identity. Tell Simon exactly what changed and show screenshots of the delivered build.
6. **Continue the full beta on that foundation.** Integrate diplomacy/intelligence with verified APIs and real original-derived screens, complete production/upgrades/research/logistics gaps, improve models/effects, and run player-driven campaign conclusion tests. Full beta gates remain in BETA-ACCEPTANCE.md. Do not stop the overall build after the first UI delivery unless Simon asks or a real blocker requires it.

## Claude's interaction and layout contract

The existing UI is a source of backend bindings, not an approved layout. Start from the original galaxy/system workflows, retain BOTF's information density and empire identity, and modernize readability and direct manipulation. A generic dashboard with decorative faction colors does not pass. Global screen navigation may exist, but must not be the main way to interact with map objects.

| Player intent | Required delivered behavior | Acceptance exercise |
|---|---|---|
| Select a sector/system | Single click updates an immediately useful local inspector; retains fleet selection when plotting requires it | Select owned, unowned, unknown and foreign sectors; no hidden enemy data |
| Open a system | Double-click an owned star opens its management screen; double-click a known foreign/unowned star opens allowed information, never ownership-only controls | Test real double clicks, including returning to the same map selection |
| Inspect/select fleets | Distinct fleet/group hit targets; co-located groups can be chosen without cycling every ship in the empire | Select two groups and an ungrouped ship in one sector; verify IDs/orders |
| Give contextual orders | Right-click is anchored to the clicked object/sector and preserves the commanding fleet. Offer legal actions, with reasons for unavailable relevant actions. It must not merely open screen navigation | Right-click a reachable destination, unreachable destination, enemy, own colony, unclaimed system and blank space |
| Issue and revise movement | Course, ETA/supply and pending state visible; replacing a draft is atomic; Cancel/Hold works without changing screen; closing a menu alone never issues an order | Plot A, replace B, cancel; compare committed state/preview, then End Turn exactly once |
| Manage a task force | Form, inspect, redeploy/split and give group orders with clear member selection; group range is the limiting member's range | Mixed-range group cannot preview an impossible shared move or split silently |
| Navigate the map | Readable names/icons, zoom and pan that preserve hit-testing and selection; practical limits and reset/focus control; keyboard alternatives | Zoom/pan then select/right/double-click near all edges; no accidental order |
| Manage a colony | Planet strip and meaningful population/food/growth/industry/energy/credits; production allocation and queue available with little navigation; Buy/Scrap show consequences | Change workers, queue/reorder/buy/cancel, inspect terraform progress, return to map |
| Read feedback | Selection, pending orders, warnings and outcomes distinguishable without relying on color alone; no development text filling main player panels | Bad order explains cause; good order confirms target; tooltips stay on screen |
| Fight and resume | Encounter choice, all ships selectable, explicit target/maneuver, round resolution and save/load recover exactly | Fight, order, round, save, menu/settings, load, finish, strategic turn commits once |
| Use global flows | Title, setup, settings, save/load and quit remain clear and reachable; no fake campaign-size choices | Start each faction; settings rollback; unsaved exit and mid-battle exit protection |

Modern right-click context menus, pan/zoom and direct double-click navigation are deliberate usability requirements from Simon's feedback and this design. Do not claim all details reproduce original controls. Check original references for actual layout and semantics; mark uncertain reconstructions separately.

## Evidence and stop conditions

- Keep one compact evidence table: requirement, source reference or deliberate change, build identity, executed check, pass/fail, remaining issue. Headless action tests support logic, but do not prove pointer hit-testing, readable layout or actual double-click routing.
- Review screenshots at practical resolutions against original screens. Essential actions must not require scrolling through a tall generic button list; no clipped menus or microscopic text.
- A placeholder labelled unavailable is honest but does not count as completed original functionality. Record gaps explicitly and continue building them.
- A build that merely compiles is not a delivery. A delivered UI section is not yet the full beta. No percentage based on lines of code, time spent or number of tests.
- Do not dispatch the same task to two owners or let builders endlessly launch duplicate background builds. Use bounded assignments, explicit release, independent review, and one integration build per meaningful revision.
- Gemini stays the main backend builder after quota reset, Claude UI only, Astra integrates, Sol/Luna review. During this pause, preparation does not authorize replacing Gemini with Codex for implementation.

## Ready inputs

- Original material: research/reference-local/aio-1.0.1-vanilla/app/Vanilla/tutor1.pdf and tutor.txt; research/BOTF-RESEARCH.md and BETA-FIDELITY.md. Read actual rendered reference pages for layout, not text alone.
- Claude conversation: d93f77eb-6ed5-44f7-967b-7ef5b0bf2a1b. Prior repair result: setup/output/claude-beta-ui-review-fixes.json. This new contract takes priority over prior claims of UI completion.
- Gemini campaign conversation: 19e0786b-88ed-4d77-8aca-da322b915880. Brief: setup/output/gemini-beta-campaign-soak-fixes.txt. Saved reproduction: game/Build/CampaignReview/Failure-26713-player4-turn171.sav.
- Diplomacy conversation: e39411c3-5011-40ed-b0ab-183ff1880098; final correction interrupted by quota. Intelligence released; adapter must bound runtime inputs. See setup/output/BETA-RESUME-HANDOFF.md for full state and remaining integration brief.
- Commands after ownership check: game/Build-Unreal.ps1, game/Test-UnrealUi.ps1, game/Build-Simulation.ps1 -Test -Tactical -Sessions. Include module switches only after their owners release files.
