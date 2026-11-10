extends SceneTree
## Packaged-exe check for the baked ship pictures (verify's exe-clean step, run in a folder with no LocalAssets and no BOTF_*
## variables): ship_icons_desktop.pck must mount and every icon in it must load through ShipIcons. Prints "ICON_CHECK ok|FAIL ..."
## lines; exit code 1 on any failure. A ship picture that would fall back to the vector line drawing counts as a failure.
##   BOTFRemastered.exe --audio-driver Dummy -s res://scenes/ui/tools/icon_check.gd

func _initialize() -> void:
	var mounted := AppLoader.mount_user_packs()
	var bad := PackedStringArray()
	var src := ShipIcons.source()
	var names := AppLoader.list(ShipIcons.PACK_DIR)
	var n := 0
	for f in names:
		if not f.ends_with("_iso.png"):
			continue
		var stem := f.trim_suffix("_iso.png")
		var fac := stem.get_slice("_", 0)
		var o := ShipIcons.FAC.find(fac)
		var klass := stem.substr(fac.length() + 1)
		n += 1
		if o < 1 or ShipIcons.texture(o, klass, "iso", 256) == null or ShipIcons.texture(o, klass, "top", 64) == null:
			bad.append(stem)
	# The pack's marker file states how many pictures it was built from (four per class: iso and top, 256 and 64 px).
	var want := int(AppLoader.text(ShipIcons.PACK_DIR + "/_pack.txt").split(" ")[1]) / 4 if AppLoader.exists(ShipIcons.PACK_DIR + "/_pack.txt") else 0
	var ok := mounted > 0 and src == "pack" and n > 0 and n == want and bad.is_empty()
	print("ICON_CHECK ", "ok " if ok else "FAIL ", "source=", src, ", ", n, " of ", want, " iso icons loaded from the pack, ", bad.size(), " failed", (" (" + ", ".join(bad.slice(0, 5)) + ")") if not bad.is_empty() else "")
	quit(0 if ok else 1)
