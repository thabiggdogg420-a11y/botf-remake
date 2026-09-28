# Audio plan

28 September 2026. This covers voice, sound effects and music for the Unreal client. All third-party and original-game audio is **private and local only**. It lives in the gitignored `remaster/game/LocalAssets/` and `/Game/Local/Audio`, and it is resolved through `remaster/assets/manifest.json` (keys `weapon.*`, `sfx.*`, `vo.*`, `mus.*`). The shipped build must work silently, or with project-original placeholder cues, when nothing local is installed.

## 1. Original BOTF audio files

The original install folder (the one containing `trek.exe`) holds four sound banks next to `stbof.res`:

| File | Contents | Encoding (per Ultimate Editor source) | Default count |
|---|---|---|---|
| `sfx.snd` | UI clicks, explosions, ship propulsion, weapons | PCM 16-bit mono 22,050 Hz | 163 |
| `music.snd` | Soundtrack | IMA ADPCM (WAVE 0x11) stereo 22,050 Hz, block 1024 | – |
| `english.snd` / `german.snd` | Voice acting | IMA ADPCM mono 22,050 Hz, block 512, 1017 samples/block | 1,364 |

`stbof.res` is a ZIP. It holds the data tables and graphics. Among them is `converse.bin`, which the Ultimate Editor uses to assign each voice entry a race and a conversation group/list. The four `.snd` files are not ZIPs. Each starts with a 32-bit little-endian offset to an index at the end of the file. The index is a run of 16-byte entries (address, size, frame count, unknown; all uint32 LE). A final entry with zero frames is editor bookkeeping.

