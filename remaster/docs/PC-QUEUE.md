# PC-side work queue

These tasks can only be done on Simon's Windows PC, for example a UE 5.8 build, the local asset
downloads, or a check against the real BOTF install. They are held here while cloud development
continues. When cloud work can go no further without them, this file lists what to run, in order.

Status markers: `[ ]` pending, `[x]` done (paste the result or log into the PR/session).

## Pending

0. [ ] **Create accounts** in the order of the sign-up checklist in ASSET-SOURCES-LOGIN.md: Nexus Mods, Steam, Freesound, ModDB, the BOTF forum at armadafleetcommand.com, then Discord/Patreon. Then check the login-gated leads marked [unverified] and download what fills a gap.
1. [x] **First real UE 5.8 build.** Done 2026-09-29: two compile fixes (80eab60), packaged, UI tests pass; see HANDOFF.md "PC session 2026-09-29".
   - Run `remaster\game\Package-Windows.ps1 -CheckOnly`, then run it again without `-CheckOnly`.
   - If it fails, paste back `Build\Logs\<stamp>\FAILURE-SUMMARY.txt`. The script also copies it to the clipboard.
   - Risk list: UNREAL-CLIENT.md, "First real compile: risk list".
2. [ ] **Online audio downloads** (the main audio route; see AUDIO-SOURCES-ONLINE.md, "Recommended acquisition order"). Partly done 2026-09-29: the no-login TrekCore and archive.org clips are in `C:\Dev\botf-downloads`, 22 keys resolved and 121 sounds imported; the voice packs still need logins (item 0).
   - TrekCore audio library, archive.org Sound Source Interactive TOS/TNG AudioClips, the voice packs from Armada Fleet Command's BOTF Ultimate Mod 5, and LCARS UI-sound repos.
   - Save them into the same downloads folder as item 4. `Import-Assets.ps1` converts and indexes them.
3. [ ] **Optional: original BOTF audio extraction.** This is a fallback now.
   - `python remaster\game\Tools\Extract-BotfAudio.py` against your own install, if you want the original voice lines alongside.
4. [ ] **Asset downloads.** 2026-09-30: 14 hero ship models downloaded (logged-in browser) and imported through
   `Tools/blender_hero_ships.py` + `Tools/ue_import_hero_ships.py`; see HANDOFF.md "PC session 2026-09-30". Still
   open: the remaining classes use generated hulls (Keldon and Nagus are now hero variants, 2026-09-30).
   - Download candidates from `remaster/docs/ASSET-SOURCES.md` into one folder, then run `remaster\game\Import-Assets.ps1 -Downloads <folder>`.
   - Requires Blender (with NifTools for .nif), ffmpeg and 7-Zip.
   - `-UnrealImport` imports into `/Game/Local`.
5. [ ] **Model orientation check.**
   - After import, open a few ships in the editor and fix any `forwardAxis` override the automatic guess got wrong.

6. [ ] **Asset registry and audio in the real build** (after 1, and 4 for real assets).
   - Compile risks for the new subsystems are in UNREAL-CLIENT.md ("First real compile: risk list", last rows).
   - After `Import-Assets.ps1 -UnrealImport`, check `LocalAssets/resolved-manifest.json` has `ueAssets` paths that open in the editor (the UE import script rewrites them with the real object paths).
   - Start a battle: the log line `BOTF assets: N keys from ...` should appear, and a resolved class (for example `ship.fed.galaxy`) should show its own mesh at a sensible size. Report any ship that is huge, tiny or rotated.
   - Settings, Audio: move each slider and confirm the level changes and survives a restart (`Saved/Config/Windows/GameUserSettings.ini` `[BotfAudio]`).
   - End a turn with research or a battle and listen for the beep/alert and advisor voice; say whether ducking (effects dipping under voice) feels right.
7. [ ] **Rules 3 battle scene: visual tuning and socket placement in the editor** (after 1; 4 and 5 for real models).
   - 2026-09-30: the battle camera is interactive now (HANDOFF.md, "Battle camera redesign"). In a real battle try the
     wheel (zooms toward the cursor), right-drag (pan), click a ship (select), double-click a ship (follow) or empty
     space (fly there), F / G / Home. Say how the speeds and the marker density feel.
   - Start a Remastered (rules 3) battle and watch a round in every camera mode, including the new CINEMATIC one.
   - Check that ships fly from their start pose to their end pose without popping, and that they bank the right way
     into turns and strafes. `PoseRotation` in `BotfCombatScene.cpp` assumes that ship tops face the default camera
     (-Y); flip the up vector or the bank sign if the ships look wrong.
   - Beams, pulses and torpedoes should leave the hull, not the ship's centre.
     - For each imported model, open the static mesh and add or check sockets `WPN_Beam_NN`, `WPN_Pulse_NN` and
       `WPN_Torp_NN` (or the manifest's `WPN_<Type>_<Loc>_<NN>`) at the emitters and tubes.
     - Report models without sockets whose bounds-based hardpoints look wrong.
   - Check the effect sizes against real models: beam width, torpedo glow, the shield bubble on impact, hull sparks,
     explosion size and debris, the cloak shimmer and warp-out. The numbers are in `TickTimeline`. Report any that are
     too large or too small.
   - Optional: create Niagara systems at `/Game/Local/VFX/...`, set their object paths in the resolved manifest under
     the `vfx.*` keys, and confirm that they replace the fallbacks. The user parameters are `User.Color`,
     `User.BeamStart`, `User.BeamEnd` and `User.Progress`.
   - Listen for weapon fire and impacts in time with the visuals, and for barks ("shields at 50%", "torpedo away",
     "target destroyed") once the `vo.combat.*` lines exist. Say whether the round length (6 s,
     `SetTimelineRoundSeconds`) feels right.
   - Try a battle with more than 8 ships per side (the new cap is 24) and report the frame rate.
8. [x] **Editor-side audio setup (optional polish).** Not needed (2026-09-29): the 21 mapped cues measure -17.8 to -24.2 dB RMS, within 7 dB, so the per-category gains are enough. If imported WAVs play too loud or too quiet relative to each other, create Sound Classes (Voice, SFX, UI, Music) and a Sound Mix under `/Game/Audio` and report their paths; the subsystem currently sets per-component volume only.

9. [ ] **Unreal Insights profile of a 24-v-24 battle** (`Run-Showcase.ps1` plus `-trace=cpu,gpu,frame`); record the top costs in HANDOFF.md.
10. [x] **Merge the Slate UI redesign** from `claude/ui-redesign` after review of its showcase screenshots. Already merged: `origin/claude/ui-redesign` is an ancestor of the working branch (checked 2026-09-30).
11. [ ] **Fresh-clone check:** run `game\Build-GeneratedArt.ps1` on a clean checkout (needs Python with numpy and Pillow, and Blender 5.x). The generated art is gitignored.

## Blocked cloud work (waiting on the items above)

- Keep the Unreal client compiling: every cloud change to `remaster/game/Source` should be followed by a PC build (`Build.bat BOTFRemakeEditor`) and a `Run-Showcase.ps1` run.
- Tuning ship scale, weapon socket positions and VFX against the real models (needs 4 and 5).
- Automatic voice cue mapping, and tuning mixer levels against the real audio (needs 2).
