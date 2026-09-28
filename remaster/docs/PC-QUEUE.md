# PC-side work queue

These tasks can only be done on Simon's Windows PC, for example a UE 5.8 build, the local asset
downloads, or a check against the real BOTF install. They are held here while cloud development
continues. When cloud work can go no further without them, this file lists what to run, in order.

Status markers: `[ ]` pending, `[x]` done (paste the result or log into the PR/session).

## Pending

1. [ ] **First real UE 5.8 build.**
   - Run `remaster\game\Package-Windows.ps1 -CheckOnly`, then run it again without `-CheckOnly`.
   - If it fails, paste back `Build\Logs\<stamp>\FAILURE-SUMMARY.txt`. The script also copies it to the clipboard.
   - Risk list: UNREAL-CLIENT.md, "First real compile: risk list".
2. [ ] **BOTF audio extraction.**
   - Run `python remaster\game\Tools\Extract-BotfAudio.py` with the path to your own BOTF install.
   - Confirm `english_0000.wav` and a few sfx play.
   - Report any file the extractor rejects.
3. [ ] **Voice cue mapping.**
   - Fill `LocalAssets/botf-original/cue-map.json`, using Ultimate Editor to identify race and group, or wait for the automatic `converse.bin` mapping.
4. [ ] **Asset downloads.**
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
7. [ ] **Editor-side audio setup (optional polish).** If imported WAVs play too loud or too quiet relative to each other, create Sound Classes (Voice, SFX, UI, Music) and a Sound Mix under `/Game/Audio` and report their paths; the subsystem currently sets per-component volume only.

## Blocked cloud work (waiting on the items above)

- Verifying the Unreal client compiles (needs 1).
- Tuning ship scale, weapon socket positions and VFX against the real models (needs 4 and 5).
- Automatic voice cue mapping, and tuning mixer levels against the real audio (needs 2).