Sources: [Armada Fleet Command: "botf sfx and music"](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=2955), [How to edit English/German.snd](https://www.armadafleetcommand.com/onscreen/botf/viewforum.php?f=296), and the Ultimate Editor sources `source/ue/edit/snd/*.java` and `source/ue/audio/Wave.java` on [gitlab.com/stbotf/ultimate-editor](https://gitlab.com/stbotf/ultimate-editor). The extractor re-implements the layout; it copies no code.

### Extractor

`remaster/game/Tools/Extract-BotfAudio.py` (Python 3, standard library only):

```
python remaster\game\Tools\Extract-BotfAudio.py --botf "C:\Games\BOTF" --convert
# or, as part of the import:
.\Import-Assets.ps1 -BotfInstall "C:\Games\BOTF"
```

It reads only the user's own install. It writes `LocalAssets/botf-original/audio/<bank>/<bank>_NNNN.wav`, adding a RIFF header to the raw payloads, plus an `index.json` per bank (offset, size, frames, SHA-256). With `--convert` it also writes 48 kHz PCM copies through ffmpeg. Payloads that are already RIFF are kept byte-for-byte.

Caveats: the header layout was checked against a synthetic bank, not a retail file. The first run on a real install should confirm that `english_0000.wav` plays. The GOG, retail 1.0.2 and AIO overlays may differ. A modded `sfx.snd` can contain entries in other formats; they will be flagged when ffmpeg fails to convert them.

### Mapping extracted voice files to cues

Entry indices are stable within one install but not documented as named cues. The workflow:

1. Run the extractor, then open the Ultimate Editor on the same install. Its sound panel shows each voice entry's race and group (`[index]: [race] [group/list, type]`). Its description export (`english.dsc`) can be saved next to the bank.
2. Fill `LocalAssets/botf-original/cue-map.json`, for example `{"vo.combat.ack.fed": ["english/english_0412.48k.wav", "english/english_0413.48k.wav"]}`. The importer resolves `botf-install` candidates only through this map.
3. A later lane can automate step 2 by parsing `converse.bin` from `stbof.res`. This depends on the Ultimate Editor `Converse` reader, which has not been inspected yet.

## 2. Voice line categories

### Present in original BOTF (race-specific voice sets for the five majors, plus neutral and alien)

| Category | Examples of what the original voices | Remaster key |
|---|---|---|
| Advisor / event announcements | new turn events, a colony founded, construction complete, research breakthrough, a system under attack, unrest, a race met | `vo.advisor.<fac>` |
| Diplomacy (converse) | greetings, accepting or refusing proposals, treaty announcements, declarations of war; the race of the speaker is fixed by the other empire | `vo.diplomacy.<fac>`, `vo.minor.generic` |
| Tactical combat | order acknowledgements when a target or tactic is set (the tutorial relies on this confirmation), battle start and end | `vo.combat.ack.<fac>` |

### Expansion categories (new; local recordings, TTS placeholders or silent)

| Category | Why | Key pattern |
|---|---|---|
| Station / outpost events | stations and outposts come from GAMEPLAY-REMASTER lane D | `vo.station.<fac>.<event>` |
| Anomaly / event-deck cards | anomalies and event deck | `vo.event.<cardId>` |
| Victory types | scientific and diplomatic victory, plus the chronicle | `vo.victory.<type>.<fac>` |
| Treaty effects | allied supply, shared sensors, refusing a call to arms | `vo.treaty.<effect>.<fac>` |
| Per-ship battle orders | ram, cloak ambush, disengage, hail (lane A/H) | `vo.combat.<order>.<fac>` |
| Minor-race personalities | the 30-race roster (lane G2) | `vo.minor.<race>` |

Rule: a missing expansion cue falls back to the matching original category, then to a UI beep, never to another faction's voice.

## 3. Event to cue mapping

`Priority` decides what plays when several events fire in one frame: higher wins and lower is queued (voice) or mixed (sfx). Voice uses one channel per listener with a 0.6 s gap; sfx up to 24 voices with distance attenuation in the tactical scene.

| Game event (sim / UI) | SFX key | Voice key | Priority | Notes |
|---|---|---|---:|---|
| Turn start | `sfx.ui.beep` | – | 1 | |
| Research complete | `sfx.ui.beep` | `vo.advisor.<fac>` | 4 | |
| Construction complete | `sfx.ui.beep` | `vo.advisor.<fac>` | 3 | |
| Colony founded / outpost built | `sfx.ui.beep` | `vo.advisor.<fac>` / `vo.station.*` | 4 | |
| System under attack / enemy fleet sighted | `sfx.alert.red` | `vo.advisor.<fac>` | 8 | |
| First contact | – | `vo.diplomacy.<other>` | 7 | speaker is the other empire |
| Diplomatic proposal received / answered | – | `vo.diplomacy.<other>` | 6 | |
| War declared | `sfx.alert.red` | `vo.diplomacy.<other>` | 9 | |
| Treaty signed / effect triggered | `sfx.ui.beep` | `vo.treaty.*` | 5 | |
| Anomaly / event card | `sfx.ui.beep` | `vo.event.<cardId>` | 5 | |
| Victory / defeat | – | `vo.victory.*` | 10 | music sting |
| Encounter window opens (Hail/Auto/Fight) | `sfx.alert.red` | – | 8 | |
| Tactical order accepted | `sfx.ui.beep` | `vo.combat.ack.<fac>` | 6 | required feedback per COMBAT-REFERENCE |
| Beam fires (phaser / disruptor / spiral / whip / cutting beam) | `weapon.PhaserArray`, `weapon.Disruptor`, `weapon.Disruptor.rom`, `weapon.Disruptor.car` (spiral), `weapon.PhaserArray.fer` (plasma whip), `weapon.CuttingBeam`; generally `weapon.<Type>.<fac>` then `weapon.<Type>` | – | 3 | bound by weapon `socketPrefix` |
| Pulse cannon fires | `weapon.PulsePhaser` | – | 3 | |
| Torpedo launch | `weapon.PhotonTorpedo` / `weapon.QuantumTorpedo` / `weapon.PlasmaTorpedo` | – | 3 | |
| Shield hit | `sfx.shield.hit` | – | 2 | |
| Hull hit | `sfx.explosion.small` | – | 2 | |
| Ship destroyed | `sfx.explosion.large` | – | 5 | |
| Cloak / decloak | `sfx.cloak.<fac>` / `sfx.decloak.<fac>` | – | 4 | Klingon falls back to Romulan |
| Retreat / warp out | `sfx.warp.out` | – | 3 | |
| Fleet arrives (strategic map) | `sfx.warp.in` | – | 2 | |
| Background | – | – | – | `mus.botf.all` by context (galaxy, combat, diplomacy); silent if missing |

## 4. Unreal integration

- The importer puts WAVs under `/Game/Local/Audio/<Weapons|Combat|UI|Voice|Music>/<key>` and records their object paths (`ueAssets`) in `LocalAssets/resolved-manifest.json`; `ue_import_local_assets.py` rewrites them with the paths Unreal actually created.
- `UBotfAssetRegistry` reads that file and walks each key's fallback chain (`BotfAssetCatalog.h`). A missing asset yields silence, never a crash.
- `UBotfAudioSubsystem` (`BotfAudioSubsystem.cpp`) plays cues chosen by `BotfAudioCues.h` / `BotfAudioEvents.h`: the table in section 3 (keys and priority), per-key cooldowns (weapons 0.08 s, UI 0.12 s, other sfx 0.15 s, alerts 3 s, a repeated voice line 4 s), one voice line at a time with a 0.6 s gap and a queue of 4 ordered by priority (lines older than 6 s are dropped), at most 24 sfx with the lowest priority culled, and ducking of sfx and music to 45 % while a voice line plays. Voice keys with several files rotate through them.
- Categories are SFX, Voice, UI and Music plus a master level, each a slider in Settings, stored in `GameUserSettings.ini` `[BotfAudio]`.
- Hooks: turn briefing (`FinishTurn`: one cue per kind from the chronicle, battles and outcome), encounter start, tactical orders (`vo.combat.<order>.<fac>` then `vo.combat.ack.<fac>`), each resolved round (weapon fire by the attacker's dominant roster weapon, hits, losses, decloaks), and diplomatic orders (war, peace, trade treaty). Music uses `mus.botf.<galaxy|combat>` then `mus.botf.all`.
- The simulation must not depend on audio, and replays must play the same cues from the event log.

## 5. Tools

| Tool | Needed for | Install |
|---|---|---|
| ffmpeg | converting anything to 48 kHz WAV and loudness normalisation | `winget install Gyan.FFmpeg` |
| Python 3 | `Extract-BotfAudio.py` | `winget install Python.Python.3.12` |
| Ultimate Editor | naming voice entries (cue map) | [Armada Fleet Command UE thread](https://www.armadafleetcommand.com/onscreen/botf/viewforum.php?f=125) |
