# BOTF Remastered: finish plan (16 Nov 2026)

Outranked by `AI_stuff\standing\simon\botf-rulings.md` (Simon's words, including the 16 Nov answers). Inputs: MASTER-PLAN.md, research/MOD-PARITY.md, the beta v8 READ-ME known limits, RESUME-2026-11-10.md.

## Start point
- Beta v8, build 0.1.382, main `8538a08` (pushed to the private repo 16 Nov). Save schema 25.
- Simon on v8: "the battle Sim is much better than it was before". Not a pass of the "like the shows" bar; F2 carries it on.
- Done: M0 refactor, M1 galaxy/hulls/Trek pass/weapon FX, M2 gen and empire, MOD-1 to MOD-8 and MOD-10, M4 combat flight, Bridge Commander audio, Trekify round 2.

## Decisions (16 Nov)
- Push to the private GitHub repo: yes.
- Online models: Simon has OK'd every file the work needs. No account sign-ups or payments without asking. Record source and licence of each file in `docs/ASSET-SOURCES.md`.
- "Ultimate mod" = Dafedz's UM5 Redux; minor counts and difficulty percentages stay as built (lead's call on Simon's "you decide").
- Hosting: the browser build goes on Simon's website for his friends. F1 builds a host-ready folder; the upload itself waits for his site access details.

## Phases
Each phase ends in a packaged beta Simon plays. At most 2 lanes at once. Barclay builds, O'Brien packages and plumbing, Odo audits, Tuvok takes doc and research slices. Builders look at every frame; the lead looks again before handover.

| Phase | Work | Done when | Beta |
|---|---|---|---|
| F0 Housekeeping | Push main (done). Merged worktrees: archive any untracked files, then remove the worktree (branches stay). Fix the intermittent crash while importing sound files during packaging. | No stale worktrees; three clean import runs in a row | none |
| F1 Browser | Browser shows the full galaxy disc. First load and total download inside the web budget (faction and hull packs lazy-loaded and cached, lighter browser hulls). wasm core builds from any lane. Full browser play-through: title, new game with presets/scenarios, galaxy, every screen, a battle, save/load. A host-ready static folder (relative paths, correct MIME/gzip notes for Simon's host). | A remote-style load (throttled) plays end to end; Odo audit passes | v9 |
| F2 Combat to show standard | Beams from round 1; per-round 55 ms and first-open 320 ms hitches gone; camera never holds on empty space; glare and explosion white-out capped; top bar and rail fold away in rounds. Side-by-side with DS9 "Sacrifice of Angels". | Lead's frame review against the show clip, then Simon's verdict | v10 |
| F3 Planets and people | 3D system view (star, orbiting textured planets, structures). Hail scenes (silhouette bust on a viewscreen, per ruling 10 Nov). Busts for Dominion, Borg, Breen, Mirror powers; Terran portrait. Voice acknowledgements on orders. Advisor hints and a first-run tutorial. | New player reaches turn 10 unaided | v11 |
| F4 Content and models | Online models then Blender for Ferengi, Xindi, Suliban, Andorian, Vulcan, Bajoran, Talarian, Nausicaan, K-7, stations; flagship models; Mirror Romulans and Ferengi; House of Duras personality. MOD-9: Dominion voice, intro sequence, Borg cutscenes. End-game speeches. Ground-combat step on invasion. | No stand-in or procedural hull in a normal game | v12 |
| F5 Whole game | Hotseat then LAN multiplayer (private). Encyclopedia with 3D viewers. Autosave slots with thumbnails. Music identified and adaptive. AI-vs-AI soaks to every victory type on every preset. Balance and performance passes. | Full campaign on each victory type without a blocker | v13 release candidate |
| F6 Extras | Real-time and helm combat modes (parked in MASTER-PLAN §1.5). | Simon's verdict | v14 |

## Machine rules (unchanged)
One heavy job at a time (`Documents\ChatGPT\.botf-heavy.lock`), BelowNormal priority, `--audio-driver Dummy`, stall watch on every running lane, full suite once per job at the end.

## F2 status (17 Nov, Barclay)
- Built, awaiting the lead's frame review and Simon's verdict. Proof: `Documents\ChatGPT\f2-captures` (frame-time probe before/after, SoA strips, packaged 1v1 clip). Probe: `scenes/battle/tools/f2_probe.gd`.
- Hitches: the round-1 stall (hulls built on the round's first frame, 335-470 ms) is gone (the round waits for the hulls); round starts 55-90 ms -> under 34 ms (timeline built on a worker thread, resolve / playback / panels on separate frames). Battle open: worst frame 135-150 ms -> 80-95 ms; what remains is the combat screen's first layout.
- Beams draw in every round the core fires one. Rounds 1-3 of a drill have no beam shots in the core (beam range 4 cells, torpedo 7; the fleets deploy further apart): a rules question for Simon.
- 18 Nov (Simon's ruling, botf-rulings.md): the fleets deploy inside beam range, so phasers and disruptors fire from round 1; weapon ranges unchanged. Core: the two lines deploy at x=6 and x=9 on rows 3/5/7 (stations on the same lines), every ship within 4 cells of every enemy (`deployColumn` / `deployRow` in TacticalCombat.h); test: a beam shot in round 1 for every preset across five seeds. f2_probe drill: 10 beam jobs in round 1. The turn-20 fingerprint is unchanged (16091701257822506776, native and wasm). Web: kill fireballs and hull hits keep their orange (the Compatibility renderer clamped the 3-5x hot core to white). Burning hulls' fire and smoke live 0.4-1.1 s (no long trailing plume).
- Battle-open frame (80-95 ms): not removed. The frame after goto("combat") is main-thread process time (86 ms), not rendering: goto is 27 ms on the frame before, the view mount 7 ms; handing the warmed viewport over (render buffers and shadow atlas pre-allocated) changed nothing (86.5 ms), so it is the new screen's first Control layout and text shaping, which needs the profiler to split.
- Camera: a tracking shot replaces any shot that shows no readable hull for 0.25 s. Glare: flares capped at 32% of the frame height, glow 0.4, flash lights capped. Top bar and rail fold away during rounds.
- SoA side-by-side fixes: thinner phasers, kill fireball and hull hits as orange fire instead of white streaks.
