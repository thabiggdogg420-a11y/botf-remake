extends SceneTree
## Saves the LCARS Theme resources (one per faction) built by Lcars.build_theme().
##   godot --headless --path remaster/godot -s res://scenes/ui/tools/build_theme.gd
## The app builds its theme at runtime (so palettes and the colour-blind setting apply live); the
## saved .tres files let editor-authored scenes use the same look.

func _initialize() -> void:
	DirAccess.make_dir_recursive_absolute("res://scenes/ui/theme")
	for id in range(1, 6):
		var key: String = Lcars.PALETTES[id]["key"]
		var t := Lcars.build_theme(id)
		var path := "res://scenes/ui/theme/lcars_%s.tres" % key
		var err := ResourceSaver.save(t, path)
		print(path, " ", "ok" if err == OK else "error %d" % err)
	quit(0)
