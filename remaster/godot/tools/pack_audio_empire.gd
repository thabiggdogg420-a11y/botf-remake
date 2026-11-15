extends SceneTree
## Web audio split (M1-WEBPACK). The main web pck carries only the `shared` audio pack (export preset excludes the empire
## folders); this build step writes one raw-ogg pck per empire, audio_<emp>_web.pck, which the web client downloads only for the
## empire a campaign picks (AppLoader.fetch_empire_audio). Raw .ogg + cues.json, so no import step; BotfAudioLibrary.stream_at
## reads them with AudioStreamOggVorbis.load_from_file.
##   godot --headless --audio-driver Dummy --path remaster/godot -s res://tools/pack_audio_empire.gd -- <pack_dir> <out_dir>
## <pack_dir> is the output of tools/pack_audio_web.py (the folder with shared/, fed/, ... and pack_manifest.json).
## Copy the pcks to <web export>/packs/. Build tool: reads disk on purpose; the game never does.

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() < 2:
		printerr("usage: -- <pack_dir> <out_dir>")
		quit(2)
		return
	DirAccess.make_dir_recursive_absolute(args[1])
	var total := 0
	for emp in ["fed", "kli", "rom", "car", "fer"]:
		var src: String = str(args[0]).path_join(emp)
		if not DirAccess.dir_exists_absolute(src):
			continue
		var out: String = str(args[1]).path_join("audio_%s_web.pck" % emp)
		var pk := PCKPacker.new()
		if pk.pck_start(out) != OK:
			printerr("cannot start ", out)
			quit(1)
			return
		var n := _walk(pk, src, "res://scripts/audio/pack/" + emp)
		pk.flush(false)
		var sz := FileAccess.get_file_as_bytes(out).size()
		total += sz
		print("%s: %d files, %.1f MB" % [out.get_file(), n, sz / 1048576.0])
	# The shared music (about 9 MB of ogg) is its own web pack, fetched after the title: the main pck stays small.
	var music := str(args[0]).path_join("shared/music")
	if DirAccess.dir_exists_absolute(music):
		var mout: String = str(args[1]).path_join("audio_music_web.pck")
		var mp := PCKPacker.new()
		if mp.pck_start(mout) != OK:
			printerr("cannot start ", mout)
			quit(1)
			return
		var mn := _walk(mp, music, "res://scripts/audio/pack/shared/music")
		mp.flush(false)
		print("%s: %d files, %.1f MB" % [mout.get_file(), mn, FileAccess.get_file_as_bytes(mout).size() / 1048576.0])
	quit(0)

func _walk(pk: PCKPacker, dir: String, dest: String) -> int:
	var n := 0
	var d := DirAccess.open(dir)
	if d == null:
		return 0
	for sub in d.get_directories():
		n += _walk(pk, dir.path_join(sub), dest.path_join(sub))
	for f in d.get_files():
		if f.get_extension().to_lower() in ["ogg", "json"]:
			pk.add_file(dest.path_join(f), dir.path_join(f))
			n += 1
	return n
