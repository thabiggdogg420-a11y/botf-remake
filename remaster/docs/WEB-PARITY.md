# Browser build: visual parity with the desktop (19 Nov 2026, Barclay)

Outranked by `AI_stuff\standing\simon\botf-rulings.md` (19 Nov: "why does the browser version look so bad? Do we not have
direct parity?"). The WebGPU / Forward+-on-web question is the lead's: `WEB-FULL-VERSION-RESEARCH.md`.

Method: the packaged exe (Forward+, clean folder, private APPDATA) and the web export in headless Chrome on the real GPU
(ANGLE D3D11, RTX 4090, `--mute-audio`, page 1536x864 CSS at devicePixelRatio 1.25 = a 1920x1080 canvas) run the same
`screenshot.gd` shots on the same seed (424242): title, galaxy, system, research, diplomacy, hail, battle-plan,
battle-volley, battle-kill (held 0.75 s into the first kill), battle-burning (held on the first ship under half hull).
Tool: `Documents\ChatGPT\webparity-captures\capture.ps1 -Tag <tag>`, sheets `sheets.py` (before row over after row).
For fast checks the editor runs the same shots under `--rendering-method gl_compatibility` on the desktop: it showed the
renderer gaps without the web packs, which is how the web-only causes below were separated out.

## Gap list (before, 0.1.482 code) and causes
| # | Screen | What differs | Cause | Status |
|---|---|---|---|---|
| 1 | hail | Desktop exe showed the red silhouette bust, the web showed the Gowron portrait | Missing asset on the desktop side: verify.ps1 copied planets/ships/ship_icons packs beside the exe but not `art_desktop.pck` | Fixed: the exe now ships it; both show the portrait |
| 2 | system view | Desktop sun has a wide warm halo; web sun a bare disc | Renderer: Compatibility's glow does not fire on an 8-bit frame (Forward+ blooms the HDR core) | Fixed: CompatBloom (below). Web halo is whiter and tighter than the desktop's warm one |
| 3 | battle: fire, engines, muzzle flares | Desktop: orange halos round fires and flares, red/orange star flares; web: no halo, flares near white | Renderer: no HDR range (colours over 1.0 clip toward white per channel) and no working glow | Partly: CompatBloom works (proved on the editor's Compatibility run) but is off in battle, see 5 |
| 4 | battle: kill fireball | Web lobes smoother, hot core clipped | Renderer: no HDR range. An HDR 2D buffer (use_hdr_2d) restored the lobe shape, but Godot 4.5 Compatibility then skips the tonemapper and every lit hull and the title planet blow out to white | Not fixable in Compatibility 4.5; HDR kept as opt-in `BOTF_COMPAT_HDR=1` |
| 5 | title, battle: hero hulls | Web hulls read white and speckled (tiles of bright specks, sandy Cardassian hulls), desktop hulls grey with panel detail | Web-only, not the renderer: the editor's Compatibility run with the desktop PNG textures draws clean grey hulls (hull mean 144 vs Forward+ 162, 0.3 % clipped). So it is the web hull packs (1024 px WebP albedo, lossy WebP normal/ORM) or how they load. Bloom spread the specks into a white haze, so bloom is off on battle and title | Open: next step is a web pack with lossless normal/ORM to isolate it. Fixed on the way: WebP glb images were not decoded on the loader thread (PNG decoder on WebP bytes) |
| 6 | galaxy | Web backdrop bluer (background +15-25 blue levels), the home-system glow dimmer | Renderer: dust disc is added in 8-bit sRGB (F1 boosts it 5x, `COMPAT_DISC_STRENGTH`); lowering it to 3 darkened the disc without fixing the hue | Open |
| 7 | battle: smoke trail of a burning ship | Desktop shows grey puffs, web none in the held frame | Not a renderer gap: GPU particles do not replay on a seek, so a held frame shows whatever the wall clock left | No action |
| 8 | battle state | Web battle-kill/burning frames showed "Engagement concluded" | Capture tool: the deferred "Enemy contact" choice opened late on the web and the drill auto-resolved under it | Fixed in screenshot.gd (`_take_command`). The web drill still reaches its first kill in round 1 where the exe does in round 2, so the battle frames are the same drill, not the same instant |
| 9 | title | Different power filmed | Capture: random title empire | Fixed: `BOTF_TITLE_EMPIRE` also read as a web user argument |
| 10 | UI, text, DPR | None visible | Canvas is 1920x1080 at DPR 1.25 already (canvas_resize_policy 2, Godot sizes the canvas in device pixels); fonts identical | Nothing to do |
| 11 | MSAA | None visible at 1080p | Settings' MSAA 2x applies to Compatibility SubViewports too | Nothing to do |
| 12 | SSAO, volumetric fog, SSR | Not used by the desktop look (art bible), nebula already faked (CompatNebula) | - | Nothing to do |

## What was built
- `scenes/battle/compat_bloom.gd` + `shaders/compat_bloom*.gdshader`: bloom for a Compatibility 3D view. Bright pass with a
  soft knee, downsampled to 1/4 and 1/16 in two 2D-only SubViewports, screen-blended by the view's SubViewportContainer
  material. Three cheap full-screen passes, no 3D work, nothing under Forward+. On: system view (sun halo), galaxy map.
  Off: battle and title, until gap 5 is fixed (`CompatBloom.attach` is one line to put back).
- `CompatLook.hdr()` / `viewport()`: the HDR-buffer experiment (opt-in), `CompatLook.flag()` reads switches from the
  environment or web user arguments.
- `battle_assets.gd`: WebP glb images decode and mip on the loader thread.
- `verify.ps1`: `art_desktop.pck` beside the exe.

## Sizes
No download change: no textures were added or enlarged (first playable load 19.0 MB gzip / 14.9 MB brotli against the
25 MB budget; every pack 405.9 MB transferred). Higher-resolution hull textures were not tried: gap 5 shows the problem
is not resolution, and the 1024 px cap stays for GPU memory (F1 note below).

## Proof
Sheets: `Documents\ChatGPT\webparity-captures\sheets\parity-<screen>.jpg` (before row: 0.1.482 code; after row: this
release). Before battle rows are the 3D view alone; after rows are full frames (`SHOT_ROOT=1`) so the web bloom shows.

## 19 Nov follow-up (0.1.488, Barclay)
- Gap 5 cause found: not the WebP packs. The editor on Compatibility drew the web WebP glbs clean, and the WebGPU trial drew the same packs clean. The white, speckled hulls on the web title (a cold visit has no hull packs, so the title draws procedural hulls) and on the battle starbase come from `hull.gdshader`. Its lit-window grid is smaller than a pixel at range and runs at energy 4, so on Compatibility (no HDR, no tonemap roll-off) it clips and aliases into white speckle. Fix, Compatibility only: windows fade under ~3 px, energy is capped at 1.2, roughness has a floor at range, albedo is x0.8. No pack change and no size change (19.0 MB gzip first load).
- Also fixed: the web (no worker threads) loaded hero glbs without the hero entry, so `mip_scene_images` never ran (`_shared_model(path, hero, root)`).
- CompatBloom is back on battle and title. Phaser, star-flare and engine glow show in the browser.
- Gap 6: the disc's cool dust is warmer and greyer on Compatibility (`COMPAT_DISC_COOL`). The web backdrop is now neutral grey like the desktop.
- Still open: the web title's procedural hulls are brighter than the desktop's hero hulls (different models by design on a cold visit). The kill fireball halo is unconfirmed: the web drill's held kill frame is a different instant (gap 8).

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
