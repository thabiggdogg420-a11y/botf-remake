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
