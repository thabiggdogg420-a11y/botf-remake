extends SceneTree
## Insignia, hull glyphs and baked-icon lookup: every mark builds and draws at 14/30/84 px, the class-to-family mapping holds,
## the icon file naming and the hide override work.
##   godot --headless --path remaster/godot -s res://scenes/ui/tests/test_trek_icons.gd
## Exit code 0 on success, 1 on any failure.

var failures := 0

func check(cond: bool, what: String) -> void:
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

class Canvas extends Control:
	var keys: Array = []
	func _draw() -> void:
		for k in keys:
			for px in [14, 30, 84]:
				Insignia.draw(self, k, Rect2(Vector2(10, 10), Vector2(px, px)), Color.WHITE)
				Insignia.draw(self, k, Rect2(Vector2(10, 10), Vector2(px, px)), Color.WHITE, "outline")
		for f in ShipGlyph.FAMILIES:
			ShipGlyph.draw_hull(self, f, Vector2(50, 50), 40.0, 0.3, Color.WHITE)
		ShipGlyph.draw_station(self, Vector2(50, 50), 10.0, Color.WHITE, true)
		ShipGlyph.draw_station(self, Vector2(50, 50), 10.0, Color.WHITE, false)
		ShipGlyph.draw_contact(self, Vector2(50, 50), 10.0, Color.WHITE, 2, "K'T'Inga")
		ShipGlyph.draw_contact(self, Vector2(50, 50), 10.0, Color.WHITE, 0, "")

