# Asset sources requiring an account (research)

Research date 2026-09-28. This is a companion to `ASSET-SOURCES.md` and
`AUDIO-SOURCES-ONLINE.md`, which both skipped sources that need a login,
sign-up or manual/gated download. This document covers those sources. Same
policy applies: nothing here is downloaded into the repo, no binaries are
committed, and an uploader's stated licence is not a clearance of Star Trek
IP. For a private, never-distributed project the practical risk is low, but
treat every "permission" column here as the uploader's claim, not a fact.

Everything below was found by search or by fetching a page. Several pages
returned HTTP 403 to an unauthenticated fetch (Nexus Mods, some forums) —
those are marked **[login-gated, unverified beyond search summaries]** and
need the user's own account to confirm contents before relying on them.

## Summary

Coverage for the model gaps is thin even with logins in play. Nexus Mods'
Bridge Commander and Star Trek Online sections, ModDB's Armada 3 addons, and
Steam Workshop mods for Stellaris/Homeworld/Sins all plausibly hold Vor'cha,
Keldon, Hideki, D'Kora and Jem'Hadar meshes, but almost every one carries a
restrictive per-model/per-author reuse clause (same pattern already
documented for the base Armada 3/BC Remastered entries) — an account gets
you the *download*, not a clearer *licence*. The most promising new leads are
a dedicated **Keldon** mod on the BC Nexus, a **Cardassian shipset** for
Homeworld's Steam Workshop, and STO's **FSB sound tools** (Nexus + FORGE)
which are the best real route to Cardassian/Ferengi/Dominion/Borg voice and
SFX, since STO is the only major Trek game with all six priority factions
voiced. Freesound.org needs only a free account and has the cleanest terms
of any source in either document, but its Trek content is recreation/SFX,
not canon voice acting. The BBC Sound Effects library turned out not to
need an account at all (worth correcting in the main audio doc). The BOTF
forum (armadafleetcommand.com) requires an account to post but downloads
appear open to guests, which could not be fully confirmed without logging
in.

## Sign-up checklist (priority order)

1. **Nexus Mods** (free) — single account covers Star Trek: Bridge Commander,
   Star Trek Online and Star Trek: Starfleet Command sections. Highest
   value: unlocks the Keldon mod, STO FSB sound-editing tools, and lets
   `ASSET-SOURCES.md`'s existing "restricted" BC Remastered candidates be
   re-checked for per-author permission contacts.
2. **Steam** (free, likely already owned) — needed only to subscribe to
   Workshop items; the files themselves (Stellaris New Horizons, Sins Armada
   3-adjacent items, Homeworld shipsets) are visible without an account but
   downloading/extracting needs a Steam login and the base game installed.
3. **Freesound.org** (free) — cleanest terms of anything researched; needed
   for CC0/CC-BY UI and ambience sounds.
4. **ModDB** (free) — some Armada 3 addon packs and the BC Remastered/
   Kobayashi Maru pages gate download speed or comments behind an account;
   browsing and most downloads work without one, but create an account to
   message modellers for reuse permission.
5. **armadafleetcommand.com forum** (free) — the only source structured
   around BOTF's own voice/graphics taxonomy (Ultimate Universe, Galaxies
   MOD, Vanilla Upgrade). Register to read/download in the modding
   sub-forums and to message mod authors about reuse.
6. **Internet Archive** (free, optional) — most Trek items are already
   open; an account only helps for the handful of "lending library" items,
   which are irrelevant here (borrowed books/video, not assets).
