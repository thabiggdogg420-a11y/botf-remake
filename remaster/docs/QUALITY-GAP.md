# Quality gap: screenshots Simon saw vs packaged beta v3 (audit, 10 Nov 2026)

Read-only audit by Odo. No Godot or exe launched. Not committed.
Simon, 9 Nov: "substantially further away from the quality i thought it was at ... better UIs, and a way better looking 3d galaxy type map ... now it all looks very basic again."

## Verdict up front

1. NO frame of the packaged exe exists. Every "v3" frame under `remaster\captures\2026-11-09\<time>\` (screens, smoke, galaxy, battles, compat) is a Godot DEV run from the repo (Vulkan Forward+, RTX 4090, `captures\2026-11-09\134817\logs\pc-smoke.log` line 2) on the dev PC, which has `game\LocalAssets`. Those frames already OVERSTATE what the exe shows. The only frames from the shipped artefacts are the web ones (`captures\2026-11-09\v3-web\`, Compatibility renderer, 1280x720).
2. No unmerged branch explains the gap. CONFIRMED: `git branch --no-merged` empty; all 25 worktree HEADs (wt-m0-*, wt-m1-*, wt-r*) are ancestors of main df18064; origin/claude/ui-redesign and hopeful-franklin are merged too. No scene for galaxy/UI was deleted or reverted (`git log --diff-filter=D -- scenes/galaxy` shows only a debug .uid). Caveat: several worktrees hold UNCOMMITTED edits (wt-m1-webparity 32 files incl. battle_environment.gd, space_sky.gdshader, galaxy_sky.gdshader, title_fleet.gd, title_screen.gd; wt-m1-weaponfx 17 files). That is work not in v3 (it is "web parity" and weapon FX; it may touch sky/look). Not examined line by line.
3. The "better" shots are of three kinds: (a) the frozen UNREAL client (29-30 Sep), (b) a STAGED 80-system Godot galaxy capture (8 Nov), (c) dev-PC Godot frames with hero ships. Each is better than the exe for a different reason.

## Which screenshots Simon most likely saw (ranked)

| Rank | Set | Build | Date | Path |
|---|---|---|---|---|
| 1 | Unreal showcase shots: title, galaxy, battle | Unreal (frozen 7 Nov) | 29-30 Sep, copied 7 Nov | `remaster\docs\pc-screens\` (15 png; HANDOFF.md lines 153-207 name them) |
| 2 | 3D galaxy zoom strips and contact sheets | Godot dev, Forward+, staged | 8 Nov 13:37 | `wt-m0-galaxy3d\remaster\captures\galaxy3d\forward_plus\` and `sheet-forward_plus-0/1/2.png` (worktree only, not in main) |
| 3 | Godot UI/battle contact sheets | Godot dev with LocalAssets | 8-9 Nov | `remaster\captures\2026-11-09\134817\sheet-*.png`, `wt-m1-ui\...\141439\` |
| 4 | Older Godot doc shots | Godot dev | 7 Nov | `remaster\godot\docs\screens\` |

Rank 1 and 2 fit "better UIs" and "way better 3d galaxy type map" best.

## Title

- Better: `remaster\docs\pc-screens\title-menu-live.png` (Unreal, 30 Sep). Big "BIRTH OF THE FEDERATION" wordmark, painted teal/violet nebula panorama, Galaxy-class hero fleet in glossy detail, glass-panel main menu, five-empire chips.
- Beta v3 comparison: `remaster\captures\2026-11-09\134817\smoke\pc-01-title.png` (dev run). Fed fleet is there but only because the dev PC has LocalAssets. Cleaner, flatter, plainer purple sky, orange block buttons.
- Packaged frame: NEEDS CAPTURE.
- Cause 1 (PROVEN): packaged exe has no LocalAssets; `scenes\battle\battle_assets.gd:115-121` looks for `$BOTF_RESOLVED_MANIFEST`, `<exe dir>/LocalAssets`, `res://../LocalAssets`, `res://../game/LocalAssets`; the beta folder has none, and `title_fleet.gd` falls back to procedural hulls. Title fleet becomes basic hulls.
- Cause 2 (PROVEN by comparison): the Unreal title was a hand-set cinematic with painted `T_SpaceNebula` panorama; Godot uses a procedural sky shader (parity table row "Skybox"). Never ported as art.
- Cause 3 (SUSPECTED): wt-m1-webparity has uncommitted edits to `title_fleet.gd`/`title_screen.gd`/`space_sky.gdshader`, not in v3.
- Fix: ship LocalAssets (other lane); port the wordmark/scrim layout and a painted panorama; then recapture from the exe.

