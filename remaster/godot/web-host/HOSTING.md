# BOTF Remastered: web build, hosting notes

This folder is the whole browser game as static files. Upload **everything in it**, keeping the layout, to any folder on a
static host, for example `https://example.com/botf/`, and open `https://example.com/botf/` (the trailing slash matters).
Nothing is uploaded for you. The game is a private fan project: share the link only with the people you mean to.

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
  `index.wasm`, `index.side.wasm`, the core `.wasm`, `index.html` and the worklets. Serve the sidecar with
  `Content-Encoding: gzip` or `br` (and the original file's `Content-Type`) when the browser accepts it.
  - Apache: the included `.htaccess` does exactly this (mod_rewrite + mod_headers).
  - nginx: `nginx.conf.example` (`gzip_static on;` and, with the brotli module, `brotli_static on;`).
  - Cloudflare or another CDN in front: switch on its Brotli and gzip compression for `application/wasm` and JavaScript; it then
    compresses on the fly and you can ignore the sidecars.
  - A host that can do neither: upload as is. It works, just with a much bigger first download.
- **Do not compress `packs/*.pck`.** They are already compressed assets, and the game's downloader cannot take a
  content-encoded body. If your host or CDN compresses everything, exclude `/packs/`.

## Caching
- `packs/*.<12 hex>.pck`: the name carries a hash of the content, so a pack never changes under its name. Send
  `Cache-Control: public, max-age=31536000, immutable`. The game also keeps each pack in the browser's own storage
  (IndexedDB), so a returning player downloads nothing for a pack they already have, whatever your cache headers say.
- `index.*`, `packs/manifest.json`, `sizes.json`: `Cache-Control: no-cache` (revalidate every visit; a 304 costs nothing). A new
  release replaces these and the packs whose hash changed; delete the old hashed packs after the upload if you wish.

## Files
- `index.html`, `index.js`, `index.wasm`, `index.side.wasm`, `index.pck`, `index.audio*.js`, `index.png`, `index.icon.png`,
  `index.apple-touch-icon.png`: the engine and the game.
- `libbotf_core.web.template_release.wasm32.wasm`: the simulation core (C++ compiled to WebAssembly).
- `packs/manifest.json` and `packs/*.pck`: ship models by faction, planets, portraits, music, ship pictures, voices. They download
  as the game needs them, not all at once.
- `sizes.json`: the measured file sizes (raw, gzip, brotli).
- `.htaccess`, `nginx.conf.example`, this file: host notes, not needed by the game.

## Download sizes (measured at release; see `sizes.json`)
Measured on the 16 Nov 2026 build (0.1.394), served gzip-compressed (brotli is about 20% smaller again):

| What | Raw | Transferred (gzip) |
|---|---|---|
| First playable load (engine, core, main pack, title) | 57.6 MB | 17.7 MB (13.7 MB brotli) |
| In a new campaign's galaxy (cumulative, includes the first load) | n/a | 68 MB (measured through a gzip-on-the-fly host) |
| Every pack in the game (what "Preload whole game" downloads) | 382.8 MB | 342.9 MB |

The default path is lazy: a player downloads the first load, then only the packs a game needs (a hull is fetched the first time a
battle needs it, about 1-3 MB each). Packs are kept in the browser's storage (IndexedDB), so a second visit downloads nothing that is
already there. "Preload whole game" (title screen and Settings) fetches the rest in one go; the page shows the exact size first.
Cold first load under a 20 Mbps, 40 ms browser throttle: the game core is running after about 12 s.

Not tested here: the shipped `.htaccess` (Apache) and `nginx.conf.example`. The game downloads packs with the browser's `fetch()`, so
a host or CDN that compresses on the fly is fine; do not add rules that rewrite or redirect `/packs/`.

## Quick check after uploading
Open the address in Chrome, press F12, Network tab: `index.wasm` and `index.side.wasm` must show type `wasm` and a
`content-encoding` of `br` or `gzip`; no request may go to another host and none may fail. The title screen appears, then
New Campaign works.
