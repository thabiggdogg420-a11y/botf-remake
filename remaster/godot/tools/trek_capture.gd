extends SceneTree
## Proof frames for the Trek insignia / ship-icon lane (full 1920x1080, a real GPU window; always --audio-driver Dummy).
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://tools/trek_capture.gd -- <out_dir> <mode> [args]
## Modes:
##   insignia            insignia-sheet.png: every insignia at 14/30/84 px on the dark UI background, labelled
##   fallback            ship-fallback-sheet.png: every vector hull family, station and contact glyph
##   ships [page]        ship-icons-sheet[-N].png: every baked icon (both views) with its class name (needs LocalAssets/ship_icons)
##   screens <faction>   <faction>-<screen>.png for galaxy fleets empire diplomacy new research system database (faction 1..5)

var out_dir := ""
var mode := ""
var args2: Array = []

func _initialize() -> void:
	var a := OS.get_cmdline_user_args()
	out_dir = a[0]
	mode = a[1]
	args2 = a.slice(2)
	DirAccess.make_dir_recursive_absolute(out_dir)
	_run.call_deferred()

func _wait(n: int = 6) -> void:
	for i in n:
		await process_frame

func _save(name_: String) -> void:
	await _wait(6)
	var img := root.get_viewport().get_texture().get_image()
	img.save_png(out_dir.path_join(name_))
	print("saved ", out_dir.path_join(name_), " ", img.get_size())

func _run() -> void:
	match mode:
		"insignia":
			await _sheet(InsigniaSheet.new(), "insignia-sheet.png")
		"fallback":
			await _sheet(FallbackSheet.new(), "ship-fallback-sheet.png")
		"ships":
			var pages := ShipIconSheet.pages()
			print("baked classes: ", ShipIconSheet.classes().size(), " pages ", pages)
			for pg in pages:
				var sh := ShipIconSheet.new()
				sh.page = pg
				await _sheet(sh, "ship-icons-sheet.png" if pg == 0 else "ship-icons-sheet-%d.png" % (pg + 1))
		"screens":
			await _screens(int(args2[0]) if args2.size() > 0 else 1)
	quit(0)

func _sheet(c: Control, name_: String) -> void:
	var bg := ColorRect.new()
	bg.color = Lcars.BG
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_child(bg)
	c.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_child(c)
	await _save(name_)
	c.queue_free()
	bg.queue_free()
	await _wait(2)

func _screens(faction: int) -> void:
	var app: App = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	await _wait(6)
	var tag: String = ["x", "fed", "kli", "rom", "car", "fer"][faction]
	app.start_campaign({"faction": faction, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48, "preset": "remastered", "events": true, "aiAggression": true})
	if app.core.is_native():
		for n in 14:
			app.end_turn()
			app.close_all_modals()
	app.close_all_modals()
	var shots: Array = args2.slice(1) if args2.size() > 1 else ["galaxy", "fleets", "empire", "diplomacy", "new", "research", "system", "database"]
	for s in shots:
		match s:
			"new":
				var keep: Dictionary = app.view
				app.view = {}
				app.goto("new")
				await _wait(4)
				app.current_screen()._pick_faction(faction) # the picker with this game's empire chosen
				await _wait(10)
				for t in app.get_node("Toasts").get_children():
					t.queue_free()
				await _save("%s-%s%s.png" % [tag, s, OS.get_environment("TREK_SUFFIX")])
				app.view = keep
				continue
			"galaxy":
				app.goto("galaxy")
				var cols: Array = app.view.get("fleets", [])
				if cols.size() > 0:
					app.select_fleet(int(cols[0]["id"]))
			"galaxy-close":
				# The chart close up on the lead fleet with two foreign contacts, an unknown contact, a starbase and an outpost added
				# to the view (test data, for the frame only) so every map glyph is on screen at once.
				app.goto("galaxy")
				await _wait(4)
				var f0: Dictionary = app.view["fleets"][0]
				var other1 := 2 if faction != 2 else 3
				var other2 := 4 if faction != 4 else 5
				app.view["contacts"] = [
					{"id": 9001, "owner": other1, "ownerName": "Foreign", "name": "K'T'Inga — Battle Cruiser II", "className": "Destroyer", "x": int(f0["x"]) + 1, "y": int(f0["y"]), "atWar": false},
					{"id": 9002, "owner": other2, "ownerName": "Foreign", "name": "Galor — Strike Cruiser", "className": "Destroyer", "x": int(f0["x"]) + 1, "y": int(f0["y"]) + 1, "atWar": true},
					{"id": 9003, "owner": 0, "ownerName": "Unknown", "name": "Unknown contact", "className": "", "x": int(f0["x"]), "y": int(f0["y"]) + 1, "atWar": false}]
				app.view["stations"] = [
					{"id": 9101, "owner": faction, "ownerName": "Own", "mine": true, "name": "Starbase 1", "type": "Starbase", "typeName": "Starbase", "x": int(f0["x"]) - 1, "y": int(f0["y"]), "hull": 230, "shields": 700, "builtTurn": 6, "maxHull": 230, "maxShields": 700, "beam": 33, "torpedo": 27, "supplyRadius": 5, "sensorRadius": 3, "upkeep": 104, "repairPercent": 20},
					{"id": 9102, "owner": faction, "ownerName": "Own", "mine": true, "name": "Outpost 1", "type": "Outpost", "typeName": "Outpost", "x": int(f0["x"]) - 1, "y": int(f0["y"]) + 1, "hull": 90, "shields": 100, "builtTurn": 6, "maxHull": 90, "maxShields": 100, "beam": 0, "torpedo": 0, "supplyRadius": 2, "sensorRadius": 2, "upkeep": 10, "repairPercent": 5}]
				var gm: GalaxyMap = app.current_screen().map
				gm.rebuild()
				app.select_cell(0) # nothing selected: the selection ring would cover the hull
				gm.focus_cell(int(f0["x"]), int(f0["y"]), 3.0, false)
				await _wait(8)
			"system-ships":
				app.goto("system")
				await _wait(4)
				var sc: Node = app.current_screen()
				sc.tab = "ships"
				sc.refresh()
			_:
				app.goto(s)
		await _wait(10)
		for t in app.get_node("Toasts").get_children():
			t.queue_free()
		await _save("%s-%s%s.png" % [tag, s, OS.get_environment("TREK_SUFFIX")])
	app.queue_free()

