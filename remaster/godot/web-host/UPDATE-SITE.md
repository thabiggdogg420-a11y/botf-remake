# Updating Free4allhub.com/botfremaster

This folder is the complete new build. To update the live site:

1. Upload the whole contents of this folder to `/botfremaster/` on the site, replacing what is there.
2. Delete any files on the server under `/botfremaster/` that are not in this folder (old packs have hashed names like `fed.1a2b3c4d5e6f.pck`; leaving them wastes space but breaks nothing).
3. Keep `.htaccess` (hidden file, starts with a dot). It sets the wasm type, compression and caching.
4. Check: open https://free4allhub.com/botfremaster/VERSION.txt and it shows the build number below. Then open the game and start a new game.

Players get the new build on their next visit: index files are revalidated every load, packs are content-hashed, so no one needs to clear their cache. Saved games stay in the player's browser.

## New in this build: Desktop | Mobile (25 Nov 2026)
Nothing extra to upload or configure: it is the same folder, the same one upload. The page now shows a small **Desktop | Mobile**
switch (over the loading screen, then a small round tab at the bottom-left), picks Mobile by itself on a phone, and remembers the player's
choice. Mobile always uses the standard (WebGL) build. After uploading, also check:
1. `https://free4allhub.com/botfremaster/?mode=mobile`: the switch shows Mobile highlighted, the game opens with a larger interface.
2. `https://free4allhub.com/botfremaster/?mode=desktop`: the switch shows Desktop highlighted, the game looks as before.
3. Tap the switch, choose the other mode: the page reloads in that mode, and stays there on the next visit (same browser).
Link for a friend on a phone: `https://free4allhub.com/botfremaster/?mode=mobile` (or just the plain link: a phone picks Mobile itself).
See HOSTING.md, "Desktop | Mobile".
