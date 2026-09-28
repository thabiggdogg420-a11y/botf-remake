# BOTF review beta

Release status: development paused after Gemini quota exhaustion on 27 September. The last playable editor-runtime build is 15:07:42; newer source is not release-ready. No standalone ReviewBeta package has been promoted.

This is an early playable beta for Simon to inspect, not a claim of parity with the original Birth of the Federation. Development pauses after delivery until Simon provides feedback.

## What to assess

- Galaxy-first layout and navigation between system, research, diplomacy, intelligence, empire and battle views.
- Double-click an owned system to manage it. Right-click a sector for contextual actions. Use the mouse wheel to zoom; Tab cycles ships.
- Queue colony construction, assign workers, allocate research and issue fleet orders; End Turn commits orders.
- Use the campaign menu for Save, Load, Settings and Quit. Ctrl+S saves. Esc closes the current sheet or opens the menu.
- F1–F7 select Galaxy, System, Research, Diplomacy, Intelligence, Empire and Battles. Ctrl+Enter requests End Turn.
- In manual combat, choose orders and advance rounds; Auto Finish resolves the remaining encounter. Continue commits the strategic turn. Battle records include a recorded 2D replay alongside the 3D presentation.

## Known limits to retain in the final handoff

- Full original-game parity is not demonstrated. Balance, AI diplomacy and faction-specific behaviour need further development and broader campaign testing.
- Modern social-engine features and the visible diplomacy/intelligence controls must be distinguished in final release notes; backend code alone is not a playable feature.
- Ship art, animation, audio and combat presentation remain interim. The current 3D scene shows at most eight ships per side; the tactical simulation and ship lists may contain more.
- Some original capabilities remain absent, including structure upgrades and hailing. Scrapping currently returns no credits.
- Numerical reconstruction choices and unresolved original-table mappings remain provisional.
- Existing packages and saves are preserved. Use the exact final release path below rather than an older preview folder.

## Verified release record

Pending. Record build ID, executable path, test evidence and remaining observed defects here after the live build and package checks.

## Quota checkpoint stocktake — 27 September

Use PowerShell to launch the preserved playable build:

```powershell
& 'C:\Users\BiggDogg\Documents\ChatGPT\Game Remakes\game\Launch-Prototype.ps1' -EditorRuntime
```

The newer source contains diplomacy/intelligence UI controls, campaign consistency changes and additional tactical maneuvers. It has not been compiled into this playable build. Native tests compile, but the Simulation suite stops after87 passes on a new gift-test timing expectation and the Session suite stops after2 passes on mid-battle save/replay divergence. Tactical8/8, Intelligence14/14 and Diplomacy14/14 pass; new maneuvers still need dedicated coverage. UI fixes are independently source-reviewed, not runtime-verified.

Ten converted asset candidates and tooling exist, but the independent asset review must be resolved before import. The modern 3D scene upgrade is unfinished. Combat, ship-asset and mechanics-improvement research is complete and reviewed. No new work is running. Detailed resume evidence: ../setup/output/BETA-RESUME-HANDOFF.md.
