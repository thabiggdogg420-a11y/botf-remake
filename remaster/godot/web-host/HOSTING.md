# BOTF Remastered: web build, hosting notes

This folder is the whole browser game as static files. Upload **everything in it** (including the hidden `.htaccess`), keeping the
layout, to any folder on a static host, for example `https://example.com/botf/`, and open `https://example.com/botf/` (the trailing slash matters).
Nothing is uploaded for you. The game is a private fan project: share the link only with the people you mean to.

## Layout (two builds, one set of packs)
- `index.html`: the loader. It picks `webgpu/` (desktop Chrome or Edge with hardware WebGPU) or `webgl/` (Firefox, Safari, mobile,
  everything else) and sends the player there.
- `webgl/` and `webgpu/`: one complete engine build each (`index.html`, `index.js`, `index.wasm`, `index.side.wasm`, `index.pck`,
  the core wasm, worklets, `sw.js`; `webgpu/` also has `index.shaders.pck` and `wgsl-cache.js`).
- `packs/`: the one shared pack set (both builds read `../packs/`), `manifest.json` and the content-hashed `*.pck`.
- `.htaccess`, `nginx.conf.example`, `HOSTING.md`, `VERSION.txt` (`tools/web/assemble_next.py` puts all of them in the folder).

## What the host must do
1. **Serve the files over HTTPS** (the browser needs a secure page for saves in IndexedDB and for audio).
2. **MIME types**
   | extension | type |
   |---|---|
   | `.html` | `text/html` |
   | `.js` | `text/javascript` |
   | `.wasm` | `application/wasm` (a wrong type makes the engine fail to start; it is the most common host mistake) |
   | `.pck` | `application/octet-stream` |
   | `.json` | `application/json` |
   | `.png` | `image/png` |
3. **No special headers are needed.** The build is single-threaded: it does not use SharedArrayBuffer, so no COOP/COEP headers are
   required, and none should be added (they would block cross-origin images and scripts the page does not use anyway).
4. **Relative paths only.** The page and every download resolve against its own address, so any sub-path works (`/botf/`,
   `/games/botf/`, or the site root). Nothing refers to a disk path or another host.

