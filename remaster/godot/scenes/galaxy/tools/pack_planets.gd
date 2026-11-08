extends SceneTree
## Pack-building step for the planet textures (M0-LANES "Browser target": the runtime reads no disk paths). Packs a folder of
## L7 planet sets (planets/<class>/<tier>/v<n>_albedo.png ...) into a .pck whose files sit under res://assets/planets/.
##   godot --headless --path remaster/godot -s res://scenes/galaxy/tools/pack_planets.gd -- <planets_dir> <out.pck> [web|desktop]
## Two packs, two names (M1-WEBPACK): `web` -> planets_web.pck, only the 1024x512 "web" tier, albedo/clouds/night (no rough, no
## normal, no meshes); copy it to <web export>/packs/, where AppLoader.fetch_web_packs downloads it. `desktop` (default) ->
## planets_desktop.pck, the 4096 desktop tier plus the web tier (the galaxy map reads the small tier first); copy it to the user
## data folder's packs/, mounted at boot by AppLoader.mount_user_packs. The web build never sees the desktop pack.
## Build tool: it reads disk on purpose; the game never does.

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() < 2:
		printerr("usage: -- <planets_dir> <out.pck> [web|desktop]")
		quit(2)
		return
	var web_only := args.size() > 2 and args[2] == "web"
	var pk := PCKPacker.new()
	if pk.pck_start(args[1]) != OK:
		printerr("cannot start ", args[1])
		quit(1)
		return
	var n := _walk(pk, args[0], "res://assets/planets", web_only)
	pk.flush(true)
	print("packed %d files into %s" % [n, args[1]])
	quit(0 if n > 0 else 1)

func _walk(pk: PCKPacker, dir: String, dest: String, web_only: bool) -> int:
	var n := 0
	var d := DirAccess.open(dir)
	if d == null:
		return 0
	for sub in d.get_directories():
		if web_only and sub == "desktop":
			continue
		n += _walk(pk, dir.path_join(sub), dest.path_join(sub), web_only)
	for f in d.get_files():
		if web_only and (f.contains("_rough") or f.contains("_normal")):
			continue
		if f.get_extension().to_lower() in ["png", "webp", "jpg"]:
			pk.add_file(dest.path_join(f), dir.path_join(f))
			n += 1
	return n