# ---------------------------------------------------------------------------------------------
class InsigniaSheet extends Control:
	func _draw() -> void:
		var f := Lcars.font("display", 600)
		var m := Lcars.font("mono", 400)
		draw_string(f, Vector2(30, 40), "INSIGNIA SHEET  (14 px / 30 px / 84 px, plus the 84 px mark on an accent elbow)", HORIZONTAL_ALIGNMENT_LEFT, -1, 24, Lcars.TEXT)
		var keys: Array = Insignia.ALL
		var cols := 4
		var cw := 470.0
		var rh := 190.0
		for i in keys.size():
			var col_i: int = i % cols
			var row_i: int = i / cols
			var o := Vector2(30 + col_i * cw, 70 + row_i * rh)
			var k: String = keys[i]
			var id := Insignia.BY_ID.find(k)
			var tint: Color = Lcars.owner_color(id) if id >= 0 else Color("#b9c4d8")
			draw_rect(Rect2(o, Vector2(cw - 14, rh - 12)), Color(Lcars.PANEL, 0.7))
			draw_string(m, o + Vector2(10, 22), k.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, -1, 15, Lcars.TEXT2)
			Insignia.draw(self, k, Rect2(o + Vector2(14, 80), Vector2(14, 14)), tint)
			Insignia.draw(self, k, Rect2(o + Vector2(50, 70), Vector2(30, 30)), tint)
			Insignia.draw(self, k, Rect2(o + Vector2(100, 40), Vector2(84, 84)), tint)
			draw_rect(Rect2(o + Vector2(200, 40), Vector2(86, 86)), Lcars.accent())
			Insignia.draw(self, k, Rect2(o + Vector2(201, 41), Vector2(84, 84)), Lcars.ink(), "auto", Lcars.accent())
			Insignia.draw(self, k, Rect2(o + Vector2(300, 60), Vector2(30, 30)), Lcars.ink() if false else tint, "small")
			Insignia.draw(self, k, Rect2(o + Vector2(340, 40), Vector2(84, 84)), tint, "outline")
			draw_string(m, o + Vector2(14, 160), "14", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Lcars.TEXT3)
			draw_string(m, o + Vector2(54, 160), "30", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Lcars.TEXT3)
			draw_string(m, o + Vector2(100, 160), "84", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Lcars.TEXT3)
			draw_string(m, o + Vector2(200, 160), "on elbow", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Lcars.TEXT3)
			draw_string(m, o + Vector2(300, 160), "30 small", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Lcars.TEXT3)
			draw_string(m, o + Vector2(340, 160), "outline", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Lcars.TEXT3)

