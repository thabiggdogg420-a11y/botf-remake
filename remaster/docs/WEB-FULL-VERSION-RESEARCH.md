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
