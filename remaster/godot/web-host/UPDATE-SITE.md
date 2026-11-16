# Updating Free4allhub.com/botfremaster

This folder is the complete new build. To update the live site:

1. Upload the whole contents of this folder to `/botfremaster/` on the site, replacing what is there.
2. Delete any files on the server under `/botfremaster/` that are not in this folder (old packs have hashed names like `fed.1a2b3c4d5e6f.pck`; leaving them wastes space but breaks nothing).
3. Keep `.htaccess` (hidden file, starts with a dot). It sets the wasm type, compression and caching.
4. Check: open https://free4allhub.com/botfremaster/VERSION.txt and it shows the build number below. Then open the game and start a new game.

Players get the new build on their next visit: index files are revalidated every load, packs are content-hashed, so no one needs to clear their cache. Saved games stay in the player's browser.
