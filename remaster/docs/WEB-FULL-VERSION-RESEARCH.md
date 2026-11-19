# Full game in the browser: research (19 Nov 2026)

Outranked by: `AI_stuff\standing\simon\botf-rulings.md` (19 Nov browser parity; 16 Nov hosting on Simon's site, low cost).

## Why the browser looks worse
Upstream Godot (4.5 and the current 4.7) exports to the web through WebGL 2 with the Compatibility renderer only. Forward+ and Mobile need WebGPU, which upstream Godot does not support on the web. So the browser loses glow, SSAO, volumetrics, SDFGI and full HDR. On top of that, we ship it lighter WebP textures.

## Options
| Option | Look | Cost | Risk |
|---|---|---|---|
| A. Polish WebGL (the parity pass now running) | Closer, still not full | Free | Low |
| B. Community WebGPU fork (Shane-Gadsby/godotwebgpu, MIT) | Full Forward+: glow, SSAO, SDFGI, volumetrics, TAA, FSR. 87/87 Forward+ tests pass in Chrome. | Free | Medium. One-person fork on Godot 4.8-dev (we are on 4.5); no known shipped game; Chrome/Edge solid, Firefox mostly, Safari untested; runs at about 45-75% of native speed |
| C. Cloud streaming (the PC renders, the browser gets video) | Exactly the desktop | GPU host about $250-380/mo, or tie up Simon's 4090 | Breaks the low-cost goal |

## Recommendation
Do B as a trial alongside A, and ship both builds: the page checks for WebGPU and loads the full Forward+ build, otherwise it falls back to the WebGL build. Trial steps:
1. Copy the project to a branch.
2. Upgrade it to the fork's 4.8 base.
3. Export with the fork's prebuilt templates.
4. Compare the same frames with the desktop.
5. Measure FPS and load size in Chrome, Edge and Firefox.
If the trial fails, A stands on its own.

## Sources
- Godot web export docs: https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_web.html
- Godot 4.7 release coverage: https://app.cinevva.com/news/2026-06-19-godot-4-7-released
- WebGPU fork: https://github.com/Shane-Gadsby/godotwebgpu
- Port of the fork to 4.7.1 in a game client: https://github.com/StudfarmStudios/godot-aw3/pull/31
- Streaming costs: https://www.strayspark.studio/blog/pixel-streaming-ue5-cloud-gaming-demo , https://bloggeek.me/is-webrtc-really-free

## WebGPU trial round 3/r4 (19 Nov 2026, Barclay)
- Cause of the freeze, from Chrome traces: not WGSL translation alone. (1) The GPU process compiled 78 render pipelines synchronously (DXC, up to 3.9 s each) on the thread that also presents frames; (2) about 400 material shader stages that the export bake does not cover were translated SPIR-V to WGSL on the main thread (13-15 s).
- Fix 1, fork patch `remaster/tools/webgpu-fork/botf-async-all-and-wgsl-cache.patch` (on fork commit c4b5f43eb): every render pipeline goes through createRenderPipelineAsync while `globalThis.godotWebgpuAsyncAll` is not false, and pending draws are skipped. `godotWebgpuPending` counts what is still compiling. Compute is left out, because skipping one-shot compute work made hulls render black.
- Fix 2, shipped WGSL cache: WGSL recorded from play-throughs and keyed by SPIR-V hash goes in `wgsl-cache.js` (17.7 MB raw, 2.0 MB gzip). The driver checks it before translating, which took runtime translation from 402 stages to 0.
- Game: `webgpu_warm.gd` starts the off-screen battle warm-up on the galaxy screen and waits for pending to reach 0, then turns async-all off.
- Result: the shader stall is gone and the first battle's worst frame dropped from 12.6 s to 0.26-3.8 s. The 250 ms goal is NOT met. The frames still over it come from single-threaded main-thread loading (boot about 4.7 s, battle scene and hull setup about 3.4 s on a new profile). The shipped WebGL build has the same problem and worse: boot 17.9 s, battle 2.7 s.

## Round 5: shipped as two builds, loading spread over frames (19 Nov 2026, Barclay)
- Host folder `Documents\ChatGPT\BOTF-Web-Next\`: loader `index.html`, `webgpu\` (this branch, fork + async-all + `wgsl-cache.js`), `webgl\` (live tip), one shared `packs\` (pages set `globalThis.BOTF_PACKS='../packs/'`), `VERSION.txt`, `HOSTING.md`. Built by `BOTF-WebGPU-Trial\tools\assemble_next.py`; tested on a plain static host (no COOP/COEP, sub-path `/botf/`) by `tools\host_server.py` + `tools\host_run.py`, cold and returning profiles, through the loader.
- Game changes (live branch, cherry-picked here): web boot compiles the game's scripts a few per frame behind a "Starting" panel; App start-up one step a frame; hero glbs load a step per frame (parse, scene, one image's mips per step, plating, pack) because the web has no worker threads (ResourceLoader threaded loads run inline there); title backdrop a piece per frame; `LOADT` console trace of any loading step of 20 ms or more.
- Result (Chrome, worst frame): boot 4.3 s -> 2.2 s; returning-profile battle open 2.6 s -> 1.0 s; cold battle open and round 1 stay under 250 ms. 250 ms everywhere is NOT met. What is left, from the trace: engine start before any game code (about 2 s), GPU-process pipeline compiles while the title first draws (1.2-2 s frames with no game code running), procedural lead hull material creation on WebGPU (1.4 s, shader compile in wasm), the GLTF parse of a big hull (`append_from_file` decodes every embedded image: up to 1 s) and the first galaxy plus battle warm-up (1.5-1.9 s). On WebGL (Firefox) the first draw still freezes 12-15 s for ANGLE shader compiles; returning visits do not cache them in Firefox.
- Next steps: parse glbs with textures discarded and decode them per frame ourselves (halves the parse and removes the double decode); bake the remaining material shaders so wasm never runs glslang at runtime; warm hulls when their packs mount on the galaxy.