## Galaxy map (3D)

- Better (a): `remaster\docs\pc-screens\galaxy-nebula-territory.png` (Unreal). Painted nebula panorama, glowing spectral-class stars with real names, translucent territory wash and borders, left inspector, right inbox, bottom order dock. Six stars visible and readable.
- Better (b): `wt-m0-galaxy3d\...\forward_plus\strip-12-huge80-galaxy.png`, `strip-15-anomalies.png`, `strip-16-anomalies-close.png` (Godot dev, 8 Nov). A tilted disc of 80 ringed stars in five empire colours, glowing territory borders, volumetric anomalies (orange accretion disc, pink/blue clouds). This is the shot most likely meant by "way better looking 3d galaxy".
- Beta v3: `remaster\captures\2026-11-09\134817\smoke\pc-03-galaxy.png` (dev), `...\galaxy\ui-galaxy-open.png`, `...\v3-web\web-galaxy.png` (web, nearly black: a ring, one star "RIGEL", a dozen dots).
- Differences: v3 default view is a sector zoom (1.6x) on one colony with 2-3 named stars and fog; no disc of stars, no colour-coded empires, no volumetrics; web has no sky at all.
- Cause 1 (PROVEN): the huge80 frames are STAGED. `scenes\galaxy\tools\galaxy_strip.gd:62-76,149-153`: `_fill_huge` injects 70-odd client-side stand-in systems because "the core only reports systems the player knows (5 at turn 10, no reveal call in the bridge)". Real play at default Medium (`new_campaign_vm.gd:22`, 28 systems, fog) never shows that picture. The code that draws it is in main (5708b5f merged via 80100d2) but the content does not exist in a live game.
- Cause 2 (PROVEN): the Unreal screenshot had a painted panorama and bright stars; Godot galaxy sky is procedural (`galaxy_sky.gdshader`) and much dimmer. Never ported art.
- Cause 3 (PROVEN, web only): `project.godot:25` sets `gl_compatibility` for web, which drops sky/glow/volumetrics (compat captures in `galaxy3d\compat\` show the same black field).
- Cause 4 (SUSPECTED): the 3D map's first view opens zoomed in; the galaxy-scale hero view is only reached by scrolling out.
- Fix: for the beta, reveal more of the map (or start zoomed out with all stars of the chosen size dimly charted), brighten the sky and star glow, restore the painted panorama; keep the huge80 staging out of "proof" shots.

## Galaxy map (2D / sector and anomalies)

- Better: `remaster\captures\2026-11-09\134817\sheet-galaxy.png` (dev): giant colour wormhole/black-hole/ion/rift billboards. Those are close-ups of anomalies placed 3 cells from Sol by the capture tool, not the default view.
- Beta v3 default: pc-03-galaxy. Cause: capture tool staging (SUSPECTED, anomalies are placed by `screenshot.gd` galaxy shots). Packaged capture: NEEDS CAPTURE.

## System view

- Better: `remaster\captures\2026-11-09\134817\screens\ui-system.png` is RICH (sun, five photoreal planets, stat tiles, governor, queue). It matches or beats older `godot\docs\screens\ui-system.png` (7 Nov). No Unreal equivalent exists in pc-screens.
- Gap: none found in layout. Planet textures come from `packs\planets_desktop.pck`; the pck is beside the exe, so likely fine. Planet pack mount in the exe UNVERIFIED (`app_loader.gd:112-117`).
- Packaged frame: NEEDS CAPTURE (confirm the planets are photoreal, not flat).

## Colony / build, fleets, research, diplomacy, empire, intel, database

- Better: `captures\2026-11-09\134817\screens\ui-*.png` vs the 7 Nov `godot\docs\screens\ui-new.png`/`ui-research.png`: v3 is the M1-UI redesign (fbe3915) and is better than the 7 Nov Godot UI. Unreal had no better versions in pc-screens except the Slate tutorial overlay (`controls-help.png`).
- So "better UIs" is NOT the Godot UI regressing. The better UI he saw is the Unreal Slate redesign (`origin/claude/ui-redesign`: battle records, radar, setup galaxy preview; showcase shots 25-43 of 29 Sep, only 15 kept in pc-screens). Never ported: tactical radar, battle inspector (friendly/hostile fleet cards, left rail), orders grid with hotkeys, turn inbox, text-free HUD chips on the galaxy.
- Packaged frames: NEEDS CAPTURE for all of them.

## Battle

- Better: `pc-screens\battle-hero-chase.png`, `battle-hero-overview.png` (Unreal): full-bleed 3D viewport across the window, large hero ships, gas giant filling a quarter of the frame, violet nebula, torpedo orbs, left fleet cards, right radar and orders. Beta v3: `smoke\pc-11-battle-cinematic.png`, `pc-09b-plan-phase.png` (dev): the 3D view is a 1050x610 box inside the LCARS frame, ships small, large chrome (red alert, orders, systems, log).
- Cause 1 (PROVEN): LocalAssets absent in exe, so ships are procedural hulls (compare dev `battles\ui-battle-closeup.png`, a detailed NCC-71879 Galaxy, which the exe cannot show).
- Cause 2 (PROVEN by history): M0 L4 made the combat screen a presentation layer over CombatVM (13c8654) and M1-UI (fbe3915) wrapped it in the LCARS design system; the viewport shrank. Unreal's full-bleed layout was never the Godot target (BATTLE-DESIGN.md outranks the parity table).
- Cause 3 (PROVEN, web): web uses Compatibility; hull textures grainier (READ-ME known issue).
- Fix: LocalAssets for exe; consider a "cinematic" mode that hides chrome and lets the viewport fill the window.
- Packaged frame: NEEDS CAPTURE.

## Ranked causes by share of the gap

1. Unreal-era art direction never ported: painted nebula panorama, bright named stars, big glowing title, full-bleed battle, Slate battle HUD (PROVEN, parity doc and image comparison).
2. Staged captures overstate the product: huge80 galaxy (`_fill_huge`), anomaly close-ups, dev PC with LocalAssets (PROVEN).
3. LocalAssets and hero models missing from the exe (PROVEN; another lane fixing).
4. Galaxy fog and Medium default: a live game never shows the disc of stars (PROVEN by the code comment).
5. Web Compatibility renderer downgrade (PROVEN for web only; desktop uses Forward+, `project.godot:24`).
6. Uncommitted web-parity/weaponfx worktrees (SUSPECTED, unexamined).

Screens never ported from Unreal: painted skybox/panorama, battle Slate HUD (radar, hostile cards, hotkey grid), turn inbox, title wordmark layout, galaxy territory wash look, controls-help overlay (parity table also lists tutorial, confirm-or-revert display and quality preset as missing).

## Needs a fresh packaged capture

All screens, from the exe itself: title, new campaign, galaxy default, galaxy zoomed out, system, empire, research, fleets, diplomacy, intel, database, plan-phase battle, cinematic battle, battle end. Run once on the exe with no repo and no LocalAssets, and once with LocalAssets beside it, to size cause 3.
