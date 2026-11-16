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
	await _late_pack_test()
	quit(1 if failures > 0 else 0)

## Cold web visit: the card is drawn before art_web.pck mounts. A slot built with no art must show the portrait once the pack mounts.
func _late_pack_test() -> void:
	var key := "portrait.test.latepack"
	var slot := PortraitSlot.make(1, Vector2(300, 170), key)
	root.add_child(slot)
	await process_frame   # the root is busy during _initialize; the slot enters the tree (and runs _ready) next frame
	check(not slot.has_art(), "late pack: no art yet, the slot shows the emblem")
	check(AppLoader.art(key, "portrait") == null, "late pack: a missing picture is not cached as null")
	var dir := OS.get_temp_dir().path_join("botf_latepack_test")
	DirAccess.make_dir_recursive_absolute(dir)
	var png := dir.path_join("t.png")
	var img := Image.create(8, 8, false, Image.FORMAT_RGBA8)
	img.fill(Color.RED)
	img.save_png(png)
	DirAccess.make_dir_recursive_absolute("user://packs")
	var pck := "user://packs/art_latepack_test.pck"
	var packer := PCKPacker.new()
	packer.pck_start(pck)
	packer.add_file("res://art/portrait/%s.png" % key, png)
	packer.flush()
	check(AppLoader.mount_pack(pck), "late pack: the pack mounts")
	check(slot.has_art(), "late pack: the slot swapped the emblem for the portrait after the mount signal")
	check(AppLoader.art(key, "portrait") != null, "late pack: AppLoader.art now serves the picture")
	slot.queue_free()
	DirAccess.remove_absolute(pck)
