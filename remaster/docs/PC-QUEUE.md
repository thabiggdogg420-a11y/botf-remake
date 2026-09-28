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
2. [ ] **Online audio downloads** (the main audio route; see AUDIO-SOURCES-ONLINE.md, "Recommended acquisition order").
   - TrekCore audio library, archive.org Sound Source Interactive TOS/TNG AudioClips, the voice packs from Armada Fleet Command's BOTF Ultimate Mod 5, and LCARS UI-sound repos.
   - Save them into the same downloads folder as item 4. `Import-Assets.ps1` converts and indexes them.
3. [ ] **Optional: original BOTF audio extraction.** This is a fallback now.
   - `python remaster\game\Tools\Extract-BotfAudio.py` against your own install, if you want the original voice lines alongside.
4. [ ] **Asset downloads.**
   - Download candidates from `remaster/docs/ASSET-SOURCES.md` into one folder, then run `remaster\game\Import-Assets.ps1 -Downloads <folder>`.
   - Requires Blender (with NifTools for .nif), ffmpeg and 7-Zip.
   - `-UnrealImport` imports into `/Game/Local`.
5. [ ] **Model orientation check.**
   - After import, open a few ships in the editor and fix any `forwardAxis` override the automatic guess got wrong.

## Blocked cloud work (waiting on the items above)

- Verifying the Unreal client compiles (needs 1).
- Tuning ship scale, weapon socket positions and VFX against the real models (needs 4 and 5).
- Automatic voice cue mapping, and tuning mixer levels against the real audio (needs 2).
