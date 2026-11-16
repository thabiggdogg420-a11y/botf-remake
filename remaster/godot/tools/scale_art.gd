extends SceneTree
## Scales generated art for the game: godot --headless --path remaster/godot -s res://tools/scale_art.gd -- <raw_dir> <out_dir> <px> [contact.png]
## Every <raw_dir>/*.png becomes <out_dir>/<same name>.png at px x px (Lanczos). With a 4th argument it also writes a contact sheet
## (4 columns, 300 px cells, alphabetical) so a batch can be looked at in one frame.
func _initialize() -> void:
	var a := OS.get_cmdline_user_args()
	var raw := str(a[0])
	var out := str(a[1])
	var px := int(a[2])
	DirAccess.make_dir_recursive_absolute(out)
	var names := []
	for f in DirAccess.get_files_at(raw):
		if f.ends_with(".png"):
			names.append(f)
	names.sort()
	var cells := []
	for f in names:
		var img := Image.load_from_file(raw.path_join(f))
		if img == null or img.is_empty():
			print("SKIP ", f)
			continue
		img.resize(px, px, Image.INTERPOLATE_LANCZOS)
		img.save_png(out.path_join(f))
		cells.append(img)
		print("scaled ", f)
	if a.size() > 3:
		var cols := 4
		var cell := 300
		var rows := ceili(cells.size() / float(cols))
		var sheet := Image.create(cols * cell, rows * cell, false, Image.FORMAT_RGBA8)
		sheet.fill(Color.BLACK)
		for i in cells.size():
			var c: Image = cells[i].duplicate()
			c.convert(Image.FORMAT_RGBA8)
			c.resize(cell - 4, cell - 4, Image.INTERPOLATE_LANCZOS)
			sheet.blit_rect(c, Rect2i(0, 0, cell - 4, cell - 4), Vector2i((i % cols) * cell + 2, (i / cols) * cell + 2))
		sheet.save_png(str(a[3]))
		print("sheet ", names)
	quit(0)