7. **Discord** (free) — join Trekmeshes-adjacent and Star Trek Online modding
   Discords (found via Nexus/ModDB mod pages' own Discord links) once the
   above are exhausted; useful for asking authors directly about the D'Kora
   and Vor'cha gaps.
8. **Patreon** (paid, optional, lowest priority) — "3D Model Archive Server"
   -style Patreons exist but were not confirmed to hold Star Trek-specific
   content; only worth it if a specific creator is found making Trek meshes.

## Ranked table — models

| # | Source | Account | Contains | Format/quality | Terms as stated | Fills which gap | Steps |
|---|---|---|---|---|---|---|---|
| 1 | [Keldon (v1.0)](https://www.nexusmods.com/startrekbridgecommmander/mods/2237) — Bridge Commander Nexus | Nexus Mods, free | A single Cardassian Keldon-class ship mod for Bridge Commander | NIF (BC's native format); quality/polycount not verified without login | **[login-gated, unverified]** — likely the same "per-ship readme, author permission required" pattern as the rest of BC Remastered | **Cardassian Keldon** — currently the project's only unresolved model gap besides Vor'cha, D'Kora and starbases | Create Nexus account → download page → open readme for per-author terms → NIF needs a BC/NifTools pipeline to get to glTF/FBX before Unreal import |
| 2 | [Star Trek: The Generation Before (BETA)](https://www.nexusmods.com/startrekbridgecommmander/mods/4046) — BC Nexus | Nexus Mods, free | Cardassian Hideki, Obsidian and Keldon ship models (per search summary) | NIF; unverified | **[login-gated, unverified]** | **Cardassian Keldon and Hideki** | Same as above |
| 3 | [Aftermath MegaMod Ship Pack (Remastered)](https://www.gamefront.com/games/bridge-commander/file/aftermath-megamod-ship-pack-remastered-version) — GameFront | None (GameFront is direct-download) | Cardassian Galor, Keldon, Barkus, Kimal, Hideki | NIF; unverified polycount | Not fetched in this pass; GameFront pages for BC mods are generally "non-commercial, credit the original authors" **[unverified]** | **Cardassian Keldon and Hideki** | Direct download from GameFront, no login; still needs NIF conversion tooling |
| 4 | [Star Trek: Armada 3 addons](https://www.moddb.com/mods/star-trek-armada-3/addons) — ModDB | None to browse; account to comment/message authors | Addon packs on top of the base mod; likely includes additional ship variants beyond the base roster (Vor'cha, Keldon, Hideki, D'Kora, Jem'Hadar already exist in the base mod per `ASSET-SOURCES.md`) | Mesh format used by Sins of a Solar Empire (`.mesh`); high-detail, hand-modelled | Same base-mod restriction already logged: "Aad Moerman models need special permission (not granted); STAR remaster models may not be reused" | Reference/QA only — same restricted status as the already-logged Armada 3 base entries | ModDB account (free) to message credited modellers directly for the Vor'cha/Keldon/D'Kora/Jem'Hadar permission the base mod denies by default |
| 5 | [Stellaris: Star Trek New Horizons](https://steamcommunity.com/sharedfiles/filedetails/?id=688086068) — Steam Workshop | Steam (free), owns Stellaris | "Full ship sets for 70+ races" including UFP, Klingon, Romulan, Cardassian, Dominion, Borg; described by players as highly detailed | Stellaris entity/mesh format (not directly Unreal-importable; needs extraction via Stellaris' own tools or community extractors) | Steam Workshop's standard terms (mod content, no explicit redistribution grant); mod is a total-conversion fan project, no stated commercial/derivative licence | **Klingon Vor'cha-adjacent designs, Cardassian, Dominion (Jem'Hadar), Borg** — broadest single faction-complete source found | Steam login → subscribe in Workshop → Stellaris installed to fetch the files locally → locate `.mesh`/`.dds` under the Workshop content folder → convert |
| 6 | [Cheek's Custom Shipsets: Star Trek Cardassians](https://steamcommunity.com/sharedfiles/filedetails/?id=2331924381) — Steam Workshop (Stellaris shipset) | Steam, free | "A single model for each class of ship, titans and juggernauts, habitats, starbases and defence platforms" for the Cardassian Union | Stellaris shipset format | Not fetched in this pass; Steam Workshop default terms **[unverified]** | **Cardassian Keldon/Hideki-class stand-ins and Cardassian starbase** — direct hit on two named gaps | Steam login → subscribe → extract from local Workshop cache |
| 7 | [Star Trek Cardassian Keldon Class](https://steamcommunity.com/sharedfiles/filedetails/?id=251202273) — Steam Workshop | Steam, free | A dedicated Keldon-class model (game/engine not confirmed past the title — 429 on fetch, needs re-check while logged in) | Unverified | **[unverified — page did not load in this pass]** | **Cardassian Keldon** | Steam login → subscribe → confirm target game and format once loaded |
| 8 | Homeworld Remastered Workshop — [Star Trek: Continuum](https://steamcommunity.com/sharedfiles/filedetails/?id=403358147), [Star Trek: Chronicles - 2380](https://steamcommunity.com/sharedfiles/filedetails/?id=759628723) | Steam, free, owns Homeworld Remastered | Federation/Romulan/Klingon (Continuum) and multi-era ship sets (Chronicles), high detail (Homeworld ships are hero-asset quality) | Homeworld's own mesh format; needs the community's Homeworld modding tools to extract to a neutral format | Not fetched in this pass **[unverified]** | Possible **Klingon Vor'cha** and **Romulan** supplemental references | Steam login → subscribe → Homeworld Remastered installed → extract with HW modding tools |
| 9 | [Star Trek Online mods](https://www.nexusmods.com/games/startrekonline) — Nexus Mods (e.g. "Refreshed Ships" #174, "Galaxy Class - Definitive Edition" #186) | Nexus Mods, free | Re-textured/remastered ship *materials*, not new meshes, for STO's existing ship roster (which does include Vor'cha, Keldon-era Cardassian and Jem'Hadar ships in the base game) | STO's native format; texture-only mods, so the underlying STO ship geometry itself (via extraction, see audio table below) is the real prize | **[login-gated, unverified in detail]**; Nexus's general non-commercial modding norm applies | Texture reference only; the real gap-filler is extracting STO's base-game ship geometry, not these mods | Nexus account → STO installed (free-to-play) → Gibbed's Cryptic Tools/community extractors to pull `.geo`/model files from HOGG archives, separate from these two texture mods |
| 10 | [Trekmeshes: Vor'cha battlecruiser, Romulan Warbird (multiple), Ferengi Marauder](https://trekmeshes.eu/download/all) | Free account required to download most entries (the site is closing per its own notice; already logged in `ASSET-SOURCES.md` as a candidate but its individual per-ship pages were not resolved for Vor'cha) | David 'Knightrider' Metlesits and other named modellers' Vor'cha, Romulan Bird-of-Prey/Warbird, and Ferengi Marauder meshes | 3ds/max/obj, no polycount confirmed | No licence stated on individual pages (as already logged); site itself asks for credit | **Klingon Vor'cha, Ferengi D'Kora** — the two hardest gaps | Create a trekmeshes.eu account (site is winding down — download soon) → resolve the catalogue-index entry to the specific Vor'cha page → download |
| 11 | [Scifi-Meshes.com forums](https://forums.scifi-meshes.com/discussions/tagged/startrek) | Free forum account to download attachments in most threads | Community "Star Trek Models" thread (4+ pages) with member-made and member-collected meshes, including requests/offers threads ("Looking for...") that could turn up a free Vor'cha or D'Kora | Mixed formats, mixed quality, per-poster | Per-poster; forum's general norm is non-commercial sharing with credit **[unverified — thread contents not fetched]** | Best shot at closing **Klingon Vor'cha and Ferengi D'Kora** since Sketchfab and Trekmeshes both lack a clean download for either | Register free account → read the "Star Trek Models" and "Looking for..." threads → download individual attachments |
| 12 | [Sketchfab downloadable-only search](https://sketchfab.com/search?features=downloadable&type=models) with login | Sketchfab, free (Pro needed only for bulk/API download of paid items) | Filters to models the uploader marked downloadable; a login-gated re-search may surface Vor'cha/Keldon/D'Kora/Jem'Hadar uploads that don't appear in anonymous search (Sketchfab hides some downloadable-flagged results from logged-out users) | gltf/glb typically, uploader-declared CC terms as already catalogued for other classes | Same as existing Sketchfab entries: uploader-declared, unclear | Re-run of the search already done for other classes, specifically for the four hardest gaps | Log into Sketchfab → repeat searches for "Vor'cha", "Keldon", "Hideki", "D'Kora", "Jem'Hadar attack ship", "Klingon starbase", "Romulan starbase" with the downloadable filter |
| 13 | CGTrader / TurboSquid free-tier Trek search ([CGTrader Klingon](https://www.cgtrader.com/3d-models/klingon), [TurboSquid free Star Trek](https://www.turbosquid.com/3d-model/free/star-trek)) | Free account to download free-tier items; paid for premium | Klingon (129 models on CGTrader alone), general "Star Trek" tag (27,050 CGTrader / 300+ TurboSquid); a mix of fan and commercial listings, mostly unofficial designs or generic "Klingon-style" ships rather than named canon classes | MAX/OBJ/FBX/3DS/C4D; polycount varies widely, page-by-page | Per-listing; free-tier CGTrader models are typically "Royalty Free" for the uploader's own (non-Trek) IP claim only — **the Star Trek design itself is never the uploader's to license** | Possible generic Klingon/Cardassian dressing, unlikely to resolve the *named* Vor'cha/Keldon/D'Kora gaps specifically | Free account on either site → filter to free/downloadable → check each listing individually, most are not canon-accurate |

## Ranked table — audio

| # | Source | Account | Contains | Format/quality | Terms as stated | Fills which gap | Steps |
|---|---|---|---|---|---|---|---|
| 1 | [STO FSB Object Replacement and General Editor (FORGE)](https://www.nexusmods.com/startrekonline/mods/200) + [All SFX Modding Tools for STO](https://www.nexusmods.com/startrekonline/mods/78) — Nexus Mods | Nexus Mods, free | Tools to open STO's `.fsb` sound banks and extract every in-game sample as WAV/MP3 — the search summary states pre-extracted files exist for everything as of Feb 2025, so an account may skip needing STO installed at all | FSB4/FSB5 → WAV/MP3 extraction, game-original quality | **[login-gated, unverified in full]**; Nexus general non-commercial modding norm; STO's own audio is Cryptic/Paramount copyrighted, unofficial extraction | **All six priority factions** — STO is the only large Trek property with full Cardassian, Ferengi, Klingon, Romulan, Dominion (Jem'Hadar/Founders) and Borg voice acting, including combat acknowledgements and hails, since it is a live 25th-century MMO spanning every major race | Nexus account → download FORGE and/or the pre-extracted sample set → sort by faction using in-game context or community-maintained line lists |
| 2 | [Star Trek: Armada 3 addons](https://www.moddb.com/mods/star-trek-armada-3/addons) — ModDB | None to browse | The base mod (already logged) has authentic voice lines for hero units (Picard, Martok, Sela); addons may include more, per faction, including combat barks | Game audio format for Sins of a Solar Empire: Rebellion (`.wav`/`.snd`), unverified | Same restricted stance as the base mod: no reuse grant, per-modeller/voice credit only | **Klingon, Romulan hero/combat lines** confirmed to exist; Cardassian/Dominion/Borg not confirmed in this pass | ModDB account to message the Stellar Parallax team about reuse; otherwise reference/QA only |
| 3 | [Star Trek: New Horizons](https://steamcommunity.com/sharedfiles/filedetails/?id=688086068) — Stellaris Steam Workshop | Steam, free | Full-conversion mod voice/event audio across 70+ races, spanning the whole timeline — the mod's scope strongly implies per-faction greeting/diplomacy lines exist (Stellaris' event system uses per-species flavour audio) | Stellaris audio asset format, unverified quality | Steam Workshop default; fan mod, no stated redistribution licence | Potentially **Cardassian, Ferengi, Dominion, Borg diplomacy/greeting lines** — the single biggest named gap in `AUDIO-SOURCES-ONLINE.md` | Steam login → subscribe → Stellaris installed → locate mod's audio folder locally |
| 4 | armadafleetcommand.com — [BOTF modding forum](https://www.armadafleetcommand.com/onscreen/botf/viewforum.php?f=11), Ultimate Mod 5 thread (already logged in the audio doc) | Free forum account (posting/some downloads); guest download status **[unverified — not tested logged out in this pass]** | Already logged: "hundreds of new voice samples for Dominion gameplay events." An account may unlock more threads on `converse.bin`/`sfx.snd`/`English.snd` editing across the other five factions, since those are BOTF's own file formats | BOTF's native `.snd`/`.bin` audio, already game-ready for a BOTF-derived pipeline | Stated "public domain for non-commercial use... do not redistribute without permission from listed contributors" | **Dominion voice (already confirmed)**; possibly all six factions once inside the modding sub-forum, since BOTF ships with Cardassian/Ferengi/Klingon/Romulan/Borg lines in its own `converse.bin` that these threads document how to extract/edit | Register free account → read modding sub-forum → message Thunderchero/Dr Breen/Gowron (named maintainers) for reuse terms |
| 5 | [Freesound.org](https://freesound.org) | Free account (browsing/preview works logged out; download needs login) | Community-recorded/synthesised sci-fi UI beeps, alien-voice-style recreations, communicator chirps; CC0/CC-BY per-sound | WAV/MP3/FLAC, quality varies by uploader | Cleanest terms found in either document: explicit CC0/CC-BY per sound, redistribution/commercial use allowed without further permission | UI beeps and ambience only — **does not fill the character-voice or diplomacy-greeting gap** | Create free account → search per-faction ("Klingon", "alien radio voice", "sci-fi computer") → check each clip's licence badge before download |
| 6 | Nexus Mods — Bridge Commander sound mods (general [BC Nexus](https://www.nexusmods.com/games/startrekbridgecommmander)) | Nexus Mods, free | New weapon/ship SFX (already logged as GameFront-reachable for BC Remastered); Nexus may host additional voice-adjacent mods (bridge chatter, hails) not on GameFront | .wav, game-original BC quality | **[login-gated, unverified]** | Possible **combat acknowledgement SFX** across factions BC covers (Federation, Klingon, Romulan, Cardassian) | Nexus account → browse BC Nexus "Sounds" category specifically (not yet filtered in this pass) |
| 7 | Internet Archive — items requiring a free account (lending-library items only) | archive.org free account | Checked: the Trek audio items already logged (`startrekaudio`, TNG AudioClips, TOS SFX, SSE Library) are all open-access, no login needed, contradicting the assumption that archive.org gates them | n/a | archive.org standard hosting terms | None — **this line is a negative result**: no login-gated archive.org item with unique voice/SFX content was found in this pass | No action needed; the existing audio doc's archive.org sources are already fully accessible |
| 8 | BBC Sound Effects Archive (`sound-effects.bbcrewind.co.uk`) | **No account needed** — corrects the assumption in the task; it is free, unrestricted download of ~33,000 WAV/MP3 files | General SFX (machines, atmospheres, sci-fi-adjacent), not Star Trek-specific and not voice | WAV/MP3, broadcast-archive quality | Free for personal/research/non-commercial use; commercial use needs a separate BBC licence; explicitly excludes AI training/data-mining | Generic ambience/mechanical SFX only, not a faction-voice source | Browse and download directly, no sign-up; do not bulk-scrape for AI training per stated terms |
| 9 | Discord — Star Trek Online modding community, Stellar Parallax (Armada 3) Discord (linked from the mods' own Nexus/ModDB pages, not independently found) | Discord, free | Community members who may share extracted STO voice sets or Armada 3 audio directly, bypassing the tooling steps above | Varies | Server-specific rules, generally non-commercial fan-sharing | Fastest human-mediated route to sorted-by-faction voice lines for all six priority factions | Join via the invite link on the Nexus/ModDB mod pages (not verified in this pass — follow the link from mod page #1 and #2 above) |

## Gap-coverage matrix

| Gap (from ASSET-SOURCES.md / AUDIO-SOURCES-ONLINE.md) | Best login-gated lead | Confidence |
|---|---|---|
| Klingon Vor'cha model | Trekmeshes account (resolve the Vor'cha catalogue entry) or Scifi-Meshes forum thread | Low-medium — neither independently confirmed a working Vor'cha download in this pass |
| Cardassian Keldon model | BC Nexus "Keldon (v1.0)" mod (#2237), Homeworld Steam Workshop "Cheek's Custom Shipsets: Star Trek Cardassians" | Medium — two independent dedicated sources found, both need login to confirm terms |
| Cardassian Hideki model | "Star Trek: The Generation Before (BETA)" BC Nexus mod, Aftermath MegaMod Ship Pack (GameFront, no login) | Medium |
| Ferengi D'Kora model | Trekmeshes (existing "unclear" entry, needs a re-check with an account) or Scifi-Meshes forum | Low |
| Jem'Hadar ships (attack/battlecruiser/battleship) | Stellaris: Star Trek New Horizons (Dominion shipset), STO's base-game geometry (via extraction tools) | Medium — both plausible, neither individually confirmed |
| Klingon starbase | None found with high confidence; Stellaris New Horizons' Klingon station art is the best lead | Low |
| Romulan starbase | None found with high confidence; Stellaris New Horizons' Romulan station art is the best lead | Low |
| Minor-race ships (Breen, Tholian, Gorn, Orion, Hirogen, Kazon, etc.) | Stellaris New Horizons' "70+ races" claim is the only lead covering this breadth | Low — not verified per-race |
| Cardassian/Ferengi faction voice | STO FSB extraction (both factions appear in STO) | Medium-high — STO is confirmed to include both factions as playable/NPC content |
| Klingon/Romulan faction voice | STO FSB extraction; Armada 3 hero-unit lines (Martok, Sela) already confirmed | High |
| Dominion faction voice | BOTF Ultimate Mod 5 (already confirmed "hundreds of new voice samples"); STO FSB extraction (Jem'Hadar/Founders are STO content) | High |
| Borg faction voice | STO FSB extraction (Borg is core STO content); Stellaris New Horizons | Medium |
| Combat acknowledgements (all factions) | STO FSB extraction is the only source structured around per-faction ship combat barks | Medium — STO's combat VO is extensive but not pre-sorted by line-type |
| Diplomacy greeting lines (all factions) | Stellaris New Horizons' per-species event/diplomacy audio is the single most plausible lead found anywhere in either document | Low-medium — mod's scope is confirmed, its diplomacy-audio content specifically is not |

## Notes on verification limits

- Nexus Mods pages returned HTTP 403 to unauthenticated fetches throughout
  this pass; every Nexus entry above is a search-summary claim, not a
  confirmed page read. The user's own account is needed to check contents,
  file formats and exact stated terms before download.
- The Steam Workshop "Star Trek Cardassian Keldon Class" item (#251202273)
  returned HTTP 429 on the one fetch attempt in this pass and was not
  re-verified; its target game was not confirmed.
- Scifi-Meshes.com forum thread contents (the "Star Trek Models" and
  "Looking for..." threads) were found but not read — they need a forum
  account to open attachments.
- No Patreon or Discord community specifically dedicated to Star Trek 3D
  assets was confirmed by name in this pass; the leads listed are indirect
  (linked from other mod pages) and unverified.