## Draws every UI icon at three sizes (M4 Trekify round 2).
class IconCanvas extends Control:
	func _draw() -> void:
		for n in UiIcon.NAMES:
			for px in [14, 24, 48]:
				UiIcon.draw_icon(self, n, Rect2(Vector2(4, 4), Vector2(px, px)), Color.WHITE)
		for e in UiTokens.ERA_ORDER:
			UiTokens.era = e
			UiIcon.draw_icon(self, "intel", Rect2(Vector2(4, 4), Vector2(24, 24)), Color.WHITE)
		UiIcon.draw_seg_ring(self, Vector2(50, 50), 20.0, Color.WHITE)
		UiIcon.draw_reticle(self, Vector2(50, 50), 16.0, Color.WHITE)
		UiIcon.draw_octagon(self, Vector2(50, 50), 12.0, Color.BLACK, Color.WHITE)
		UiIcon.draw_delta(self, Vector2(50, 50), 10.0, 0.5, Color.WHITE)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var all: Array = Insignia.ALL.duplicate()
	all.append_array([0, 1, 2, 3, 4, 5, 99, "Dominion", "nonsense"])
	for k in all:
		var small: Array = Insignia.ops_for(Insignia.key_of(k), true)
		var full: Array = Insignia.ops_for(Insignia.key_of(k), false)
		check(not small.is_empty() and not full.is_empty(), "insignia %s has both variants" % str(k))
	check(Insignia.key_of(1) == "federation" and Insignia.key_of(2) == "klingon" and Insignia.key_of(3) == "romulan" and Insignia.key_of(4) == "cardassian" and Insignia.key_of(5) == "ferengi", "empire ids map to their marks")
	check(Insignia.key_of(0) == "neutral" and Insignia.key_of(99) == "neutral" and Insignia.key_of("nonsense") == "neutral", "unknown owners get the neutral mark")
	check(Insignia.key_of("Dominion") == "dominion", "roster faction names resolve")
	var c := Canvas.new()
	c.keys = all
	c.size = Vector2(200, 200)
	root.add_child(c)
	await process_frame
	await process_frame
	check(true, "every mark and hull family drew without error")
	check(ShipGlyph.family_for(1, "Galaxy") == "saucer" and ShipGlyph.family_for(1, "Defiant") == "fed_dart", "Federation hulls: saucer, Defiant dart")
	check(ShipGlyph.family_for(2, "B'rel") == "kli_bop" and ShipGlyph.family_for(2, "K'T'Inga") == "kli_cruiser", "Klingon hulls: bird of prey, cruiser")
	check(ShipGlyph.family_for(3, "D'deridex") == "warbird" and ShipGlyph.family_for(4, "Galor") == "galor" and ShipGlyph.family_for(5, "D'Kora") == "marauder", "Romulan, Cardassian, Ferengi families")
	check(ShipGlyph.family_for(0, "Borg Cube") == "borg_cube" and ShipGlyph.family_for(9, "") == "raider", "other powers")
	check(ShipGlyph.class_of({"designClass": "Oberth"}) == "Oberth" and ShipGlyph.class_of({"name": "Galaxy - Command Cruiser"}) == "Galaxy", "class name from a fleet or contact")
	check(ShipGlyph.class_of({"name": "K'T'Inga \u00e2\u20ac\u201d Battle Cruiser II"}) == "K'T'Inga", "class name survives a mangled dash")
	check(ShipIcons.file_key(1, "Galaxy") == "fed_galaxy" and ShipIcons.file_key(2, "K'T'Inga") == "kli_ktinga", "icon file names")
	check(ShipIcons.file_key(0, "", "ship.fed.galaxy") == "fed_galaxy", "icon file name from an asset key")
	OS.set_environment("BOTF_SHIP_ICONS", "none")
	ShipIcons.reset()
	check(ShipIcons.texture(1, "Galaxy") == null and not ShipIcons.has_any(), "BOTF_SHIP_ICONS=none hides every baked icon")
	var shape := ShipGlyph.shape("saucer")
	check(shape["polys"].size() >= 4, "saucer silhouette has saucer, neck, hull and nacelles")
	# The Starfleet and LCARS icon set: every named icon draws at 14, 24 and 48 px in every interface look.
	var keep_era := UiTokens.era
	var ic := IconCanvas.new()
	ic.size = Vector2(200, 200)
	root.add_child(ic)
	await process_frame
	await process_frame
	UiTokens.era = keep_era
	check(UiIcon.NAMES.size() >= 94, "the icon set has %d icons" % UiIcon.NAMES.size())
	var seen := {}
	var dup := PackedStringArray()
	for n in UiIcon.NAMES:
		if seen.has(n):
			dup.append(n)
		seen[n] = true
	check(dup.is_empty(), "no icon name is listed twice %s" % str(dup))
	var bad := PackedStringArray()
	for k in Lcars.TEXT_GLYPHS:
		if not UiIcon.NAMES.has(Lcars.TEXT_GLYPHS[k]):
			bad.append(str(k))
	check(bad.is_empty(), "every text mark that stood for an icon maps to a named glyph %s" % str(bad))
	var named := PackedStringArray()
	for sc in ShellVM.SCREEN_ICONS.values():
		if not UiIcon.NAMES.has(sc):
			named.append(sc)
	for l in GalaxyVM.LAYERS:
		if not UiIcon.NAMES.has(l[3]):
			named.append(l[3])
	check(named.is_empty(), "rail and layer icons are in the set %s" % str(named))
	check(UiIcon.minor_name("Warlike") == "minor_warlike" and UiIcon.minor_name("") == "minor" and UiIcon.minor_name("Odd") == "minor", "minor race marks by temperament")
	check(ChoiceIcon.glyph_for("Fusion plant") == "energy" and ChoiceIcon.glyph_for("Laboratory") == "research" and ChoiceIcon.glyph_for("Farm") == "food" and ChoiceIcon.glyph_for("Shipyard") == "ship" and ChoiceIcon.glyph_for("Whatsit") == "build", "structure glyphs by name")
	check(Lcars.meter(3, 10, Color.RED, 8) is ProgressBar and Lcars.meter(3, 10).get_class() == "ProgressBar", "the LCARS meter is still a ProgressBar")
	var gb := Lcars.icon_button("▾", "x", "", func(): pass)
	check(gb is Lcars.GlyphButton and (gb as Lcars.GlyphButton).glyph == "delta_down", "a text mark becomes a glyph button")
	check(Lcars.icon_button("Go", "x", "", func(): pass) is Button and not (Lcars.icon_button("Go", "x", "", func(): pass) is Lcars.GlyphButton), "plain text buttons stay plain")
	var th := Lcars.build_theme(1)
	check(th.has_icon("checked", "CheckButton") and th.has_icon("arrow", "OptionButton") and th.has_icon("grabber", "HSlider") and th.has_icon("updown", "SpinBox"), "the theme carries LCARS switch, dropdown, slider and spin icons")
	check(Lcars.delta_texture(Color.WHITE, 14, PI).get_width() == 14 and Lcars.switch_texture(Color.WHITE, true).get_width() == 46, "theme icon textures rasterise")
	check(UiTokens.icon_stroke() > 0.0 and float(UiTokens.ERAS["tos"]["stroke"]) > float(UiTokens.ERAS["movie"]["stroke"]), "TOS strokes are heavier than the movie strokes")
	print("trek icons: ", "FAILED %d" % failures if failures > 0 else "all passed")
	quit(1 if failures > 0 else 0)
