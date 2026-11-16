extends SceneTree
## PortraitSlot: a picture is cropped to cover its slot, never stretched, and the eight powers each have a portrait key.
##   godot --headless --path remaster/godot -s res://scenes/ui/tests/test_portrait_slot.gd
## Exit code 0 on success, 1 on any failure.

var failures := 0

func check(cond: bool, what: String) -> void:
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

func _initialize() -> void:
	var tex := Vector2(1024, 768)
	for slot in [Vector2(300, 170), Vector2(420, 236), Vector2(76, 62), Vector2(112, 112), Vector2(96, 96), Vector2(160, 400)]:
		var r := PortraitSlot.cover_src(tex, slot)
		check(absf(r.size.x / r.size.y - slot.x / slot.y) < 0.002, "crop of %s matches the aspect of a %s slot (no stretch)" % [tex, slot])
		check(r.position.x >= -0.01 and r.position.y >= -0.01 and r.end.x <= tex.x + 0.01 and r.end.y <= tex.y + 0.01, "crop stays inside the picture for %s" % slot)
		check(r.size.x == tex.x or r.size.y == tex.y, "crop uses the full width or full height for %s (covers, not letterboxed)" % slot)
	var wide := PortraitSlot.cover_src(tex, Vector2(300, 170))
	check(wide.position.y > 0.0 and wide.position.y < (tex.y - wide.size.y) * 0.5, "a wide slot's window sits above centre, where the face is")
	var tall := PortraitSlot.cover_src(Vector2(160, 170), Vector2(420, 236))
	check(tall.size.x == 160.0, "the old 160x170 stills are cropped, not squashed, in the 420x236 slot")
	for id in range(1, 9):
		check(PortraitSlot.MAJOR_KEYS.has(id) and str(PortraitSlot.MAJOR_KEYS[id]).begins_with("portrait."), "power %d has a portrait key" % id)
	quit(1 if failures > 0 else 0)
