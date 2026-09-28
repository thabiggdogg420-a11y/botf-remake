# Audio Sources — Online Repositories (Research)

Research pass on publicly reachable online sources of Star Trek-style audio for the
private BOTF remake, as an alternative to extracting audio from a personally-owned
BOTF install. This is a survey document only — no audio has been downloaded into the
repo. All URLs below were actually found via search or fetched directly; anything not
independently confirmed is marked **[unverified]**.

Even where a source is "public domain" or CC-licensed for the *effect itself*, most
Star Trek voice/SFX assets on these sites are original copyrighted recordings
(Paramount/CBS or game publishers). "Terms as stated" below records what each site
says; it is not a legal clearance. Since this project is private and never
distributed, the practical risk is low, but nothing here should be treated as cleared
for redistribution.

## Summary

No single source covers everything needed. The best practical mix is: **TrekCore's
audio library** for a broad, easy, no-login MP3 grab of canon computer/alien/character
voice lines and ship SFX; **archive.org's Sound Source Interactive "AudioClips" CD-ROM
dumps** (TOS and TNG) for large, cleanly organized, freely downloadable ZIP/ISO
packages including real computer-voice and character-quote libraries; and the
**Armada Fleet Command / BOTF modding community** for BOTF-native voice pack mods
(e.g. Ultimate Mod 5's "hundreds of new voice samples for Dominion gameplay events")
that already match the game's `converse.bin`/`.snd` structure and faction rosters —
though most of that community content is stated "non-commercial, do not redistribute
without permission," which fits a private, non-distributed use but not resale.
GitHub LCARS sound repos (e.g. `wofsonofmoag/lcars-sounds`) are small, easy `git
clone` UI-beep grabs but carry no license and are unsourced fan rips. Freesound.org
is the only source with genuinely clear CC0/CC-BY terms, but its Trek content is
recreations/soundalikes, not canon lines — good for LCARS-style UI beeps, not for
character voice acting. Soundboards (101soundboards, myinstants) are browsable but
not built for bulk/direct download — each clip is a one-off manual save, often behind
an embedded player, making them a last-resort per-line source. For genuinely new
lines (advisor barks, diplomacy greetings not in canon), the AI voice-model route
(fan-trained Majel Barrett "computer voice" RVC/TTS models on fakeyou.com,
voice-models.com, 101soundboards TTS, vocalize.fm) is the most promising path, but
those are third-party-trained voice clones of a real actor's performance, not
licensed synthesis, so treat as a for-personal-prototyping tool only. Coverage by
faction is very uneven: Federation/computer-voice material is abundant; Klingon and
Romulan get meaningful soundboard/GitHub coverage; Cardassian, Ferengi, Dominion and
Borg are thin outside of BOTF's own mod community and STO rips; the Dominion in
particular is really only covered by the BOTF Ultimate Mod's own new voice work.

## Ranked Source Table

| # | Source | URL | Direct-download ease | Voice-line coverage | Terms as stated |
|---|--------|-----|----------------------|----------------------|------------------|
| 1 | TrekCore Sound Archive | https://www.trekcore.com/audio/ | High — no login; click to play, right-click "Save As" per clip; MP3 | Strong: computer voice ("Unable to comply," "Regeneration cycle complete"), character quotes (Kirk/McCoy/Spock/Scotty), Klingon language samples, ~20+ categories | No copyright/terms notice found on the page itself; site is fan-run, content is unofficial rips of Paramount/CBS audio |
| 2 | archive.org — "Star Trek AudioClips: The Talking Utility" (TOS) | https://archive.org/details/startrekaudio | High — direct ZIP/ISO/individual-file/torrent downloads, no login | Original-series dialogue + SFX bundled as a Windows 3.1 "event sound" utility; character voice clips included | Standard archive.org hosting page; no explicit product license shown; originally commercial Sound Source Interactive software (1995), now archived |
| 3 | archive.org — "Star Trek: The Next Generation AudioClips [SWWNCLST05]" | https://archive.org/details/star-trek-the-next-generation-audioclips-swwnclst05 | High — direct download | Computer voice, bridge SFX (doors, transporter, bridge buttons), Borg and other alien effects | Same as above (Sound Source Interactive product, archived, no explicit redistribution license) |
| 4 | archive.org — "STAR TREK: SOUND EFFECTS – OST 1988" | https://archive.org/details/star-trek-tos-sound-effects | High — direct download, ~111MB, has a directory listing at https://archive.org/download/star-trek-tos-sound-effects | Primarily SFX (TOS), voice content limited/unverified **[unverified extent of voice lines]** | archive.org hosting page terms only |
| 5 | archive.org — "SSE Library: SCIFI" | https://archive.org/details/SSE_Library_SCIFI | High — direct download | Sci-fi/Trek-adjacent SFX (alarm horn, door swish, scanner, transporter, tricorder); not primarily voice | archive.org hosting page terms only |
| 6 | Armada Fleet Command — BOTF Modding Archive & Ultimate Mod 5 | https://www.armadafleetcommand.com/onscreen/botf/viewforum.php?f=21 and https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=2692 | Medium — forum download links, some via bundled "BOTF All-in-One" installer; registration likely needed to post but downloads appear open to guests **[unverified re: guest download]** | BOTF-native: Ultimate Mod 5 states "hundreds of new voice samples for Dominion gameplay events," plus dedicated sub-forums on editing `converse.bin`, `alt.res`, `sfx.snd`, `English/German.snd` | Stated "released into the public domain for free, for purposes of non-commercial usage"; explicit request not to redistribute without permission from listed contributors (Siggi, Thunderchero, Dr Breen, Jigglypuff, DCER, Gowron) |
| 7 | GitHub — `wofsonofmoag/lcars-sounds` | https://github.com/wofsonofmoag/lcars-sounds | High — `git clone` or ZIP download, no login | UI/LCARS beeps only (TOS/TNG/ENT/DS9 chirps, alerts, transporters, door chimes, affirm/deny tones) — ~40 MP3s, no character/advisor voice lines | No license file; README disclaimer: "I do not have anything to do with any of these. Use at your own risk." |
| 8 | GitHub — `JawsomeJason/claude-lcars` | https://github.com/JawsomeJason/claude-lcars | High — clone/download | Console/UI beep set built for a CLI tool, not a Trek voice-line archive **[unverified content list — found via search, not fetched]** | Unverified license |
| 9 | Freesound.org (Trek-style search) | https://freesound.org (e.g. https://freesound.org/people/audioorange123/sounds/218740/, https://freesound.org/s/125710/) | High — direct download, requires free account for download (browsing/preview doesn't) | None for character/advisor voice; useful only for computer-beep/communicator-chirp *recreations* | Explicit CC licenses per-sound (the two checked are CC0-style: reuse/modify/commercial use without permission) — cleanest terms of any source found |
| 10 | 101soundboards.com | https://www.101soundboards.com/search/star%20trek%20klingon (and e.g. https://www.101soundboards.com/sounds/417130-romulan-transporter) | Low-medium — playable per clip; per-sound download typically requires using their download button/manual save, no bulk export | Klingon and Romulan sections found (disruptors, transporters, computer beeps, alarms); also hosts AI "TTS" voice models (see below) | Site terms not fetched in depth **[unverified]**; general soundboard-site norm is personal/non-commercial use |
| 11 | Myinstants.com | https://www.myinstants.com/en/search/?name=star+trek | Low — one-off manual click-and-save per sound, no bulk download, entries are user-submitted and inconsistently tagged | Mostly SFX and meme clips (communicator chirp, transporter, alert klaxon); little structured voice-line coverage | Not fetched in depth **[unverified]**; user-submitted content, informal |
| 12 | Star Trek Online — Nexus Mods "All SFX Modding Tools for STO" | https://www.nexusmods.com/startrekonline/mods/78 | Login required (page returned 403 to unauthenticated fetch) — Nexus Mods requires a free account to browse/download most files | Community reports extracted FSB audio files (via Gibbed's Cryptic Tools) covering STO's in-game voice/SFX, including Majel Barrett-voiced computer lines used in "complete audio conversion" mods for other games | Not independently confirmed **[unverified — page inaccessible without login]**; Nexus Mods' general site ToS applies (non-commercial modding use) |
| 13 | Bridge Commander Remastered | https://www.nexusmods.com/startrekbridgecommmander/mods/4012 , https://www.moddb.com/games/star-trek-bridge-commander/downloads/bridge-commander-remastered , https://www.gamefront.com/games/bridge-commander/file/bridge-commander-remastered | GameFront link is direct-download, no login, no speed cap (per site); Nexus/ModDB may require account | New sound and weapon effects (ship SFX), not primarily character/advisor voice content **[unverified re: any new voice lines]** | Mod-specific terms not fetched **[unverified]**; general BC modding community norm is free, non-commercial |
| 14 | archive.org — Star Trek: Armada / Armada II (game preservation) | https://archive.org/details/armada_202412 , https://archive.org/details/armada-2 | High to access the page, but content is a full BIN/CUE disc image, not extracted individual sound files — needs extraction tooling | Full game audio (including all in-game voice acknowledgements/selections) is present in the disc image but not separated out | archive.org hosting page terms; underlying software is commercial (Activision/Mad Doc), preserved for archival access |
| 15 | AI voice models — Majel Barrett "computer voice" | https://fakeyou.com/weight/weight_46w2q9q1rk75rw4x6a25ak42n/majel-barrett , https://voice-models.com/model/1qQohBdPu8g , https://www.vocalize.fm/voices/452 , TTS entry on 101soundboards | High — text-in, audio-out web tools, no game-audio extraction needed | Generates **new** lines in a Majel-Barrett-like computer voice; does not cover captains/advisors/other factions | Fan-trained voice-clone models of a real deceased actor's copyrighted performance; sites' own ToS generally restrict to personal/non-commercial fan use — treat as prototyping-only, not a cleared source |
| 16 | Pixabay / Soundsnap / WavSource / MediaCollege — misc Trek SFX libraries | https://pixabay.com/sound-effects/search/star%20trek/ , https://www.soundsnap.com/tags/star_trek , https://www.wavsource.com/tv/star-trek/star-trek-sfx.htm , https://www.mediacollege.com/downloads/sound-effects/star-trek/tos/ | High (Pixabay, WavSource, MediaCollege: direct, no login); Soundsnap requires account/subscription for most items | Mostly SFX (transporter, phaser, alert), minimal voice-line content | Pixabay states royalty-free, no attribution required for its own uploads (note: Trek-named search hits on Pixabay are often generic sci-fi SFX, not real Trek audio — verify per-clip); WavSource/MediaCollege terms not fetched in depth **[unverified]** |

## Per-Faction Voice Coverage

- **Federation (computer voice, Starfleet captains/advisors)** — Best covered.
  TrekCore audio library and the archive.org TOS/TNG "AudioClips" packages both
  center on Federation computer-voice lines and character quotes (Kirk, McCoy, Spock,
  Scotty; TNG bridge/computer lines). GitHub LCARS repos cover UI beeps well. AI
  Majel Barrett voice models (fakeyou.com, voice-models.com, vocalize.fm) can
  generate new Federation-computer-style lines on demand.
- **Klingon** — Moderate. 101soundboards has a dedicated Klingon results page
  (disruptors, torpedoes, alarms, cloak/decloak, doors, transporters — mostly SFX,
  some vocalizations); TrekCore lists "Klingon language samples." No dedicated
  advisor/diplomacy-line archive found.
- **Romulan** — Moderate, SFX-heavy. 101soundboards has specific Romulan entries
  (transporter, computer beep 1) confirmed via fetch/search. No canon Romulan
  advisor dialogue archive found.
- **Cardassian** — Thin. No dedicated archive or soundboard section found in this
  pass; likely present only inside full-game rips (STO Nexus SFX tools, Armada
  disc images) rather than as a standalone library. **[gap]**
- **Ferengi** — Thin. Same situation as Cardassian — no dedicated collection found;
  would need extraction from STO or Armada assets, or BOTF's own mod audio.
  **[gap]**
- **Dominion** — Best BOTF-specific source is the community mod itself: Ultimate
  Mod 5 for BOTF explicitly adds "hundreds of new voice samples for Dominion
  gameplay events" (armadafleetcommand.com), since vanilla BOTF/TNG-era canon has
  very little Dominion computer/advisor material to rip in the first place.
- **Borg** — Covered indirectly. archive.org's TNG AudioClips package is
  advertised as including "Borg and other unusual effects." No structured
  Borg-voice-line archive beyond that found in this pass.

## Recommended Acquisition Order

1. **TrekCore audio library** — fastest win: broad canon computer-voice, character
   quote, and alien-SFX coverage, no login, simple right-click saves. Start here for
   UI beeps and iconic one-liners.
2. **archive.org Sound Source Interactive packages** (TOS `startrekaudio`, TNG
   `star-trek-the-next-generation-audioclips-swwnclst05`) — bulk ZIP/ISO grabs give
   a large, pre-organized base library of computer voice + character clips in one
   download each.
3. **Armada Fleet Command / BOTF modding community** — go here specifically for
   BOTF-native voice content (Ultimate Mod 5's Dominion voice samples, and the
   `converse.bin`/`sfx.snd`/`English.snd` editing threads) since this is the only
   source structured around BOTF's actual event/line taxonomy per faction.
4. **GitHub LCARS repos** (`wofsonofmoag/lcars-sounds`, `JawsomeJason/claude-lcars`)
   — quick supplemental UI-beep/chirp set via `git clone`.
5. **Freesound.org** — fill any remaining UI-beep gaps with clips that have
   unambiguous CC0/CC-BY terms, for anything that might ever need clean
   provenance.
6. **101soundboards / myinstants** — last resort, manual per-clip fills for
   Klingon/Romulan lines not found elsewhere; expect one-by-one saving, not bulk
   download.
7. **AI Majel-voice tools (fakeyou.com, voice-models.com, vocalize.fm)** — for
   genuinely new advisor/diplomacy/battle-chatter lines that don't exist in canon
   source material, once the above libraries are exhausted.
8. **STO Nexus Mods SFX tools / Bridge Commander Remastered / Armada disc images**
   — heavier lift (needs login and/or extraction tooling: Gibbed's Cryptic Tools
   for STO FSB files, disc-image mounting for Armada), pursue only if factions like
   Cardassian/Ferengi are still short after steps 1–7.

## Gaps

- No source found gives per-faction **diplomacy greeting** lines (e.g. a Ferengi or
  Cardassian leader's greeting/insult/proposal lines) as a discrete, labeled set —
  this is the single biggest hole for 4X-style leader-greeting audio.
- **Cardassian and Ferengi voice/SFX** have no dedicated public archive found in
  this pass; likely requires extracting from a full game's assets (STO, Elite Force,
  Armada) rather than a ready-made pack.
- **Combat acknowledgement barks** ("Shields at 50%," "Torpedoes away," "We are
  losing power") were not found as a structured, downloadable set anywhere except
  possibly inside unopened full-game archives (Armada BIN/CUE images, STO FSB
  extracts) — not verified line-by-line in this pass.
- STO Nexus Mods page could not be fetched without a login (403), so its actual
  content list is **unverified** beyond what community search summaries claim.
- Several soundboard/SFX-library sites (Soundsnap, WavSource, MediaCollege,
  Pixabay Trek results) were listed from search summaries only, without a full
  page fetch to confirm exact terms — flagged `[unverified]` above.
- No genuinely free, clearly-licensed source of new **character-quality voice
  acting** (as opposed to UI beeps or AI mimicry of Majel Barrett) was found;
  closing this gap likely means either the BOTF mod community's own recorded
  lines (non-commercial-use terms) or commissioning/recording new lines.
