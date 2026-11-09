extends SceneTree
## Packs the files named in a list into one .pck (build step for the ship packs, tools/build_ship_packs.py). Each line of the
## list is `res://<path in the pack>|<disk path>`; blank lines and lines starting with # are skipped.
##   godot --headless --audio-driver Dummy --path remaster/godot -s res://tools/pack_list.gd -- <list.txt> <out.pck>
## Build tool: reads disk on purpose; the game never does (it mounts the pck through AppLoader).

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() < 2:
		printerr("usage: -- <list.txt> <out.pck>")
		quit(2)
		return
	var lines := FileAccess.get_file_as_string(args[0]).split("\n")
	var pk := PCKPacker.new()
	if pk.pck_start(args[1]) != OK:
		printerr("cannot start ", args[1])
		quit(1)
		return
	var n := 0
	for raw in lines:
		var line := raw.strip_edges()
		if line == "" or line.begins_with("#"):
			continue
		var parts := line.split("|")
		if parts.size() != 2 or not FileAccess.file_exists(parts[1]):
			printerr("missing or bad line: ", line)
			quit(1)
			return
		if pk.add_file(parts[0], parts[1]) != OK:
			printerr("cannot add ", parts[1])
			quit(1)
			return
		n += 1
	pk.flush(false)
	var f := FileAccess.open(args[1], FileAccess.READ)
	print("packed %d files into %s (%.1f MB)" % [n, args[1], (f.get_length() if f else 0) / 1048576.0])
	quit(0 if n > 0 else 1)
