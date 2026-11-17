# Browser build: what does not work or differs (16 Nov 2026, F1)

Played from the host-sim `/botf/` sub-path on the real GPU (Chrome, ANGLE D3D11, RTX 4090), captures in
`Documents\ChatGPT\f1-browser-captures\`. Everything below not listed here worked in that run: title, New Campaign (Remastered
preset; Dominion War, Klingon Civil War and Mirror Universe scenarios), galaxy, every screen (colony, council, science, starfleet,
diplomacy, intel, database), Starbase drill battle with hail (truce) and rounds, save then page reload then Continue and Load,
Preload whole game (762 MB, survives a reload, a new game and a battle then fetch nothing).

## Gaps and risks
- **GPU memory in battle.** Hull textures are 1024 px in the browser build (lead decision, 17 Nov 2026: `build_ship_packs.py --web-max 1024`,
  passed by verify.ps1) so a small GPU keeps its WebGL context; the earlier all-2048 build held about 3 GB of textures on the 4090. Normal maps
  are still 2048 px (`--normal-max`); lowering them too (`--normal-max 1024`, all-1024 was 0.85 GB) is the next step if a small GPU still drops.
- **WebGL context loss** was seen twice under software rendering (SwiftShader) after about 85 s, never on the real GPU. The page now
  shows a "reload, Continue picks up your autosave" overlay instead of a black screen (`export_presets.cfg` head_include). Engine
  cannot recover a lost context itself.
- **Not driven in the browser this pass:** pack editor and user packs persisted in browser storage (code path is the same
  `AppLoader` file layer that saves use, so it should persist), the checksum refusal, 2- and 8-power games with Dominion/Borg/Breen
  and threats (the 8-power New Campaign setup screen was set up and looked right; the game was not launched), eras other than
  Early/Expanded, Diplomacy drill, Trek sounds (audio muted in the driver; the cue pack download path was exercised, not the sound).
- **Not tested:** the shipped `.htaccess` and `nginx.conf.example` on a real server; the interrupted-preload resume.
- **Throttled run:** one run at 10 Mbps / 60 ms: engine running about 20 s after open, campaign galaxy about 40 s after launch.
- Warning in the console, harmless: "FS.syncfs operations in flight at once".
- Fixed in this pass (desktop too): Quit to title kept the Klingon/Mirror palette on the title and New Campaign screens; the
  title column ran off a 720 px window when a Continue button exists.
