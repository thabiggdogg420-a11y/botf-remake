# Voice line plan

Sources that outrank this document: Simon's answers of 30 September 2026 (in the session), and `AUDIO-PLAN.md`
(cue keys, private-local rule).

Simon's answers, 30 Sep 2026:
- Find the original BOTF files online, download whatever is needed.
- Mine the Trek games he owns, and the complete series collection on `D:\TV Shows` / `D:\Movies`.
- Whisper and any other tools are approved.
- Text-to-speech is fine if needed, but found audio is preferred.
- Gap lines use original synthetic voices, not beeps.

**Everything here stays private and local.** Clips live in the gitignored `game/LocalAssets/` and `/Game/Local/Audio`.
Only tools, key lists and this plan are committed; no audio, transcripts or subtitle text.

## 1. Sources, in order of fit

| # | Source | What it gives | State |
|---|---|---|---|
| A | Original BOTF voice bank (`english.snd`, archive.org `birthof-fed`), plus end-of-game speeches and intro from the disc (`botf_20240331`) | The game's own advisor, diplomacy and combat-acknowledgement lines for all five majors, plus 57 min of the original score and 163 effects | Extracted with `Tools/Extract-BotfAudio.py` (music-table support added): 1364 voice, 151 music segments, 163 sfx. Transcribing with `Tools/transcribe_voice.py` |
| B | Bridge Commander (installed on `H:\Games`) | Federation bridge crew: Tactical 253, Helm 157, Engineering 222, Science 153, XO 113, with named files (`AttackFore`, `EvadingTorps`…). Klingon officer Korbus: 238 | Map by file name, confirm by transcript |
| C | Series and films on `D:\` | Every faction in its own voice: Romulan, Cardassian, Ferengi and Klingon commanders; hails, threats, surrender, greetings | DS9, VOY and TOS have text subtitles, so lines are found instantly. TNG and ENT need Whisper. 5.1 tracks give the centre channel (dialogue) cleanly |
| D | Mod voice packs (BOTF Ultimate Mod 5, Armada / Bridge Commander packs) | Extra faction lines | Needs the PC-QUEUE item 0 sign-ups |
| E | Local text-to-speech with original voices | Whatever A–D do not cover: new event and victory lines, the ship computer | Last resort. No cloning of real actors' voices |

## 2. Keys: more specific cues

Today one key serves many events (`vo.advisor.<fac>` for research, construction, colonies, attacks…), so a random
clip could say the wrong thing. The cue table (`BotfAudioCues.h`) gains event-specific keys that fall back to the
old ones, so partial coverage never goes silent:

- `vo.advisor.<event>.<fac>`
  - Events: `turn`, `research`, `construction`, `colony`, `outpost`, `attack`, `fleet`, `contact`, `unrest`, `event`.
  - Falls back to `vo.advisor.<fac>`.
- `vo.diplomacy.<act>.<speaker>`
  - Acts: `greet`, `propose`, `accept`, `refuse`, `war`, `peace`, `alliance`.
  - Falls back to `vo.diplomacy.<speaker>`.
- `vo.combat.<order>.<fac>`: one per manoeuvre, plus `hail`, `ram` and `cloak`.
- Barks, already specific: `shields.75|50|25`, `torpedo`, `kill`. Additions: `hull.critical`, `encounter`, `retreat`,
  `won`, `lost`.
- `vo.victory.<type>.<fac>`: `scientific`, `diplomatic`, `conquest`, `domination`, `defeat`.

One speaker per faction role (the Federation advisor is always the same voice): the catalogue records a voice id per
clip, and the resolver prefers the faction's chosen voice.

Estimated inventory: about 55 lines per major empire (275), about 3 per minor race with a shared alien fallback (90),
and about 30 for the ship computer and narrator: about 400 lines.

## 3. Pipeline

1. **Inventory.** `VOICE-LINES.csv` (committed): key, faction, role, intended wording, and alternates.
2. **Candidates.**
   - The BOTF and Bridge Commander transcripts, plus subtitle text from the episodes, are searched for each line's
     wording and its paraphrases ("shields at fifty percent", "shields are down to half").
   - Every hit keeps its file, start/end time, speaker hint, and confidence.
3. **Cut.** Clips come from the centre channel of 5.1 sources, with Demucs vocal isolation when music or effects
   remain.
4. **Clean.**
   - Loudness goes to -18 LUFS (dialogue) and silence is trimmed.
   - Hails and viewscreen lines get a comm filter.
   - Exports are 48 kHz mono.
5. **Catalogue.** A local `voice-catalogue.json` (key → clips, source, voice id), which `Import-Assets.ps1` indexes
   into the resolved manifest.
6. **Review.** A local listening page (a file, never published) plays every candidate with approve / reject; only
   approved clips are imported.
7. **Gaps.** Lines still empty get a synthetic voice, marked as such in the catalogue so they can be replaced later.

## 4. Order of work

1. Transcribe the BOTF bank and map it to keys. This alone gives every major empire its own voice.
2. Map Bridge Commander's crew lines to Federation combat barks and acknowledgements, and Korbus to Klingon.
3. Extract DS9/VOY/TOS subtitles and search for Romulan, Cardassian, Ferengi and Klingon lines.
4. Run Whisper over TNG/ENT at low priority for the remaining gaps.
5. Split the cue keys in code (with tests), then import, run the review page, and fill gaps with text-to-speech.