## Compression (matters for the first load)
The first load is the engine (`index.js`, `index.wasm`, `index.side.wasm`, `libbotf_core.web.template_release.wasm32.wasm`)
plus the game's own `index.pck`. The wasm files compress to a quarter of their size.
- **Precompressed sidecars are in the folder**: `X.gz` for gzip and, where it helps, `X.br` for brotli, beside `index.js`,
  `index.wasm`, `index.side.wasm`, `index.pck`, `index.shaders.pck`, `wgsl-cache.js`, the core `.wasm`, `index.html` and the worklets,
  in `webgl/`, `webgpu/` and the loader. `packs/` has none. Serve the sidecar with
  `Content-Encoding: gzip` or `br` (and the original file's `Content-Type`) when the browser accepts it.
  - Apache: the included `.htaccess` does exactly this (mod_rewrite + mod_headers).
  - nginx: `nginx.conf.example` (`gzip_static on;` and, with the brotli module, `brotli_static on;`).
  - Cloudflare or another CDN in front: switch on its Brotli and gzip compression for `application/wasm` and JavaScript; it then
    compresses on the fly and you can ignore the sidecars.
  - A host that can do neither: upload as is. It works, just with a much bigger first download.
- **`packs/*.pck` have no sidecars and need no compression.** They are already compressed assets (WebP, Vorbis), so a pre-built
  `.br` or `.gz` saves almost nothing (the assembler used to write 286 MB of them that no rule served; it no longer does). The game
  downloads packs with the browser's `fetch()`, which undoes a Content-Encoding a host or CDN adds, so on-the-fly compression is
  harmless but pointless there; just do not rewrite or redirect `/packs/`.

## Caching
- `packs/*.<12 hex>.pck`: the name carries a hash of the content, so a pack never changes under its name. Send
  `Cache-Control: public, max-age=31536000, immutable`. The game also keeps each pack in the browser's own storage
  (IndexedDB), so a returning player downloads nothing for a pack they already have, whatever your cache headers say.
- `index.*`, `packs/manifest.json`, `sizes.json`: `Cache-Control: no-cache` (revalidate every visit; a 304 costs nothing). A new
  release replaces these and the packs whose hash changed; delete the old hashed packs after the upload if you wish.


## Service worker (17 Nov 2026)
Each build folder (`webgl/`, `webgpu/`) has its own `sw.js` beside its `index.html`, and that page registers it (relative path, so any sub-path works; it needs HTTPS or localhost,
and the host must serve `sw.js` as JavaScript with no redirect). It keeps what the player has already downloaded, whatever cache
headers the host sends:
- in this two-build layout the packs live outside the worker's scope (`../packs/`), so the worker does not cache them: the game's
  own IndexedDB copy is the pack cache and a returning player downloads no pack twice. (The flat single-build layout of the first releases
  cached `packs/*.<12 hex>.pck` in the worker too and pruned them against the manifest; `sw.js` still carries that code, now idle.)
- the engine files, including `webgpu/`'s `index.shaders.pck` and `wgsl-cache.js` (17 MB raw, the pre-recorded WGSL pipelines), are cached in a bucket named for the build id, which `hash_web_packs.py` writes into `index.html`
  (`<meta name="botf-build">`) and `packs/manifest.json`; `index.html` and `manifest.json` always go to the network first;
- on a new upload the page carries a new build id, the old bucket is deleted and every cached pack the new manifest no longer lists
  is dropped, so nothing from an old build is ever served with a new page.
It adds nothing to the host: no headers, no rules. If the browser blocks it, the game runs as before from the network and IndexedDB.
A full storage quota only means a file is fetched again.

## Files
- In each of `webgl/` and `webgpu/`: `index.html`, `index.js`, `index.wasm`, `index.side.wasm`, `index.pck`, `index.audio*.js`,
  `index.png`, `index.icon.png`, `index.apple-touch-icon.png`, `sw.js`, `sizes.json`: the engine and the game.
- `libbotf_core.web.template_release.wasm32.wasm`: the simulation core (C++ compiled to WebAssembly).
- `packs/manifest.json` and `packs/*.pck` (top level, shared): ship models by faction, planets, portraits, music, ship pictures, voices.
  They download as the game needs them, not all at once.
- `.htaccess`, `nginx.conf.example`, this file: host notes, not needed by the game.

## Download sizes (measured at release; see `sizes.json`)
There is no total-download budget (Simon, 16 Nov 2026: quality first). Measured on the 22 Nov 2026 build (0.1.566; each build's own figures are in its `sizes.json`):

| What | Raw | Transferred |
|---|---|---|
| First playable load, `webgl/` (engine, core, main pack, title) | 59.3 MB | 19.3 MB gzip (15.2 MB brotli) |
| First playable load, `webgpu/` | 64.8 MB | 21.9 MB gzip (17.0 MB brotli) |
| Every pack in the game (what "Preload whole game" downloads) | about 418 MB | about 418 MB (the packs are not compressible) |

17 Nov compression pass (same build, before and after): all packs 621.0 MiB -> 386.4 MiB (-37.8 %). Hull textures are WebP via
`EXT_texture_webp` (colour q88, normal maps q95; `build_ship_packs.py --web-format jpeg` restores the JPEG route): the 135 hull packs went
448.4 MiB -> 256.7 MiB. The web portraits are WebP q94 (`art_web.pck` 48.8 MiB -> 5.8 MiB). On an 8-hull sample (compared with the resized originals) WebP q85 colour textures came within 0.004 SSIM of the old
JPEG q90 (0.965 vs 0.969) at 47 % of the bytes (q88 ships), and WebP q95 normal maps have a mean angular error of 0.58 degrees (old JPEG q92: 0.42) at 47 % of the bytes.
The structure and governor pictures import lossy (q0.9) so the main pck stays under the 25 MB first-load budget. Audio is unchanged: the voices and
effects are already 32 kHz mono Vorbis q2-q3 and the music is stereo q3; going lower is audible, and the audio is 16 % of the packs.
Hull textures are 1024 px (`--web-max 1024`); normal maps stay at 2048 px (`--normal-max`) and the planets are 2048 px WebP.

The default path is lazy: a player downloads the first load, then only the packs a game needs (a hull is fetched the first time a
battle needs it, about 2-6 MB each). Packs are kept in the browser's storage (IndexedDB), so a second visit downloads nothing that is
already there. "Preload whole game" (title screen and Settings) fetches the rest in one go; the page shows the exact size first.
Cold first load under a 10 Mbps, 60 ms browser throttle: the game core is running after about 20 s, a campaign galaxy about 40 s after launch (measured 16 Nov, before the 17 Nov compression pass).

Background fetching (17 Nov): once the title bundles are in, the game fetches the next packs one at a time while the player is on the title, new-campaign or galaxy screens (never during a battle load, a campaign-start fetch or a preload): on the title `ships_base` and `planets_hi`; once a campaign runs the player's empire audio and every hull of the player's empire, then the rival empires' hulls (`App._idle_prefetch`, `AppLoader.idle_queue`). The preload button stays for the rest (minor-race and monster hulls, the other empires' audio).

Not tested on a live Apache or nginx here: the shipped `.htaccess` and `nginx.conf.example` (22 Nov 2026: the `.htaccess` sidecar rule was
widened to match `webgl/index.pck`, `webgpu/index.pck` and `index.shaders.pck`; before, only a root-level `index.pck` matched, which in this layout does not exist).
Do not add rules that rewrite or redirect `/packs/`.

## Quick check after uploading
Open the address in Chrome, press F12, Network tab: `index.wasm` and `index.side.wasm` must show type `wasm` and a
`content-encoding` of `br` or `gzip`; no request may go to another host and none may fail. The title screen appears, then
New Campaign works.