class FallbackSheet extends Control:
	func _draw() -> void:
		var f := Lcars.font("display", 600)
		var m := Lcars.font("mono", 400)
		draw_string(f, Vector2(30, 40), "VECTOR FALLBACK  (hull families, station, outpost, contact; used when LocalAssets has no baked icon)", HORIZONTAL_ALIGNMENT_LEFT, -1, 22, Lcars.TEXT)
		var fams: Array = ShipGlyph.FAMILIES
		for i in fams.size():
			var o := Vector2(30 + (i % 4) * 470.0, 70 + (i / 4) * 200.0)
			draw_rect(Rect2(o, Vector2(456, 188)), Color(Lcars.PANEL, 0.7))
			var fam: String = fams[i]
			var tint := Lcars.owner_color(1 + (i % 5))
			draw_string(m, o + Vector2(10, 22), fam, HORIZONTAL_ALIGNMENT_LEFT, -1, 15, Lcars.TEXT2)
			ShipGlyph.draw_hull(self, fam, o + Vector2(110, 110), 150.0, 0.0, tint, true, true, 0.3)
			ShipGlyph.draw_hull(self, fam, o + Vector2(250, 110), 56.0, 0.0, tint, true, true, 0.3)
			ShipGlyph.draw_hull(self, fam, o + Vector2(330, 110), 30.0, -0.8, tint, false, true, 0.5)
			ShipGlyph.draw_hull(self, fam, o + Vector2(380, 110), 16.0, 0.6, tint, false, true, 0.5)
			ShipGlyph.draw_hull(self, fam, o + Vector2(420, 110), 22.0, -PI / 2.0, tint, false, true, 0.5)
		var base := Vector2(30 + 1 * 470.0, 70 + 3 * 200.0)
		var tn := Lcars.owner_color(1)
		draw_rect(Rect2(base, Vector2(456, 188)), Color(Lcars.PANEL, 0.7))
		draw_string(m, base + Vector2(10, 22), "starbase / outpost / contact / unknown contact", HORIZONTAL_ALIGNMENT_LEFT, -1, 15, Lcars.TEXT2)
		ShipGlyph.draw_station(self, base + Vector2(60, 100), 36.0, tn, true)
		ShipGlyph.draw_station(self, base + Vector2(150, 100), 30.0, tn, false)
		ShipGlyph.draw_contact(self, base + Vector2(250, 100), 30.0, Lcars.owner_color(2), 2, "K'T'Inga")
		ShipGlyph.draw_contact(self, base + Vector2(340, 100), 30.0, Lcars.owner_color(3), 3, "D'deridex")
		ShipGlyph.draw_contact(self, base + Vector2(420, 100), 22.0, Lcars.owner_color(0), 0, "")
		ShipGlyph.draw_station(self, base + Vector2(60, 160), 9.0, tn, true)
		ShipGlyph.draw_station(self, base + Vector2(100, 160), 8.0, tn, false)
		ShipGlyph.draw_contact(self, base + Vector2(140, 160), 8.0, Lcars.owner_color(4), 4, "Galor")
		# the old stand-ins for comparison
		var old := Vector2(30 + 3 * 470.0, 70 + 3 * 200.0)
		draw_rect(Rect2(old, Vector2(456, 188)), Color(Lcars.PANEL, 0.7))
		draw_string(m, old + Vector2(10, 22), "icons at map sizes (30 px hulls, owner colours)", HORIZONTAL_ALIGNMENT_LEFT, -1, 15, Lcars.TEXT2)
		for j in 5:
			var fam2: String = ["saucer", "kli_bop", "warbird", "galor", "marauder"][j]
			ShipGlyph.draw_hull(self, fam2, old + Vector2(40 + j * 80, 90), 30.0, -PI / 4.0, Lcars.owner_color(j + 1), false, true, 0.55)
			ShipGlyph.draw_hull(self, fam2, old + Vector2(40 + j * 80, 140), 20.0, 0.0, Lcars.owner_color(j + 1), false, true, 0.55)

class ShipIconSheet extends Control:
	var page := 0
	var _tx := {}   ## path -> ImageTexture, kept alive until the frame is drawn (a texture freed inside _draw draws white)
	func _tex(path: String) -> Texture2D:
		if not _tx.has(path):
			var img := Image.load_from_file(path)
			_tx[path] = ImageTexture.create_from_image(img) if img != null and not img.is_empty() else null
		return _tx[path]
	const PER_PAGE := 20
	static func classes() -> PackedStringArray:
		var out := PackedStringArray()
		var base := ShipIcons.folder()
		if base == "":
			return out
		for f in DirAccess.get_files_at(base):
			if f.ends_with("_iso.png"):
				out.append(f.trim_suffix("_iso.png"))
		out.sort()
		return out
	static func pages() -> Array:
		var n := maxi(1, int(ceil(classes().size() / float(PER_PAGE))))
		var out := []
		for i in n:
			out.append(i)
		return out
	func _draw() -> void:
		var f := Lcars.font("display", 600)
		var m := Lcars.font("mono", 400)
		var cl := classes()
		draw_string(f, Vector2(30, 36), "BAKED SHIP ICONS  page %d  (%d classes; left: three-quarter top, right: top-down)" % [page + 1, cl.size()], HORIZONTAL_ALIGNMENT_LEFT, -1, 22, Lcars.TEXT)
		for i in PER_PAGE:
			var idx := page * PER_PAGE + i
			if idx >= cl.size():
				break
			var o := Vector2(20 + (i % 5) * 380.0, 52 + (i / 5) * 256.0)
			draw_rect(Rect2(o, Vector2(372, 248)), Color(Lcars.PANEL, 0.7))
			for v in 2:
				var view: String = ["iso", "top"][v]
				var path := ShipIcons.folder().path_join("%s_%s.png" % [cl[idx], view])
				var tx := _tex(path)
				if tx != null:
					draw_texture_rect(tx, Rect2(o + Vector2(4 + v * 184, 6), Vector2(180, 180)), false)
			draw_string(m, o + Vector2(10, 214), cl[idx], HORIZONTAL_ALIGNMENT_LEFT, -1, 15, Lcars.TEXT)
			var small := _tex(ShipIcons.folder().path_join("%s_iso_64.png" % cl[idx]))
			if small != null:
				draw_texture(small, o + Vector2(300, 180))
