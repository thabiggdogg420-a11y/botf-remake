extends SceneTree
## Loads every script under scenes/ui and scenes/galaxy so parse errors surface in one run.
##   godot --headless --path remaster/godot -s res://scenes/ui/tools/check_scripts.gd

func _initialize() -> void:
	var failed := 0
	for dir in ["res://scenes/ui", "res://scenes/ui/screens", "res://scenes/galaxy", "res://scenes/ui/tests"]:
		var d := DirAccess.open(dir)
		if d == null:
			continue
		for f in d.get_files():
			if f.ends_with(".gd") and f != "check_scripts.gd":
				var s: Variant = load(dir + "/" + f)
				if s == null or not (s as GDScript).can_instantiate():
					failed += 1
					print("FAILED ", dir, "/", f)
	print("check done, failed: ", failed)
	quit(1 if failed > 0 else 0)
