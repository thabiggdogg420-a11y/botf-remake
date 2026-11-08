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
