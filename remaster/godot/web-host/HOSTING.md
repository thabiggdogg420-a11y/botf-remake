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
SIZES_PLACEHOLDER

## Quick check after uploading
Open the address in Chrome, press F12, Network tab: `index.wasm` and `index.side.wasm` must show type `wasm` and a
`content-encoding` of `br` or `gzip`; no request may go to another host and none may fail. The title screen appears, then
New Campaign works.
