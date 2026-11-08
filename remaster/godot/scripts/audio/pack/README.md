# Audio pack (res://scripts/audio/pack)

Built, never committed: everything here except this file and `.gitignore` is private (the original BOTF banks and clips cut
from Simon's own Trek library) and is ignored by git.

    python remaster/tools/pack_audio_web.py --audio <LocalAssets/audio> --out remaster/godot/scripts/audio/pack

Then `godot --headless --path remaster/godot --import` so the .ogg files become imported resources. The runtime loads them
only through `AppLoader` (res:// and user://). Folders: `shared/` plus one per empire (`fed kli rom car fer`), each with a
`cues.json` and `voice/ sfx/ music/`. `pack_manifest.json` lists the packs, sizes and budget violations.

Export: the web preset must keep the pack in the pck, with `include_filter="scripts/audio/pack/*.json"` so the cue tables
ship beside the imported .ogg resources. Empire packs can be split into their own .pck and mounted with
`AppLoader.mount_pack(...)` then `BotfAudioLibrary.load_pack("kli")`.

Full build (all private inputs stay outside git; D:\ is read only by these offline steps, never by the game):

    # 1. original BOTF banks -> LocalAssets/audio/cues.json (build_audio_cues.py, see AUDIO-PLAN)
    # 2. Trek library -> LocalAssets/audio/trek (mine_trek_audio.py: index, cue, alert, scan/cut), then the era key table
    python remaster/tools/mine_trek_audio.py cues  --trek <LocalAssets/audio/trek>
    # 3. ogg pack (web-safe, budgets in pack_manifest.json), then import
    python remaster/tools/pack_audio_web.py --audio <LocalAssets/audio> --out remaster/godot/scripts/audio/pack --jobs 2
    godot --headless --audio-driver Dummy --path remaster/godot --import
    # 4. the cue trace proves every event reaches a recorded file
    godot --headless --audio-driver Dummy --path remaster/godot -s res://scripts/audio/tests/test_cue_trace.gd
    # 5. listen sheet for Simon (cue, source file, timecode)
    python remaster/tools/mine_trek_audio.py sheet --trek <LocalAssets/audio/trek> --out remaster/docs/AUDIO-LISTEN-SHEET.md

Never play the result aloud in a test run: always `--audio-driver Dummy`.
